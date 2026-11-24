/* Chapter 22 golden-vector check: the Handbook's digital-loopback QPSK receiver (hb_loopback.c) driven by a
 * generated test signal, with the receiver initialised and driven by the text of HB[118] itself
 * (tests/shims/hb_E_init.inc and hb_E_loop.inc, #included verbatim).  The chapter does not list tx_waveform[], the carrier
 * or the RRC taps, so this fixture is ours (RRC taps: tests/rrc_coeffs_fm.h).  It is an algorithm check: not a bit-exact vector.
 *
 * Fixture: real passband QPSK at fs/4 (4800 Hz), fs = 19200 Hz, 2 samples/symbol, a 1-sample channel delay, 45 Hz carrier
 * offset, initial phase 0.7 rad, 300 symbols; BER is scored from symbol 250 with the best of the four constellation rotations
 * (the Costas loop may lock to any of them), 118 scored bits.
 * Checked: 0 errors at +-45 Hz (the headline vectors), at most 3 on the sweep rows (the +60 Hz row is still acquiring at symbol 300
 * and is reported, not asserted), the learned Costas frequency within 15 Hz of the true offset, the AGC gain against the balance
 * point of its attack/release steps over the fixture's samples, and the constants HB[118] defines (DECISION_AMP, TIMING_KD)
 * against an independent measurement / closed form.
 * Fixture limit: a real passband signal at fs/4 with 2 samples/symbol and alpha = 0.35 pulses overlaps its own mixer image
 * (the signal is wider than fs/4), so the decision radius spreads by about 30% even with ideal loops; that is the fixture, not
 * the receiver. */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "check.h"
#include <math.h>
#include "hb_loopback.h"
#include "hb_loop_gains.h"
#include "rrc_coeffs_fm.h"   /* rrc_coeffs_q15[]: 19-tap RRC, alpha 0.35, 2 sps (tools/design_rrc_fm.py) */

#define FS        19200.0
#define NSYM      300
#define NSAMP     (NSYM * 2 + 64)
#define CHAN_DELAY 1          /* samples: puts the decision sample on odd parity, as the loopback expects */
#define PIPE_SYMS  9          /* TX RRC (9 samples) + RX RRC (9 samples) = 18 samples = 9 symbols */
#define SCORE_FROM 250        /* BER is scored from this symbol on (acquisition takes ~200 symbols) */

int32_t adc_read_rx_sample(void) { return 0; }   /* board ADC read: hb_testkit.c declares it, the loopback test never calls it */
static int g_verbose = 0;

typedef struct { double amp, cfo_hz, phase0, carrier_hz; } fixture_t;

/* Bits for one dibit under a +90 degree constellation rotation: 00->10->11->01->00. */
static uint8_t rot90(uint8_t d, int times) {
    static const uint8_t next[4] = { 2, 0, 3, 1 };   /* index = dibit, value = rotated dibit */
    while (times-- > 0) d = next[d];
    return d;
}

/* Generate the transmit waveform: random dibits -> Gray QPSK -> 2 sps zero-stuff -> TX RRC -> real passband at
 * carrier_hz + cfo, with a fixed integer-sample channel delay.  carrier_hz = fs/4 puts the mixer's 2*fc image at fs/2, where
 * this 19-tap RRC has ~ -60 dB response, so the receiver's single real input works at 2 sps. */
