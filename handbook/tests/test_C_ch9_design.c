/* Chapter 9: loop-filter design equations and the worked example (HB[77], HB[78], HB[79]).
 * HB[77]/[78] are formulas / worked arithmetic (not code): every number in them is re-derived here.  Independent checks:
 *   - the damping and noise bandwidth that the Kp/Ki ACTUALLY give: closed-loop poles of the linear loop the prose
 *     itself describes (freq += Ki*e; vco += freq + Kp*e; e = theta_in - vco) and its impulse-response noise bandwidth;
 *   - the prose's "78 symbols": the three-line recursion the prose spells out, run in double precision;
 *   - "the answer doesn't depend on the size of the offset": the same recursion at 1%, 0.1%, 0.01%;
 *   - the pull-in arithmetic (915 MHz x 40 ppm, Bn = 0.02 * 38400);
 *   - HB[79] executed verbatim (tests/shims/hb_C_snippet_79.inc): the two worked-example gain statements, checked against an
 *     independent unit conversion (detector units -> NCO phase words). */
#include <stdint.h>
#include <complex.h>
#include "check.h"
#include "hb_loop_gains.h"

static void design(double zeta, double bnt, double *theta, double *kp, double *ki) {
    *theta = bnt / (zeta + 1.0 / (4.0 * zeta));
    double d = 1.0 + 2.0 * zeta * *theta + *theta * *theta;
    *kp = 4.0 * zeta * *theta / d; *ki = 4.0 * *theta * *theta / d;          /* Kd = 1 */
}

/* the prose's recursion: first symbol index (1-based) from which freq_est stays within 5% of delta through n = 600 */
static int settle_symbol(double kp, double ki, double delta) {
    double fe = 0, vco = 0, tru = 0; int last_bad = 0;
    for (int n = 1; n <= 600; n++) {
        tru += delta;
        double e = tru - vco;
        fe += ki * e;                 /* freq_est[n] = freq_est[n-1] + Ki*e[n]            */
        vco += fe + kp * e;           /* theta_vco[n] = theta_vco[n-1] + freq_est[n] + kick (Kp*e[n]) */
        if (fabs(fe - delta) > 0.05 * delta) last_bad = n;
    }
    return last_bad + 1;
}

