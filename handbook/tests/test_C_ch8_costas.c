/* Chapter 8: QPSK Costas loop (HB[74]) wired to the repo's real NCO / quadrature mixer, closed loop.
 *
 * Fixture (independent of the code under test): passband QPSK, constellation at +-45/135 degrees (the points sign(I)*Q - sign(Q)*I
 * is zero on), rectangular symbols of 8 samples, carrier fs/4 + offset, amplitude 16384, carrier phase phase0.  The receiver is
 * this test's harness around the listing's pieces: per sample quadrature_mix() + mixer_to_q15(), integrate-and-dump over the 8
 * samples of a symbol (an exact lowpass at fs/4: it nulls the 2*fc image), then costas_phase_error(), costas_update(),
 * costas_apply_correction() ONCE PER SYMBOL, as the prose says.
 *
 * Quantities: rms phase error of the dump (I,Q) to the nearest constellation point over the last 200 symbols (rotation
 * invariant: a Costas loop may lock to any of the 4 rotations), the final freq state against the ideal phase-word
 * offset (delta/8 * 2^32 per sample), and the symbol after which the freq state stayed within 5% of ideal.
 *
 * Units: costas_phase_error() returns raw sample units, ~2*amp*sin(theta), and costas_update() adds its result to values the NCO
 * reads as 2^32-per-circle phase words, so the Q16 gains are COSTAS_KP_Q16 / COSTAS_KI_Q16 of the dimensionless design gains
 * (Kp = 0.05192, Ki = 0.001385 per symbol): about 2*10^8 and 7*10^5 at 8 samples/symbol, with 64-bit rounded products in costas_update(). */
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "hb_costas.h"
#include "hb_loop_gains.h"

#define SPS 8
#define AMP 16384.0

typedef struct { double rms_deg; int32_t freq; int settle5; } cres_t;

static cres_t run_costas(double delta, double phase0, int32_t kp, int32_t ki, int nsym, int frozen) {
    nco_table_init();
    nco_t nco; nco.phase_acc = 0; nco.base_inc = nco.phase_inc = nco_phase_inc_for_freq(0.25f, 1.0f);
    costas_loop_t c = { 0, 0, kp, ki };
    lcg_state = 987654321u;
    const double ideal = delta / SPS * 4294967296.0;
    cres_t r = { 0, 0, 0 }; double acc = 0; int cnt = 0; long n = 0;
    for (int k = 0; k < nsym; k++) {
        double Ik = ((lcg_next() >> 5) & 1) ? 1 : -1, Qk = ((lcg_next() >> 5) & 1) ? 1 : -1;
        Ik /= sqrt(2.0); Qk /= sqrt(2.0);
        int32_t ia = 0, qa = 0;
        for (int s = 0; s < SPS; s++, n++) {
            double phi = 2 * M_PI * (0.25 + delta / SPS) * n + phase0;
            int16_t x = (int16_t)lrint(AMP * (Ik * cos(phi) - Qk * sin(phi)));
            int32_t i30, q30; quadrature_mix(&nco, x, &i30, &q30);
            ia += mixer_to_q15(i30); qa += mixer_to_q15(q30);
        }
        int32_t I = ia / SPS, Q = qa / SPS;
        if (!frozen) { costas_update(&c, costas_phase_error(I, Q)); costas_apply_correction(&c, &nco); }
        if (k >= nsym - 200) {
            double ang = atan2((double)Q, (double)I) * 180.0 / M_PI - 45.0;
            double e = remainder(ang, 90.0); acc += e * e; cnt++;
        }
        if (fabs(c.freq - ideal) > 0.05 * fabs(ideal) + 1.0) r.settle5 = k + 1;
    }
    r.rms_deg = sqrt(acc / cnt); r.freq = c.freq;
    return r;
}

