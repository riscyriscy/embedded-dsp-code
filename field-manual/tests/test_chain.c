/* Chapter 10 golden-vector check: the digital-loopback QPSK receiver (fm_loopback.c) driven by a
 * generated test signal. The book does NOT print raw_samples[], carrier_hz or the amplitude behind
 * target_power = 1 << 28, so this fixture is ours (see the comment block in make_signal()).
 * It is an algorithm check, exactly as the book says: not a bit-exact vector. */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "check.h"
#include "fm_qformat.h"
#include "fm_loopback.h"
#include "fm_loops.h"
#include "../examples/loopback_init.c"   /* loopback_reset(), the Chapter 6 gains, rrc_coeffs_q15[] */

#define FS        19200.0
#define NSYM      300
#define NSAMP     (NSYM * 2 + 64)
#define CHAN_DELAY 1          /* samples: puts the decision sample on odd parity, as the loopback expects */
#define PIPE_SYMS  9          /* TX RRC (9 samples) + RX RRC (9 samples) = 18 samples = 9 symbols */
#ifndef SCORE_FROM
#define SCORE_FROM 250        /* BER is scored from the receiver's 250th decision to the last tx symbol: tx symbols
                                  241..299 after the 9-symbol filter delay, 59 symbols = 118 bits (acquisition takes ~200 symbols) */
#endif

static int g_verbose = 0;
#define FIXTURE_DECISION_AMP 5600.0   /* typical |I| at the decision sample on this fixture (AGC gain 1.0-1.1, loops off); sets Kd = 2a */

typedef struct { double amp, cfo_hz, phase0, carrier_hz; } fixture_t;

/* Bits for one dibit under a +90 degree constellation rotation: 00->10->11->01->00. */
static uint8_t rot90(uint8_t d, int times) {
    static const uint8_t next[4] = { 2, 0, 3, 1 };   /* index = dibit, value = rotated dibit */
    while (times-- > 0) d = next[d];
    return d;
}

/* Generate raw_samples[]: random dibits -> Gray QPSK -> 2 sps zero-stuff -> RRC (the book's own
 * rrc_coeffs_q15) -> real passband at carrier_hz + cfo, with a fixed integer-sample channel delay.
 * carrier_hz = fs/4 puts the mixer's 2*fc image at fs/2, where this 19-tap RRC has ~ -60 dB response,
 * so the receiver's single real input works at 2 sps. */
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

/* The book's initialisation block (Chapter 10) lives in examples/loopback_init.c. */
static void loopback_init(qpsk_loopback_rx_t *rx, double carrier_hz) { loopback_reset(rx, (float)carrier_hz); }

/* Run the receiver over the signal with the tx dibit stream rotated by `rot`; return post-SCORE_FROM BER bits. */
static void run_rx_with(const fixture_t *f, const int16_t *raw, const uint8_t *dibits, int rot, qpsk_loopback_rx_t *rx,
                   uint32_t *errs, uint32_t *tested, int already_init) {
    if (!already_init) loopback_init(rx, f->carrier_hz);
    for (int n = 0; n < NSAMP - 2; n++) {
        uint32_t before = rx->symbols_processed;
        int k = (int)rx->symbols_processed - PIPE_SYMS;           /* tx symbol this decision corresponds to */
        if (k >= NSYM) break;                                     /* stop scoring once the last tx symbol is through */
        uint8_t tx = (k >= 0 && k < NSYM) ? rot90(dibits[k], rot) : 0;
        qpsk_loopback_process_sample(rx, raw[n], tx);
        if (rx->symbols_processed != before && rx->symbols_processed == SCORE_FROM) rx->ber = (ber_counter_t){ 0, 0 };
        if (g_verbose && rx->symbols_processed != before && (rx->symbols_processed % 20) == 0)
            printf("sym %u gain %d costas.freq %d mu %d\n", rx->symbols_processed, rx->agc.gain_q15, rx->costas.freq, rx->mu);
    }
    *errs = rx->ber.bit_errors; *tested = rx->ber.bits_tested;
}

/* Best-of-4-rotations score (a fixture trick: Costas locks to any of 4 constellation rotations). */
typedef struct { uint32_t errs, bits; int rot; int32_t agc_gain, costas_freq; } score_t;
static score_t score(const fixture_t *f, int freeze_agc_and_loops) {
    static uint8_t dibits[NSYM]; static int16_t raw[NSAMP];
    make_signal(f, dibits, raw);
    score_t best = { 0xFFFFFFFFu, 0, -1, 0, 0 };
    for (int rot = 0; rot < 4; rot++) {
        qpsk_loopback_rx_t rx; uint32_t e, t;
        if (freeze_agc_and_loops) {   /* fixture sanity: AGC held at unity and both loops off */
            loopback_init(&rx, f->carrier_hz);
            rx.agc.attack_shift = rx.agc.release_shift = 30; rx.costas.kp = rx.costas.ki = 0;
        }
        run_rx_with(f, raw, dibits, rot, &rx, &e, &t, freeze_agc_and_loops);
        if (e < best.errs) best = (score_t){ e, t, rot, rx.agc.gain_q15, rx.costas.freq };
    }
    return best;
}