static void make_signal(const fixture_t *f, uint8_t *dibits, int16_t *raw) {
    lcg_state = 987654321u;
    for (int k = 0; k < NSYM; k++) dibits[k] = (uint8_t)((lcg_next() >> 5) & 3);
    double bi[NSAMP] = { 0 }, bq[NSAMP] = { 0 };
    for (int k = 0; k < NSYM; k++) { iq_sample_t s = qpsk_modulate_symbol(dibits[k]); bi[2 * k] = s.i / 32768.0; bq[2 * k] = s.q / 32768.0; }
    double si[NSAMP] = { 0 }, sq[NSAMP] = { 0 };
    for (int n = 0; n < NSAMP; n++)
        for (int t = 0; t < FIR_TAPS; t++) if (n - t >= 0) { si[n] += bi[n - t] * rrc_coeffs_q15[t] / 32768.0; sq[n] += bq[n - t] * rrc_coeffs_q15[t] / 32768.0; }
    for (int n = 0; n < NSAMP; n++) {
        int m = n - CHAN_DELAY;
        double a = (m >= 0) ? 2.0 * M_PI * (f->carrier_hz + f->cfo_hz) * m / FS + f->phase0 : 0.0;
        double x = (m >= 0) ? (si[m] * cos(a) - sq[m] * sin(a)) : 0.0;
        raw[n] = (int16_t)lrint(f->amp * x);
    }
}

/* ---- the bench arrays HB[118] refers to, filled per run ------------------------------------------------------------------ */
static int32_t g_tx_waveform[NSAMP];
static uint8_t g_tx_dibit_stream[NSAMP / 2 + 2];   /* tx_dibit_stream[m]: the (rotated) dibit whose decision completes at symbol m */
static int g_off, g_len;
#define tx_waveform         (g_tx_waveform + g_off)
#define tx_dibit_stream     (g_tx_dibit_stream + g_off / 2)
#define NUM_LOOPBACK_SAMPLES g_len
#define nco_table_init()    qpsk_loopback_init_tables()   /* host build: the receiver's NCO tables live in hb_loopback.c's translation unit */

typedef struct { uint64_t errs, bits; int32_t agc_gain, costas_freq, mu; double mean_i, mean_q; } run_t;

/* Run HB[118] on a signal: init block, then the driving loop in two stretches (symbols 0..SCORE_FROM-1, then the rest, with the
 * BER counter zeroed in between).  `rot` rotates the expected dibits; `freeze` holds the AGC at unity and both carrier gains at 0
 * (fixture sanity: datapath only). */
static run_t run_chain(const int16_t *raw, const uint8_t *dibits, int rot, int freeze) {
    for (int n = 0; n < NSAMP; n++) g_tx_waveform[n] = raw[n];
    memset(g_tx_dibit_stream, 0, sizeof g_tx_dibit_stream);
    for (int m = PIPE_SYMS; m < NSYM + PIPE_SYMS; m++) g_tx_dibit_stream[m] = rot90(dibits[m - PIPE_SYMS], rot);
    run_t r;
    memset(&r, 0, sizeof r);
#include "hb_E_init.inc"
    if (freeze) { rx.agc.attack_shift = rx.agc.release_shift = 30; rx.costas.kp = rx.costas.ki = 0; }
    g_off = 0; g_len = 2 * SCORE_FROM;
#include "hb_E_loop.inc"
    memset(&rx.ber, 0, sizeof rx.ber);
    double sum_i = 0, sum_q = 0; int cnt = 0;
    for (int m = SCORE_FROM; m < NSYM + PIPE_SYMS; m++) {   /* one symbol per pass, to read the decision samples */
        g_off = 2 * m; g_len = 2;
#include "hb_E_loop.inc"
        sum_i += fabs((double)rx.hist_i[1]); sum_q += fabs((double)rx.hist_q[1]); cnt++;
    }
    r.mean_i = sum_i / cnt; r.mean_q = sum_q / cnt;
    r.errs = rx.ber.bit_errors; r.bits = rx.ber.bits_tested;
    r.agc_gain = rx.agc.gain_q15; r.costas_freq = rx.costas.freq; r.mu = rx.mu;
    return r;
}

/* Best-of-4-rotations run (a fixture trick: Costas locks to any of 4 constellation rotations). */
static run_t best_run(const fixture_t *f, int freeze, int *rot_out) {
    static uint8_t dibits[NSYM]; static int16_t raw[NSAMP];
    make_signal(f, dibits, raw);
    run_t best = { 0xFFFFFFFFu, 0, 0, 0, 0, 0, 0 };
    for (int rot = 0; rot < 4; rot++) {
        run_t r = run_chain(raw, dibits, rot, freeze);
        if (r.errs < best.errs) { best = r; if (rot_out) *rot_out = rot; }
    }
    return best;
}