int main(void) {
    /* ---- HB[78]: the worked example, digit by digit ---------------------------------------------------------------- */
    double th, kp, ki; design(0.707, 0.02, &th, &kp, &ki);
    CHECK_NEAR(0.707 + 1.0 / (4 * 0.707), 1.06061, 1e-5, "denominator zeta + 1/(4 zeta)");
    CHECK_NEAR(th, 0.01886, 5e-6, "theta_n (prose 0.01886)");
    CHECK_NEAR(kp, 0.0519, 5e-5, "Kp (prose 0.0519)");
    CHECK_NEAR(ki, 0.00138, 5e-6, "Ki (prose 0.00138)");
    CHECK_NEAR(kp, 0.05192, 5e-6, "Kp to 4 significant digits (the HB[79] statements use 0.05192)");
    CHECK_NEAR(ki, 0.001385, 5e-7, "Ki to 4 significant digits (the HB[79] statements use 0.001385)");
    /* also the intermediate expression with the rounded theta_n: Kp = (4*0.707*0.01886) / (1 + 2*0.707*0.01886 + 0.01886^2) */
    CHECK_NEAR((4 * 0.707 * 0.01886) / (1 + 2 * 0.707 * 0.01886 + 0.01886 * 0.01886), 0.0519, 5e-5, "Kp expression evaluated with the rounded theta_n");
    CHECK_NEAR((4 * 0.01886 * 0.01886) / (1 + 2 * 0.707 * 0.01886 + 0.01886 * 0.01886), 0.00138, 6e-6, "Ki expression evaluated with the rounded theta_n (0.0013854 with the rounded theta_n; 0.0013849 with the exact one: the third digit sits on a rounding boundary)");

    /* ---- HB[79]: the two worked-example statements, executed verbatim, against an independent unit conversion ------------- */
    {
#include "hb_C_snippet_79.inc"
        /* independent derivation: a phase error of theta radians gives detector output 2*A*theta (A = 7800, small angle); costas_update()
         * adds (kp_q16 * e) >> 16 to the NCO phase word, which must equal Kp*theta radians = Kp*theta*2^32/(2 pi) words. */
        const double A = 7800.0, theta = 1e-3, e = 2.0 * A * theta;
        double words_kp = (double)kp_q16 * e / 65536.0, want_kp = 0.05192 * theta * 4294967296.0 / (2.0 * M_PI);
        CHECK_NEAR(words_kp / want_kp, 1.0, 1e-6, "kp_q16 converts detector units to Kp radians of NCO phase");
        /* the integrator adds (ki_q16 * e) >> 16 to a PER-SAMPLE phase step every symbol (2 samples/symbol): over a symbol it accumulates
         * 2 x that much phase, and Ki is the per-symbol gain on the per-symbol frequency (rad/symbol) -> words/sample = rad/symbol / 2 */
        double words_ki = (double)ki_q16 * e / 65536.0, want_ki = 0.001385 * theta * 4294967296.0 / (2.0 * M_PI) / 2.0;
        CHECK_NEAR(words_ki / want_ki, 1.0, 2e-6, "ki_q16 converts detector units to Ki rad/symbol, per sample at 2 samples/symbol");
        CHECK(kp_q16 > 1.45e8 && kp_q16 < 1.55e8 && ki_q16 > 1.9e6 && ki_q16 < 2.1e6, "about 1.49e8 and 1.99e6 (got %d, %d)", kp_q16, ki_q16);
        /* the Q16 gain fits int32_t only for A above ~540: the AGC has to hold the decision amplitude up */
        CHECK(COSTAS_KP_Q16(0.05192, 2.0 * 600.0) > 0 && COSTAS_KP_Q16(0.05192, 2.0 * 600.0) < INT32_MAX, "kp_q16 fits int32_t at A = 600 (Kd = 1200)");
        CHECK(0.05192 / (2.0 * 540.0) * NCO_WORDS_PER_RAD * 65536.0 < 2147483647.0 * 1.02 && 0.05192 / (2.0 * 500.0) * NCO_WORDS_PER_RAD * 65536.0 > 2147483647.0, "kp_q16 overflows int32_t near A = 540");
        printf("  [info] kp_q16 = %d, ki_q16 = %d\n", kp_q16, ki_q16);
    }
    CHECK_NEAR(NCO_WORDS_PER_RAD, 4294967296.0 / (2.0 * M_PI), 1e-3, "NCO_WORDS_PER_RAD = 2^32 / 2pi");
    CHECK_NEAR(TIMING_GAIN_Q16(0.05192, 1000.0, 65536.0), 0.05192 / 1000.0 * 65536.0 * 65536.0, 1.0, "TIMING_GAIN_Q16 = g / Kd * mu_per_symbol * 2^16");
    CHECK(FLOAT_TO_Q16(0.5f) == 32768 && FLOAT_TO_Q16(-0.5f) == -32768 && FLOAT_TO_Q16(0.0f) == 0, "FLOAT_TO_Q16 exact values");
    CHECK(FLOAT_TO_Q16(1.0f / 65536.0f * 0.4f) == 0 && FLOAT_TO_Q16(1.0f / 65536.0f * 0.6f) == 1, "FLOAT_TO_Q16 rounds to nearest");
    CHECK(FLOAT_TO_Q16(1.5f / 65536.0f) == 2 && FLOAT_TO_Q16(2.5f / 65536.0f) == 2, "lrintf ties go to even (as in the FLOAT_TO_Q15 of Chapter 1)");

    /* ---- do the equations deliver the zeta and Bn*T they are called with?  (poles + noise bandwidth of the linear loop) ---- */
    const double zetas[] = { 0.5, 0.707, 1.0 }, bnts[] = { 0.005, 0.01, 0.02, 0.05 };
    for (unsigned zi = 0; zi < 3; zi++) for (unsigned bi = 0; bi < 4; bi++) {
        double t, p, i; design(zetas[zi], bnts[bi], &t, &p, &i);
        double b = p + i - 2.0, c = 1.0 - p;                                     /* closed loop: z^2 + (Kp+Ki-2) z + (1-Kp) = 0 */
        double complex z1 = (-b + csqrt(b * b - 4.0 * c + 0.0 * I)) / 2.0, s = clog(z1);
        double zeta_meas = -creal(s) / cabs(s);
        double fe = 0, vco = 0, sumsq = 0;
        for (int n = 1; n < 40000; n++) { double u = (n == 1) ? 1.0 : 0.0, e = u - vco; fe += i * e; vco += fe + p * e; sumsq += vco * vco; }
        double bnt_meas = sumsq / 2.0;                                              /* Bn*T = (1/2) sum h^2 for a unit-rate sampled loop */
        CHECK_NEAR(zeta_meas, zetas[zi], 0.006, "damping delivered by Kp/Ki");
        CHECK(bnt_meas > bnts[bi] && bnt_meas < bnts[bi] * (1.0 + 1.0 * bnts[bi] * 100.0 + 0.02),
              "zeta=%.3f BnT=%.3f: delivered noise bandwidth %.5f (discretisation makes it slightly wider, %.1f%%)", zetas[zi], bnts[bi], bnt_meas, 100 * (bnt_meas / bnts[bi] - 1));
        if (zi == 1 && bi == 2) printf("  [info] zeta 0.707, BnT 0.02: delivered zeta %.4f, Bn*T %.5f (+%.1f%%)\n", zeta_meas, bnt_meas, 100 * (bnt_meas / bnts[bi] - 1));
    }

    /* ---- "within 5% of the true offset in about 78 symbols" (linear model of the loop) -------------------------------------- */
    const double delta1 = 2 * M_PI * 0.01;
    int s1 = settle_symbol(0.0519, 0.00138, delta1);                       /* the rounded prose gains */
    int s2 = settle_symbol(kp, ki, delta1);                                /* the exact design gains */
    int s3 = settle_symbol(0.05192, 0.001385, delta1);                     /* the gains in the HB[79] statements */
    printf("  [info] linear recursion, 1%% offset, 5%% band: settles from symbol %d (prose gains) / %d (exact gains) / %d (0.05192, 0.001385); prose says 78\n", s1, s2, s3);
    CHECK(s1 == 78, "prose gains: %d", s1);
    CHECK(s2 == 78, "exact gains: %d", s2);
    CHECK(s3 == 78, "HB[79] source gains: %d", s3);
    CHECK(settle_symbol(0.0519, 0.00138, 2 * M_PI * 0.001) == s1 && settle_symbol(0.0519, 0.00138, 2 * M_PI * 0.0001) == s1,
          "offset independence in the linear model (0.1%% and 0.01%% land on the identical symbol)");

    /* ---- the pull-in arithmetic -------------------------------------------------------------------------------------- */
    CHECK_NEAR(915e6 * 40e-6, 36600.0, 1.0, "two 20 ppm crystals at 915 MHz: 36.6 kHz");
    CHECK_NEAR(0.02 * 38400.0, 768.0, 1e-9, "Bn = 0.02 * 38400 = 768 Hz");
    CHECK(36600.0 / 1152.0 > 30.0, "the 36.6 kHz offset is %.1fx the 1152 Hz (1.5 x Bn) upper end of the measured pull-in range", 36600.0 / 1152.0);
    TEST_DONE("test_C_ch9_design");
}