int main(void) {
    /* ---- detector: costas_phase_error() against the closed form, and its S-curve ------------------------------------ */
    lcg_state = 5u;
    for (int i = 0; i < 20000; i++) {
        int32_t I = (int32_t)(lcg_next() % 60001) - 30000, Q = (int32_t)(lcg_next() % 60001) - 30000;
        double want = ((I >= 0) ? 1.0 : -1.0) * Q - ((Q >= 0) ? 1.0 : -1.0) * I;
        CHECK(costas_phase_error(I, Q) == (int32_t)want, "detector(%d,%d)", I, Q);
    }
    {   /* a constellation point at (r cos(45+90m+theta), r sin(...)): e = 2a sin(theta), a = r/sqrt2, in every quadrant */
        const double a = 5800.0, r = a * sqrt(2.0); double worst = 0;
        for (int m = 0; m < 4; m++) for (int deg = -44; deg <= 44; deg += 4) {
            double th = deg * M_PI / 180.0, ang = M_PI / 4 + m * M_PI / 2 + th;
            int32_t I = (int32_t)lrint(r * cos(ang)), Q = (int32_t)lrint(r * sin(ang));
            double d = fabs(costas_phase_error(I, Q) - 2 * a * sin(th)); if (d > worst) worst = d;
        }
        CHECK(worst <= 3.0, "S-curve e = 2*a*sin(theta) in all four quadrants: worst deviation %.1f", worst);
    }

    /* ---- costas_apply_correction(): set the rate, nudge once, zero the kick ----------------------------------------- */
    {
        nco_t n = { 1000u, 0u, 0x40000000u }; costas_loop_t c = { 500, 7777, 0, 0 };
        costas_apply_correction(&c, &n);
        CHECK(n.phase_inc == 0x40000000u + 7777u, "phase_inc = base_inc + freq");
        CHECK(n.phase_acc == 1500u, "phase_acc += phase (once)");
        CHECK(c.phase == 0 && c.freq == 7777, "kick zeroed, freq kept");
        costas_apply_correction(&c, &n);
        CHECK(n.phase_inc == 0x40000000u + 7777u, "second call does not accumulate into phase_inc");
        CHECK(n.phase_acc == 1500u, "second call does not re-apply the kick");
        c.freq = -5; c.phase = -7; n.phase_acc = 3u; costas_apply_correction(&c, &n);
        CHECK(n.phase_inc == 0x40000000u - 5u && n.phase_acc == (uint32_t)(3 - 7), "negative freq/phase wrap correctly in uint32 arithmetic");
    }

    /* ---- costas_update() arithmetic vs a 64-bit round-to-nearest reference, at the real gains ------------------------------ */
    {
        const double amp_ref = AMP * 0.5 / sqrt(2.0);
        const int32_t kp = COSTAS_KP_Q16(0.05192, 2.0 * amp_ref), ki = COSTAS_KI_Q16(0.001385, 2.0 * amp_ref, SPS);
        costas_loop_t c = { 0, 0, kp, ki }; int64_t f = 0, p = 0; int ok = 1;
        for (int i = 0; i < 500; i++) {
            int32_t e = (int32_t)(lcg_next() % 23001) - 11500;
            f += ((int64_t)ki * e + 32768) >> 16; p += ((int64_t)kp * e + 32768) >> 16;
            costas_update(&c, e); if (c.freq != f || c.phase != p) ok = 0;
        }
        CHECK(ok, "costas_update vs 64-bit reference at the real gains (products far past int32)");
        CHECK((int64_t)kp * 8000 > INT32_MAX, "kp * 8000 = %lld exceeds int32: the products need 64 bits", (long long)kp * 8000);
    }
    /* rounding: zero-mean detector output must leave the integrator at zero */
    {
        costas_loop_t fx = { 0, 0, 3403, 91 };
        for (int i = 0; i < 2000; i++) { int32_t e = (i & 1) ? -7000 : 7000; costas_update(&fx, e); fx.phase = 0; }
        CHECK(abs(fx.freq) <= 1, "integrator stays at 0 on zero-mean input (got %d)", fx.freq);
    }

    /* ---- gain conversion ---------------------------------------------------------------------------------------------- */
    const double amp_dec = AMP * 0.5 / sqrt(2.0);                                  /* |I| at the decision point: mixing halves the amplitude */
    const int32_t kp_f = COSTAS_KP_Q16(0.05192, 2.0 * amp_dec), ki_f = COSTAS_KI_Q16(0.001385, 2.0 * amp_dec, SPS);
    printf("  [info] decision amplitude %.0f; Kp/Ki in this detector's units (%d samples/symbol): kp %d, ki %d\n", amp_dec, SPS, kp_f, ki_f);
    CHECK_NEAR(kp_f, 0.05192 * (4294967296.0 / (2 * M_PI)) / (2 * amp_dec) * 65536.0, 1e-7 * kp_f, "COSTAS_KP_Q16 closed form");
    CHECK_NEAR(ki_f, 0.001385 * (4294967296.0 / (2 * M_PI)) / (2 * amp_dec * SPS) * 65536.0, 1e-7 * ki_f + 1.0, "COSTAS_KI_Q16 closed form");
    CHECK_NEAR(COSTAS_KI_Q16(0.001385, 2.0 * amp_dec, SPS) * (double)SPS / COSTAS_KP_Q16(0.05192, 2.0 * amp_dec), 0.001385 / 0.05192, 1e-6, "ki/kp ratio is Ki/Kp divided by samples per symbol");

    /* ---- closed loop -------------------------------------------------------------------------------------------------- */
    /* rig sanity: frozen loop, no offset, phase 0 -> constellation sits on the 45-degree points; with an offset it rotates */
    { cres_t s = run_costas(0.0, 0.0, 0, 0, 600, 1);
      printf("  [sanity] loop frozen, clean carrier: rms phase error to nearest constellation point %.2f deg\n", s.rms_deg);
      CHECK(s.rms_deg < 2.0, "rig sanity: %.2f deg", s.rms_deg);
      cres_t t = run_costas(0.01, 0.0, 0, 0, 600, 1);
      CHECK(t.rms_deg > 15.0, "rig sanity: a frozen loop with a 1%% offset must show a rotating constellation (%.1f deg)", t.rms_deg); }

    const struct { double delta, ph; } cfg[] = { { 0.0, 0.7 }, { 0.002, 0.7 }, { -0.002, 2.0 }, { 0.01, 0.7 }, { -0.01, 1.3 }, { 0.005, 0.1 } };
    const int NC = (int)(sizeof cfg / sizeof cfg[0]);
    for (int i = 0; i < NC; i++) {
        double ideal = cfg[i].delta / SPS * 4294967296.0;
        cres_t b = run_costas(cfg[i].delta, cfg[i].ph, kp_f, ki_f, 800, 0);
        printf("  delta %+.3f cyc/sym phase %.1f: rms %.2f deg, freq %d (ideal %.0f), within 5%% from symbol %d\n",
               cfg[i].delta, cfg[i].ph, b.rms_deg, b.freq, ideal, b.settle5);
        /* locked, quantisation-limited jitter, rate recovered */
        CHECK(b.rms_deg < 3.5, "Costas (delta %+.3f phase %.1f): rms phase error %.2f deg", cfg[i].delta, cfg[i].ph, b.rms_deg);
        if (cfg[i].delta != 0.0) CHECK(fabs(b.freq - ideal) < 0.03 * fabs(ideal), "Costas freq %d vs ideal %.0f (3%%)", b.freq, ideal);
        if (fabs(cfg[i].delta) >= 0.005) CHECK(b.settle5 > 78 && b.settle5 < 250, "delta %+.3f: settling symbol %d (prose: roughly 110 to 160)", cfg[i].delta, b.settle5);
    }

    /* the 1% offset: the linear model of the design equations settles at symbol 78 (see test_C_ch9_design); the real sign-detector loop is slower */
    { cres_t b = run_costas(0.01, 0.7, kp_f, ki_f, 800, 0);
      printf("  [info] real (sign-detector) Costas loop, 1%% offset: freq within 5%% of ideal and staying there from symbol %d (linear-model prediction: 78)\n", b.settle5);
      CHECK(b.settle5 > 78 && b.settle5 < 250, "settling symbol %d", b.settle5); }

    /* Chapter 9 prose, "Pull-in range vs. pull-in time": a Bn*T = 0.02 loop pulls in offsets of about 1 to 1.5 x Bn (Bn = 768 Hz on a
     * 38.4 kbaud link, i.e. 0.02 .. 0.03 cycles/symbol).  Measured on the real (sign-detector) loop: */
    {
        const double ds[] = { 0.01, 0.02, 0.03, 0.06, 0.1, 0.2 }; double last_lock = 0, first_fail = 1e9;
        for (unsigned i = 0; i < sizeof ds / sizeof ds[0]; i++) for (int sg = -1; sg <= 1; sg += 2) {
            double d = sg * ds[i], ideal = d / SPS * 4294967296.0;
            cres_t r = run_costas(d, 0.7, kp_f, ki_f, 4000, 0);
            int locked = fabs(r.freq - ideal) < 0.05 * fabs(ideal) && r.rms_deg < 5.0;
            printf("  [pull-in] offset %+.2f cycles/symbol (%+.0f Hz at 38.4 kbaud): %s (freq/ideal %.3f, rms %.1f deg, 5%% band from symbol %d)\n",
                   d, d * 38400.0, locked ? "LOCKED" : "no lock", r.freq / ideal, r.rms_deg, r.settle5);
            if (locked && fabs(d) > last_lock) last_lock = fabs(d);
            if (!locked && fabs(d) < first_fail) first_fail = fabs(d);
        }
        printf("  [info] pull-in limit of this loop: between %.2f and %.2f cycles/symbol = %.0f..%.0f Hz at 38.4 kbaud (%.1f..%.1f x Bn)\n",
               last_lock, first_fail, last_lock * 38400.0, first_fail * 38400.0, last_lock * 38400.0 / 768.0, first_fail * 38400.0 / 768.0);
        CHECK(last_lock >= 0.02, "the loop must at least pull in +-0.02 cycles/symbol = 1.0 x Bn (got %.2f)", last_lock);
        CHECK(first_fail <= 0.03 + 1e-9 && first_fail > last_lock, "the loop fails to pull in %.3f cycles/symbol; prose: range of about 1 to 1.5 x Bn (0.02 .. 0.03)", first_fail);
        CHECK(36600.0 / 38400.0 > 0.9 && first_fail < 0.1, "the 36.6 kHz crystal offset the prose worries about is far outside the measured range (0.95 cycles/symbol vs %.2f)", first_fail);
    }
    TEST_DONE("test_C_ch8_costas");
}