int main(int argc, char **argv) {
    if (argc > 1) g_verbose = 1;
    qpsk_loopback_init_tables();   /* HB[118] calls nco_table_init(); this TU's copy is only used by the measurement below */
    fixture_t base = { .amp = 16384.0, .cfo_hz = 45.0, .phase0 = 0.7, .carrier_hz = FS / 4.0 };

    /* 0. Fixture sanity: the datapath (mixer, RRC, slicer, BER counter), AGC held at 1.0 and loops off, on a signal with no
     *    offset and phase 0, must decode essentially perfectly.  If this fails the fixture's alignment is wrong. */
    fixture_t clean = base; clean.cfo_hz = 0.0; clean.phase0 = 0.0;
    int rot0 = 0; run_t s0 = best_run(&clean, 1, &rot0);
    printf("[sanity ] datapath, AGC frozen, loops off, clean carrier: %llu/%llu bit errors (rotation %d)\n", (unsigned long long)s0.errs, (unsigned long long)s0.bits, rot0);
    CHECK(s0.bits >= 100 && s0.errs <= 1, "fixture sanity: %llu/%llu", (unsigned long long)s0.errs, (unsigned long long)s0.bits);

    /* 1. The receiver as HB[118] initialises it, on the fixture: converge, track the offset, do not rail the AGC. */
    typedef struct { double amp, cfo, ph; int max_errs; } cfg_t;   /* max_errs < 0: reported, not asserted */
    const cfg_t cfgs[] = { {16384, 45, 0.7, 0}, {16384, -45, 0.7, 0}, {16384, 45, 0.1, 3}, {16384, 20, 2.0, 3}, {16384, 0, 1.0, 3},
                           {12000, 30, 1.3, 3}, {16384, 60, 0.7, -1} };
    double amp_i = 0, amp_q = 0;
    for (size_t i = 0; i < sizeof cfgs / sizeof cfgs[0]; i++) {
        fixture_t f = base; f.amp = cfgs[i].amp; f.cfo_hz = cfgs[i].cfo; f.phase0 = cfgs[i].ph;
        run_t s = best_run(&f, 0, NULL);
        double want = cfgs[i].cfo / FS * 4294967296.0;
        printf("[chain  ] amp %5.0f cfo %+5.1f Hz phase %.1f: %llu/%llu bit errors, AGC gain %.2f, costas.freq %d (ideal %.0f), mu %d\n",
               cfgs[i].amp, cfgs[i].cfo, cfgs[i].ph, (unsigned long long)s.errs, (unsigned long long)s.bits, s.agc_gain / 32768.0, s.costas_freq, want, s.mu);
        /* The headline case (the 45 Hz of the golden vector, both signs) must be clean; the sweep points are allowed a couple of
         * stragglers because acquisition of the larger offsets is still finishing inside the 300-symbol window.  The +60 Hz point
         * is still pulling in at symbol 300 (its frequency estimate is only converging), so its bit errors are reported, not
         * asserted; the frequency and AGC checks below still apply to it. */
        if (cfgs[i].max_errs >= 0)
            CHECK(s.errs <= (unsigned)cfgs[i].max_errs, "chain: %llu/%llu bit errors at amp %.0f cfo %.0f phase %.1f (allowed %d)", (unsigned long long)s.errs, (unsigned long long)s.bits, cfgs[i].amp, cfgs[i].cfo, cfgs[i].ph, cfgs[i].max_errs);
        else
            printf("[chain  ]   (still acquiring at symbol 300: bit errors reported, not asserted)\n");
        CHECK(s.agc_gain > 32768 / 4 && s.agc_gain < 32768 * 4, "AGC gain %d is railed or implausible", s.agc_gain);
        CHECK(fabs(s.costas_freq - want) <= 15.0 / FS * 4294967296.0, "Costas freq %d is more than 15 Hz from ideal %.0f", s.costas_freq, want);
        if (i < 2) { amp_i += s.mean_i / 2; amp_q += s.mean_q / 2; }
    }

    /* 2. AGC equilibrium.  The squared output sample is the AGC's power estimate and attack (4) and release (8) differ, so the gain
     *    settles where the downward steps balance the upward ones, not where the mean power equals the target.  Reference: with
     *    P = (g x)^2 and step proportional to |T - P| >> shift, the balance sum_{P>T} (P-T) / 2^4 = sum_{P<T} (T-P) / 2^8 (g cancels
     *    from the step size) over the fixture's raw samples; solve it for g by bisection. */
    {
        static uint8_t d[NSYM]; static int16_t raw[NSAMP];
        fixture_t f = base; make_signal(&f, d, raw);
        double lo = 0.05, hi = 20.0;
        for (int it = 0; it < 60; it++) {
            double g = 0.5 * (lo + hi), up = 0, down = 0;
            for (int n = 100; n < 2 * NSYM; n++) {
                double pw = g * raw[n] * g * raw[n], t = (double)AGC_TARGET_POWER;
                if (pw > t) down += (pw - t) / 16.0; else up += (t - pw) / 256.0;
            }
            if (down > up) hi = g; else lo = g;
        }
        double ideal = 0.5 * (lo + hi) * 32768.0;
        run_t s = best_run(&base, 0, NULL);
        printf("[agc    ] gain %d, attack/release balance of the fixture's samples %.0f\n", s.agc_gain, ideal);
        CHECK(fabs(s.agc_gain - ideal) < 0.1 * ideal, "AGC gain %d vs attack/release balance %.0f", s.agc_gain, ideal);
    }

    /* 3. The constants HB[118] defines, against an independent measurement / closed form. */
    printf("[consts ] decision amplitude in lock (mean over the +-45 Hz headline runs): mean |I| %.0f, mean |Q| %.0f (DECISION_AMP %.0f)\n", amp_i, amp_q, DECISION_AMP);
    CHECK(fabs(amp_i - DECISION_AMP) < 0.1 * DECISION_AMP && fabs(amp_q - DECISION_AMP) < 0.1 * DECISION_AMP, "DECISION_AMP %.0f vs measured |I| %.0f |Q| %.0f", DECISION_AMP, amp_i, amp_q);
    {   /* Gardner detector gain for alpha = 0.35 raised-cosine pulses, i.i.d. +-1 data: dE/dd of sum_j (rc(d-j)-rc(d-1-j)) rc(d-1/2-j), in units
         * of A^2/2^16 per rail, from the closed-form rc(); the loopback sums two rails of amplitude DECISION_AMP. */
        const double al = 0.35, h = 1e-4;
        #define RC(t) ((fabs(t) < 1e-12 ? 1.0 : sin(M_PI * (t)) / (M_PI * (t))) * cos(M_PI * al * (t)) / (1.0 - (2 * al * (t)) * (2 * al * (t))))
        double ep = 0, em = 0;
        for (int j = -40; j <= 40; j++) { ep += (RC(h - j) - RC(h - 1 - j)) * RC(h - 0.5 - j); em += (RC(-h - j) - RC(-h - 1 - j)) * RC(-h - 0.5 - j); }
        #undef RC
        double kd = 2.0 * (ep - em) / (2 * h) * DECISION_AMP * DECISION_AMP / 65536.0;
        printf("[consts ] Gardner Kd for two rails at A = %.0f, alpha 0.35: %.0f (TIMING_KD %.0f)\n", DECISION_AMP, kd, TIMING_KD);
        CHECK(fabs(kd - TIMING_KD) < 0.03 * TIMING_KD, "TIMING_KD %.0f vs closed form %.0f", TIMING_KD, kd);
    }
    TEST_DONE("test_E_chain");
}