int main(int argc, char **argv) {
    if (argc > 1) g_verbose = 1;
    /* The Q16 gains the book quotes, against the closed form kp * 2^32/(2 pi) / Kd * 2^16 (and / sps for ki). */
    const double kd = 2.0 * FIXTURE_DECISION_AMP, w = 4294967296.0 / (2.0 * M_PI);
    CHECK(costas.kp == 207671349 && costas.ki == 2769885, "Chapter 6 gains: kp %d ki %d", costas.kp, costas.ki);
    CHECK_NEAR(costas.kp, 0.05192 * w / kd * 65536.0, 0.5, "kp closed form");
    CHECK_NEAR(costas.ki, 0.001385 * w / (kd * 2.0) * 65536.0, 0.5, "ki closed form");
    fixture_t base = { .amp = 16384.0, .cfo_hz = 45.0, .phase0 = 0.7, .carrier_hz = FS / 4.0 };

    /* 0. Fixture sanity: the datapath (mixer, RRC, slicer, BER counter), AGC held at 1.0 and loops off, on a
     *    signal with no offset and phase 0, must decode essentially perfectly. If this fails the fixture's
     *    alignment is wrong, and nothing below means anything. */
    fixture_t clean = base; clean.cfo_hz = 0.0; clean.phase0 = 0.0;
    score_t s0 = score(&clean, 1);
    printf("[sanity ] AGC frozen, loops off, clean carrier: %u/%u bit errors (rotation %d)\n", s0.errs, s0.bits, s0.rot);
    CHECK(s0.bits >= 100 && s0.errs <= 1, "fixture sanity: %u/%u", s0.errs, s0.bits);

    /* 1. The algorithm check: 300 random QPSK symbols, 2 samples/symbol, RRC, +45 Hz carrier offset, Gardner-only
     *    for the first 40 symbols then Costas on with the worked-example gains. The AGC settles, the Costas loop
     *    pulls in the offset, and the decoded stream matches the TX pattern (scored from symbol 250). */
    typedef struct { double amp, cfo, ph; } cfg_t;
    const cfg_t cfgs[] = { {16384, 45, 0.7}, {16384, -45, 0.7}, {16384, 45, 0.1}, {16384, 20, 2.0}, {16384, 0, 1.0}, {12000, 30, 1.3}, {16384, 60, 0.7} };
    for (size_t i = 0; i < sizeof cfgs / sizeof cfgs[0]; i++) {
        fixture_t f = base; f.amp = cfgs[i].amp; f.cfo_hz = cfgs[i].cfo; f.phase0 = cfgs[i].ph;
        score_t s = score(&f, 0);
        double want = cfgs[i].cfo / FS * 4294967296.0;
        printf("[chain  ] amp %5.0f cfo %+5.1f Hz phase %.1f: %u/%u bit errors, AGC gain %.2f, costas.freq %d (ideal %.0f)\n",
               cfgs[i].amp, cfgs[i].cfo, cfgs[i].ph, s.errs, s.bits, s.agc_gain / 32768.0, s.costas_freq, want);
        /* The headline case (the book's 45 Hz, both signs) must be clean; the sweep points are allowed a couple of
         * stragglers because acquisition of the larger offsets is still finishing inside the 300-symbol window. */
        unsigned allowed = (fabs(cfgs[i].cfo) == 45.0 && cfgs[i].ph == 0.7) ? 0u : 3u;
        CHECK(s.errs <= allowed, "%u/%u bit errors at amp %.0f cfo %.0f phase %.1f (allowed %u)", s.errs, s.bits, cfgs[i].amp, cfgs[i].cfo, cfgs[i].ph, allowed);
        CHECK(s.agc_gain > 32768 / 4 && s.agc_gain < 32768 * 4, "AGC gain %d is railed or implausible", s.agc_gain);
        CHECK(fabs(s.costas_freq - want) <= 15.0 / FS * 4294967296.0, "Costas freq %d is more than 15 Hz from ideal %.0f", s.costas_freq, want);
    }
    TEST_DONE("test_chain");
}
