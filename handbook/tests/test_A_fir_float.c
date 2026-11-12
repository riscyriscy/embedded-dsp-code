/* GROUP A: float FIR (HB[2]) and the symmetric-coefficient version (HB[4]).
 * References: direct convolution y[n] = sum h[k] x[n-k] in double over the whole input history (no circular buffer),
 * closed forms (boxcar sidelobe, linear-phase delay), and a TEST-ONLY 33-tap build of the same listings so the
 * odd-tap middle-tap branch (dead code at FIR_TAPS = 32) is exercised. */
#include <stdint.h>
#include "check.h"
#include "hb_fir_float.h"
#include "hbA_fir_float_odd.h"

#define NSAMP 600
static double xin[NSAMP];

static void make_sym(float *h, int n, uint32_t seed) {
    lcg_state = seed;
    for (int i = 0; i < (n + 1) / 2; i++) { h[i] = h[n - 1 - i] = ((int)(lcg_next() & 0xFFFF) - 32768) / 40000.0f; }
}
static double conv_ref(const float *h, int n, const double *x, int t) {
    double acc = 0.0;
    for (int k = 0; k < n; k++) if (t - k >= 0) acc += (double)h[k] * x[t - k];
    return acc;
}

int main(void) {
    CHECK(FIR_TAPS == 32, "FIR_TAPS = %d", FIR_TAPS);
    float h[FIR_TAPS], h33[FIR_TAPS_ODD];
    make_sym(h, FIR_TAPS, 7); make_sym(h33, FIR_TAPS_ODD, 9);
    lcg_state = 99; for (int i = 0; i < NSAMP; i++) xin[i] = ((int)(lcg_next() & 0xFFFF) - 32768) / 32768.0;

    /* 1. impulse response == coefficients, then exactly zero once the impulse has left the history (also proves the circular index wraps). */
    { fir_filter_t f; fir_init(&f, h); int bad = 0;
      for (int n = 0; n < 3 * FIR_TAPS; n++) { float y = fir_process(&f, n == 0 ? 1.0f : 0.0f); float want = n < FIR_TAPS ? h[n] : 0.0f; if (y != want) bad++; }
      CHECK(bad == 0, "impulse response differs from coefficients at %d samples", bad); }

    /* 2. random input vs double-precision convolution, 600 samples = ~19 wraps of the 32-entry ring. */
    { fir_filter_t f; fir_init(&f, h); double worst = 0;
      for (int n = 0; n < NSAMP; n++) { double e = fabs((double)fir_process(&f, (float)xin[n]) - conv_ref(h, FIR_TAPS, xin, n)); if (e > worst) worst = e; }
      CHECK(worst < 2e-5, "direct FIR vs double convolution: worst error %g", worst); }

    /* 3. symmetric implementation == direct implementation == double convolution on a symmetric coefficient set (even tap count). */
    { fir_filter_t a, b; fir_init(&a, h); fir_init(&b, h); double w_sd = 0, w_sr = 0;
      for (int n = 0; n < NSAMP; n++) {
          float ya = fir_process(&a, (float)xin[n]), yb = fir_process_symmetric(&b, (float)xin[n]);
          if (fabs((double)ya - yb) > w_sd) w_sd = fabs((double)ya - yb);
          double e = fabs((double)yb - conv_ref(h, FIR_TAPS, xin, n)); if (e > w_sr) w_sr = e; }
      CHECK(w_sd < 2e-5, "symmetric vs direct: worst difference %g", w_sd);
      CHECK(w_sr < 2e-5, "symmetric vs double convolution: worst error %g", w_sr); }

    /* 4. same for an odd tap count: exercises the middle-tap branch ("add it once, unpaired"). */
    { fir_filter_t_odd a, b; fir_init_odd(&a, h33); fir_init_odd(&b, h33); double w_sd = 0, w_sr = 0, w_ar = 0;
      for (int n = 0; n < NSAMP; n++) {
          float ya = fir_process_odd(&a, (float)xin[n]), yb = fir_process_symmetric_odd(&b, (float)xin[n]);
          if (fabs((double)ya - yb) > w_sd) w_sd = fabs((double)ya - yb);
          double r = conv_ref(h33, FIR_TAPS_ODD, xin, n);
          if (fabs((double)yb - r) > w_sr) w_sr = fabs((double)yb - r);
          if (fabs((double)ya - r) > w_ar) w_ar = fabs((double)ya - r); }
      CHECK(w_ar < 2e-5, "33-tap direct FIR vs double convolution: worst error %g", w_ar);
      CHECK(w_sd < 2e-5, "33-tap symmetric vs direct: worst difference %g", w_sd);
      CHECK(w_sr < 2e-5, "33-tap symmetric vs double convolution: worst error %g", w_sr);
      CHECK(fabs(h33[FIR_TAPS_ODD / 2]) > 0.01, "test is meaningful: middle tap h[16] = %g is non-zero", h33[FIR_TAPS_ODD / 2]); }

    /* 5. "Valid only when coeffs[i] == coeffs[N-1-i]": on an asymmetric set the half-multiply form must NOT agree with the direct form. */
    { float hs[FIR_TAPS]; for (int i = 0; i < FIR_TAPS; i++) hs[i] = (float)(i + 1) / 32.0f;
      fir_filter_t a, b; fir_init(&a, hs); fir_init(&b, hs); int diff = 0;
      for (int n = 0; n < 100; n++) if (fabs((double)fir_process(&a, (float)xin[n]) - fir_process_symmetric(&b, (float)xin[n])) > 1e-3) diff++;
      CHECK(diff > 50, "asymmetric coefficients: symmetric form differs on %d of 100 samples (documented precondition)", diff); }

    /* 6. Moving average (boxcar, h = 1/N): DC gain 1; first side lobe about -13 dB (book); evaluated from the FIR's own impulse response. */
    { float hb[FIR_TAPS]; for (int i = 0; i < FIR_TAPS; i++) hb[i] = 1.0f / FIR_TAPS;
      fir_filter_t f; fir_init(&f, hb); double imp[FIR_TAPS];
      for (int n = 0; n < FIR_TAPS; n++) imp[n] = fir_process(&f, n == 0 ? 1.0f : 0.0f);
      double dc = 0; for (int n = 0; n < FIR_TAPS; n++) dc += imp[n];
      CHECK_NEAR(dc, 1.0, 1e-6, "boxcar DC gain");
      double sidelobe = 0;
      for (int i = 0; i <= 4000; i++) {
          double w = M_PI * i / 4000.0; if (w < 2.0 * M_PI / FIR_TAPS * 1.0001) continue;   /* beyond the first null */
          double re = 0, im = 0; for (int n = 0; n < FIR_TAPS; n++) { re += imp[n] * cos(w * n); im -= imp[n] * sin(w * n); }
          double m = sqrt(re * re + im * im); if (m > sidelobe) sidelobe = m; }
      CHECK_NEAR(20.0 * log10(sidelobe), -13.0, 0.5, "boxcar first side lobe in dB (book: about -13 dB)"); }

    /* 7. Linear phase: for symmetric h, H(w) * exp(+j w (N-1)/2) is purely real, i.e. constant group delay (N-1)/2 = 15.5 samples (book). */
    { fir_filter_t f; fir_init(&f, h); double imp[FIR_TAPS], worst = 0;
      for (int n = 0; n < FIR_TAPS; n++) imp[n] = fir_process(&f, n == 0 ? 1.0f : 0.0f);
      for (int i = 1; i < 200; i++) {
          double w = M_PI * i / 200.0, re = 0, im = 0, d = (FIR_TAPS - 1) / 2.0;
          for (int n = 0; n < FIR_TAPS; n++) { re += imp[n] * cos(w * (n - d)); im -= imp[n] * sin(w * (n - d)); }
          if (fabs(im) > worst) worst = fabs(im); }
      CHECK(worst < 1e-6, "imaginary part of H(w)e^{jw 15.5}: %g (0 for linear phase)", worst); }

    /* 8. State is per-instance: two filters do not interact, and fir_init clears history. */
    { fir_filter_t a, b; fir_init(&a, h); fir_init(&b, h);
      for (int n = 0; n < 50; n++) fir_process(&a, 1.0f);
      CHECK(fir_process(&b, 0.0f) == 0.0f, "fresh filter outputs zero for zero input regardless of other instance");
      fir_init(&a, h); CHECK(fir_process(&a, 0.0f) == 0.0f, "fir_init clears history"); }
    TEST_DONE("test_A_fir_float");
}
