/* HB[53]: 31-tap Hilbert FIR I/Q generator, hilbert_iq_process() (src/hb_hilbert.c, coefficients from include/generated/hilbert_coeffs.h,
 * which tools/design_hilbert.py writes).
 * References: the closed-form analytic signal of a tone (e^{j w (n-15)}), an FFT-method analytic signal of a multi-tone signal
 * (zero the negative-frequency half of a double-precision DFT), and a direct 31-tap convolution over ALL taps. */
#include <stdint.h>
#include <complex.h>
#include "check.h"
#include "hb_hilbert.h"

/* Q-path frequency response Hq(f) = -sum h[k] e^{-j 2 pi f k}; ideal analytic-signal Q path = -j e^{-j 2 pi f 15} for f in (0, 0.5) */
static double complex Hq(double f) { double complex s = 0; for (int k = 0; k < HILBERT_TAPS; k++) s += hilbert_coeffs[k] * cexp(-2.0 * M_PI * I * f * k); return -s; }
static double complex ideal(double f) { return -I * cexp(-2.0 * M_PI * I * f * HILBERT_DELAY); }

int main(void) {
    /* ---- the structural claims in the prose ---- */
    CHECK(HILBERT_TAPS == 31 && HILBERT_DELAY == 15, "31 taps, group delay 15");
    for (int i = 0; i < HILBERT_TAPS; i++) {
        if (i % 2 == 1) CHECK(hilbert_coeffs[i] == 0.0f, "odd-indexed tap h[%d] is exactly 0 (got %g)", i, hilbert_coeffs[i]);
        else CHECK(hilbert_coeffs[i] != 0.0f, "even-indexed tap h[%d] is nonzero", i);
        CHECK(hilbert_coeffs[i] == -hilbert_coeffs[HILBERT_TAPS - 1 - i], "antisymmetric: h[%d] == -h[%d]", i, HILBERT_TAPS - 1 - i);
    }
    CHECK(hilbert_coeffs[15] == 0.0f, "centre tap h[15] is exactly 0");

    /* ---- frequency response of the generated design: constant magnitude, -90 degree phase (after removing the 15-sample delay) ---- */
    double worst_mag = 0, worst_ph = 0;
    for (double f = 0.05; f <= 0.45 + 1e-12; f += 0.0025) {
        double complex r = Hq(f) / ideal(f);       /* should be ~1 */
        double dm = fabs(cabs(r) - 1.0), dp = fabs(carg(r));
        if (dm > worst_mag) worst_mag = dm;
        if (dp > worst_ph) worst_ph = dp;
    }
    printf("  [info] Hilbert design 0.05..0.45 fs: worst magnitude error %.4f (%.3f dB), worst phase error %.4f rad (%.2f deg)\n", worst_mag, 20 * log10(1 + worst_mag), worst_ph, worst_ph * 180 / M_PI);
    CHECK(worst_mag < 0.004, "magnitude flat to 0.4%% over 0.05..0.45 fs (got %g)", worst_mag);
    CHECK(worst_ph < 1e-6, "phase is -90 deg to numerical precision (type III antisymmetric FIR: exact), got %g rad", worst_ph);
    CHECK(cabs(Hq(0.0)) < 1e-12 && cabs(Hq(0.5)) < 1e-6, "zero response at DC and Nyquist (|H(0)| = %g, |H(0.5)| = %g): Hilbert FIRs cannot pass either", cabs(Hq(0.0)), cabs(Hq(0.5)));

    /* ---- I path: pure delay of exactly 15 samples from the first call; Q path: direct convolution over ALL 31 taps from the first call ---- */
    { enum { N = 200 }; static float x[N]; hilbert_iq_t h = { {0}, 0 }; int ok_i = 1, ok_q = 1;
      for (int n = 0; n < N; n++) x[n] = (float)(((int)(lcg_next() & 0xFFFF) - 32768) / 32768.0);
      for (int n = 0; n < N; n++) {
          float i_out, q_out; hilbert_iq_process(&h, x[n], &i_out, &q_out);
          float wi = (n >= HILBERT_DELAY) ? x[n - HILBERT_DELAY] : 0.0f;
          if (i_out != wi) ok_i = 0;
          double q = 0; for (int k = 0; k < HILBERT_TAPS; k++) if (n - k >= 0) q += (double)hilbert_coeffs[k] * x[n - k];
          if (fabs(q_out - (-q)) > 1e-6) { ok_q = 0; printf("  n=%d q %g want %g\n", n, q_out, -q); }
      }
      CHECK(ok_i, "I output == x delayed by exactly 15 samples (0 during the first 15), bit-exact");
      CHECK(ok_q, "Q output == -(full 31-tap convolution), including the first 31 calls (zero history)"); }

    /* ---- tone: analytic signal e^{j w (n-15)} (closed form), image rejection, sign convention ---- */
    printf("  [info] tone    freq  |  image rejection measured / predicted  | worst |z - analytic|\n");
    for (int bin = 8; bin <= 56; bin += 8) {      /* N = 128 points per period: f = bin/128 = 0.0625 .. 0.4375 */
        enum { P = 128 }; const double f = bin / (double)P, w = 2.0 * M_PI * f;
        hilbert_iq_t h = { {0}, 0 }; static double zi[3 * P], zq[3 * P];
        for (int n = 0; n < 3 * P; n++) { float i_out, q_out; hilbert_iq_process(&h, (float)cos(w * n), &i_out, &q_out); zi[n] = i_out; zq[n] = q_out; }
        double worst = 0;
        for (int n = 2 * P; n < 3 * P; n++) { double d = hypot(zi[n] - cos(w * (n - HILBERT_DELAY)), zq[n] - sin(w * (n - HILBERT_DELAY))); if (d > worst) worst = d; }
        /* sign convention: instantaneous frequency of z = I + jQ must be +f */
        double dphi = 0; for (int n = 2 * P + 1; n < 3 * P; n++) dphi += carg((zi[n] + I * zq[n]) * conj(zi[n - 1] + I * zq[n - 1]));
        CHECK_NEAR(dphi / (P - 1) / (2.0 * M_PI), f, 0.01 * f, "mean instantaneous frequency of I+jQ is +f (positive input frequency -> positive output frequency)");
        CHECK(dphi > 0, "sign: phase advances (counter-clockwise) for a positive-frequency input");
        /* image rejection: |negative-frequency line| / |positive-frequency line| of z over exactly one period */
        double complex pos = 0, neg = 0;
        for (int n = 2 * P; n < 3 * P; n++) { double complex z = zi[n] + I * zq[n]; pos += z * cexp(-I * w * n); neg += z * cexp(I * w * n); }
        double irr = 20.0 * log10(cabs(pos) / cabs(neg));
        /* closed form: z = cos + j*|Hq| sin(phase exact) -> lines (1+g)/2 and (1-g)/2, with g = |Hq(f)| (phase is exactly -90) */
        double g = cabs(Hq(f)); double pred = 20.0 * log10((1.0 + g) / fabs(1.0 - g));
        printf("  [info]  bin %2d  f=%.4f  |  %6.1f dB / %6.1f dB  |  %.2e\n", bin, f, irr, pred, worst);
        CHECK(fabs(irr - pred) < 0.5 || (irr > 60 && pred > 60), "image rejection %.1f dB matches the closed form %.1f dB from the coefficients' own |H|", irr, pred);
        CHECK(irr > 30.0, "image rejection at f=%.4f is %.1f dB (> 30 dB)", f, irr);
        CHECK(worst < 0.004, "I+jQ follows the analytic signal e^{jw(n-15)} within the design ripple: worst error %g", worst);
    }
    /* near the edges of the design band the ripple is the bound; outside it (DC, Nyquist) there is no I/Q: Q -> 0 */
    { hilbert_iq_t h = { {0}, 0 }; float i_out = 0, q_out = 0;
      for (int n = 0; n < 100; n++) hilbert_iq_process(&h, 1.0f, &i_out, &q_out);   /* DC */
      CHECK(i_out == 1.0f && fabs(q_out) < 1e-6, "DC input: I = 1, Q = 0 (the Hilbert transform of a constant is 0), got Q = %g", q_out);
      for (int n = 0; n < 100; n++) hilbert_iq_process(&h, (n & 1) ? -1.0f : 1.0f, &i_out, &q_out);   /* fs/2 */
      CHECK(fabs(q_out) < 1e-6, "Nyquist input: Q = 0, got %g", q_out); }

    /* ---- multi-tone vs an FFT-method analytic signal (double precision reference) ---- */
    { enum { P = 128 }; static double x[P]; static double complex X[P], Z[P], zref[P];
      static const int bins[] = { 7, 13, 21, 30, 41, 49, 57 };
      for (int n = 0; n < P; n++) { x[n] = 0; for (unsigned t = 0; t < sizeof bins / sizeof bins[0]; t++) x[n] += 0.12 * cos(2.0 * M_PI * bins[t] * n / P + 0.5 * t); }
      for (int k = 0; k < P; k++) { X[k] = 0; for (int n = 0; n < P; n++) X[k] += x[n] * cexp(-2.0 * M_PI * I * (double)((long long)k * n % P) / P); }
      for (int k = 0; k < P; k++) Z[k] = (k == 0 || k == P / 2) ? X[k] : (k < P / 2 ? 2.0 * X[k] : 0.0);   /* analytic signal: double the positive half, drop the negative half */
      for (int n = 0; n < P; n++) { double complex s = 0; for (int k = 0; k < P; k++) s += Z[k] * cexp(2.0 * M_PI * I * (double)((long long)k * n % P) / P); zref[n] = s / P; }
      hilbert_iq_t h = { {0}, 0 }; double worst = 0;
      for (int n = 0; n < 4 * P; n++) {
          float i_out, q_out; hilbert_iq_process(&h, (float)x[n % P], &i_out, &q_out);
          if (n >= 3 * P) { int m = (n - HILBERT_DELAY) % P; double d = hypot(i_out - creal(zref[m]), q_out - cimag(zref[m])); if (d > worst) worst = d; }
      }
      printf("  [info] 7-tone signal (peak ~0.8): worst |I+jQ - analytic(x)[n-15]| = %.4f\n", worst);
      CHECK(worst < 0.002, "multi-tone I/Q equals the FFT-method analytic signal (delayed 15) within the filter ripple: %g", worst); }
    TEST_DONE("test_B_hilbert");
}
