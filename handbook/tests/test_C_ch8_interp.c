/* Chapter 11: timing interpolators (HB[70] linear, HB[71] cubic Farrow float, HB[72] fixed-point Farrow).
 * References: exact polynomial values (4-point Lagrange is exact for cubics), true sine values, double-precision Lagrange weights.
 * timing_interpolate_farrow_cubic_q15() truncates mu to the table step at or below it (timing resolution 1/32 sample), and the
 * weight p_0(mu=0) = 1.0, which does not fit an int16_t, is stored as 32767. */
#include <stdint.h>
#include "check.h"
#include "hb_timing.h"
#include "hb_farrow.h"

static void lagrange(double mu, double w[4]) {
    w[0] = -mu * (mu - 1) * (mu - 2) / 6.0;
    w[1] = (mu + 1) * (mu - 1) * (mu - 2) / 2.0;
    w[2] = -(mu + 1) * mu * (mu - 2) / 2.0;
    w[3] = (mu + 1) * mu * (mu - 1) / 6.0;
}

int main(void) {
    /* ---- HB[70] timing_interpolate_linear ------------------------------------------------------------------------- */
    lcg_state = 99u;
    int worst = 0;
    for (int i = 0; i < 20000; i++) {
        int32_t a = (int32_t)(lcg_next() % 60001) - 30000, b = (int32_t)(lcg_next() % 60001) - 30000, mu = (int32_t)(lcg_next() % 32768);
        double want = a + (b - a) * (mu / 32768.0);
        int d = abs(timing_interpolate_linear(a, b, mu) - (int)floor(want));
        if (d > worst) worst = d;
    }
    CHECK(worst <= 1, "linear interpolator vs a + (b-a)*mu (double): worst %d LSB", worst);
    CHECK(timing_interpolate_linear(1234, -777, 0) == 1234, "mu = 0 returns sample_a");
    CHECK(abs(timing_interpolate_linear(1234, -777, 32767) - (-777)) <= 1, "mu -> 1 returns sample_b");
    CHECK(timing_interpolate_linear(-1000, 1000, 16384) == 0, "midpoint of -1000..1000");

    /* ---- HB[71] float cubic: exact for cubic polynomials ----------------------------------------------------------- */
    double worst_poly = 0;
    for (int trial = 0; trial < 50; trial++) {
        double c[4]; for (int i = 0; i < 4; i++) c[i] = ((int)(lcg_next() % 2001) - 1000) / (i == 0 ? 50.0 : (i == 1 ? 10.0 : (i == 2 ? 100.0 : 1000.0)));
        int32_t x[4];
        for (int j = 0; j < 4; j++) { double t = j - 1; x[j] = (int32_t)lrint(20.0 * (c[0] + c[1] * t + c[2] * t * t + c[3] * t * t * t)); }
        for (int mu = 0; mu < 32768; mu += 512) {
            double t = mu / 32768.0, want = 0;
            double xx[4]; for (int j = 0; j < 4; j++) xx[j] = x[j];
            /* exact cubic through the four integer samples: Newton/Lagrange in double on the SAME integers */
            double w[4]; lagrange(t, w); for (int j = 0; j < 4; j++) want += w[j] * xx[j];
            double d = fabs(timing_interpolate_farrow_cubic(x[0], x[1], x[2], x[3], mu) - want);
            if (d > worst_poly) worst_poly = d;
        }
    }
    CHECK(worst_poly < 1.0 + 1e-3, "float cubic vs double Lagrange: worst %.4f (the int32 cast truncates, so < 1 + float rounding)", worst_poly);
    /* polynomial exactness with a known closed form: x[n] = n^3 - 2n^2 + 7n - 5, sampled at n = -1,0,1,2; value at n = mu */
    {
        int32_t x[4]; for (int j = 0; j < 4; j++) { int n = j - 1; x[j] = n * n * n - 2 * n * n + 7 * n - 5; }
        for (int mu = 0; mu < 32768; mu += 4096) {
            double t = mu / 32768.0, want = t * t * t - 2 * t * t + 7 * t - 5;
            double got = timing_interpolate_farrow_cubic(x[0], x[1], x[2], x[3], mu);
            CHECK(fabs(got - want) <= 1.0, "closed-form cubic at mu=%.4f: got %.0f want %.4f", t, got, want);
        }
    }
    CHECK(timing_interpolate_farrow_cubic(-5, 1000, 2000, 9, 0) == 1000, "mu = 0 returns x_0");

    /* ---- "meaningfully better than linear": fractional delay of a sine, against the true sine -------------------------- */
    {
        const double f = 0.1, A = 20000.0;                    /* 10 samples per period */
        double e_lin = 0, e_cub = 0;
        for (int n = 4; n < 40; n++) for (int mu = 0; mu < 32768; mu += 1024) {
            int32_t s[4]; for (int j = 0; j < 4; j++) s[j] = (int32_t)lrint(A * sin(2 * M_PI * f * (n + j - 1)));
            double t = n + mu / 32768.0, truth = A * sin(2 * M_PI * f * t);
            double dl = fabs(timing_interpolate_linear(s[1], s[2], mu) - truth), dc = fabs(timing_interpolate_farrow_cubic(s[0], s[1], s[2], s[3], mu) - truth);
            if (dl > e_lin) e_lin = dl;
            if (dc > e_cub) e_cub = dc;
        }
        printf("  [info] sine at fs/10, amplitude 20000: worst interpolation error linear %.0f, cubic %.0f LSB\n", e_lin, e_cub);
        CHECK(e_cub * 8 < e_lin, "cubic (%.1f) should beat linear (%.1f) by a wide margin", e_cub, e_lin);
        CHECK(e_lin < 20000.0 * pow(2 * M_PI * f, 2) / 8.0 * 1.05 + 2, "linear error vs the textbook bound A (2 pi f)^2 / 8");
    }

    /* ---- HB[72] fixed-point Farrow ---------------------------------------------------------------------------------- */
    CHECK(FARROW_MU_STEPS == 32, "FARROW_MU_STEPS");
    int worst_w = 0, sat0 = 0;
    for (int s = 0; s < FARROW_MU_STEPS; s++) {
        double w[4]; lagrange(s / (double)FARROW_MU_STEPS, w);
        int rowsum = 0;
        for (int j = 0; j < 4; j++) {
            int want = (int)lrint(w[j] * 32768.0), got = farrow_weights_q15[s][j];
            if (want == 32768) { sat0++; want = 32767; }
            if (abs(got - want) > worst_w) worst_w = abs(got - want);
            rowsum += got;
        }
        CHECK(abs(rowsum - 32768) <= 2, "weights row %d sums to %d (want 32768 +-2: partition of unity)", s, rowsum);
    }
    CHECK(worst_w == 0, "table vs round(32768 * Lagrange), saturated: worst %d", worst_w);
    CHECK(sat0 == 1, "exactly one weight (mu=0, p_0) needs saturation, found %d", sat0);

    /* interpolated values at exactly representable mu = step/32, against the exact double Lagrange of the same samples */
    int worst_i = 0;
    for (int trial = 0; trial < 400; trial++) {
        int32_t x[4]; for (int j = 0; j < 4; j++) x[j] = (int32_t)(lcg_next() % 24001) - 12000;
        for (int s = 1; s < FARROW_MU_STEPS; s++) {
            double w[4]; lagrange(s / 32.0, w);
            double want = 0; for (int j = 0; j < 4; j++) want += w[j] * x[j];
            int d = abs(timing_interpolate_farrow_cubic_q15(x[0], x[1], x[2], x[3], s * 1024) - (int)floor(want));
            if (d > worst_i) worst_i = d;
        }
    }
    CHECK(worst_i <= 3, "fixed Farrow at mu = step/32 vs double Lagrange: worst %d LSB (4 weights x 0.5 LSB x |x| <= 12000 / 32768 + truncation)", worst_i);
    CHECK(timing_interpolate_farrow_cubic_q15(1, 2, 3, 4, 32768) == timing_interpolate_farrow_cubic_q15(1, 2, 3, 4, 31 * 1024),
          "mu_q15 = 32768 (1.0) must clamp to the last step, not read past the table");

    /* truncating quantiser: any mu inside [step, step+1)/32 maps to the same row, up to 1/32 sample away */
    int same = 1;
    for (int s = 0; s < FARROW_MU_STEPS; s++)
        if (timing_interpolate_farrow_cubic_q15(-3000, 7000, 11000, 2000, s * 1024 + 1023) != timing_interpolate_farrow_cubic_q15(-3000, 7000, 11000, 2000, s * 1024)) same = 0;
    CHECK(same, "mu_q15 = step*1024 + 1023 should give the SAME output as step*1024 (truncating quantiser)");
    {   /* the cost: worst timing error of the quantiser on a sine stays within 1/32 sample */
        const double f = 0.1, A = 20000.0; double e_floor = 0; int n = 10;
        int32_t sx[4]; for (int j = 0; j < 4; j++) sx[j] = (int32_t)lrint(A * sin(2 * M_PI * f * (n + j - 1)));
        for (int mu = 0; mu < 32768; mu += 37) {
            double truth = A * sin(2 * M_PI * f * (n + mu / 32768.0));
            double d = fabs(timing_interpolate_farrow_cubic_q15(sx[0], sx[1], sx[2], sx[3], mu) - truth);
            if (d > e_floor) e_floor = d;
        }
        const double slope_per_32 = A * 2 * M_PI * f / 32.0;              /* 1/32-sample timing error at the sine's steepest point */
        printf("  [info] sine fs/10 A=20000: worst error of the truncating quantiser %.0f LSB (1/32 sample = %.0f LSB)\n", e_floor, slope_per_32);
        CHECK(e_floor > 0.9 * slope_per_32 && e_floor < 1.1 * slope_per_32, "quantiser error %.0f should reach, and stay within, ~1/32 sample = %.0f LSB", e_floor, slope_per_32);
    }
    /* p_0(0) = 1.0 is stored as 32767: mu = 0 loses one LSB on positive samples */
    CHECK(farrow_weights_q15[0][1] == 32767, "table holds 32767 for the weight 1.0 at mu = 0");
    CHECK(timing_interpolate_farrow_cubic_q15(5, 1000, 3, 7, 0) == 999, "mu = 0, x_0 = 1000 returns %d (table weight 32767/32768)", timing_interpolate_farrow_cubic_q15(5, 1000, 3, 7, 0));
    CHECK(timing_interpolate_farrow_cubic_q15(5, -1000, 3, 7, 0) == -1000, "mu = 0, negative x_0 is exact (floor of -999.97)");
    TEST_DONE("test_C_ch8_interp");
}
