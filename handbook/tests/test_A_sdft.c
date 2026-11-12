/* GROUP A: sliding DFT bin update (HB[14]).
 * Reference: direct double-precision DFT of the CURRENT window, X_m[k] = sum_{n=0}^{N-1} x[m+n] exp(-j 2 pi k n / N)
 * (oldest sample first, the book's negative-exponent convention).  The prose claims tested here:
 *   - X_new = (X_old - x_leaving + x_entering) * exp(+j 2 pi k / N) reproduces that DFT exactly (sign discussion),
 *   - the pole is on the unit circle, so float rounding error accumulates over a long run,
 *   - a periodic hard reset (recompute the bin directly) cures it,
 *   - a slightly damped variant (pole radius just under 1) trades a small decay for bounded error.
 * The damped variant is NOT in the book's listing; it is a HARNESS in this file, labelled as such. */
#include <stdint.h>
#include "check.h"
#include "hb_sdft.h"

#define N 64
#define K 5
#define LEN 2000000

static float *stream;     /* LEN + N samples */

static void dft_window(const float *w, int k, double *re, double *im) {   /* w[0] is the OLDEST sample */
    double r = 0, i = 0;
    for (int n = 0; n < N; n++) { double a = -2.0 * M_PI * (double)((long)k * n % N) / N; r += (double)w[n] * cos(a); i += (double)w[n] * sin(a); }
    *re = r; *im = i;
}
static void init_bin(sdft_bin_t *b, int k) { b->w_re = (float)cos(2.0 * M_PI * k / N); b->w_im = (float)sin(2.0 * M_PI * k / N); b->x_re = b->x_im = 0.0f; }
static double err_vs_dft(const sdft_bin_t *b, const float *win, int k) {
    double dr, di; dft_window(win, k, &dr, &di); return sqrt((b->x_re - dr) * (b->x_re - dr) + (b->x_im - di) * (b->x_im - di));
}

int main(void) {
    stream = malloc((size_t)(LEN + N) * sizeof *stream);
    if (!stream) { printf("out of memory\n"); return 1; }
    lcg_state = 555;
    for (int i = 0; i < LEN + N; i++) stream[i] = (float)(((int)(lcg_next() & 0xFFFF) - 32768) / 32768.0);   /* white, |x| <= 1 */
    const double scale = sqrt((double)N);     /* typical |X| of white noise */

    /* --- 1. exact recurrence against the direct DFT, window filling from silence then sliding ------------------------------------------------ */
    { sdft_bin_t b; init_bin(&b, K); double worst = 0;
      /* window starts as N zeros: while it fills, the 'leaving' sample is 0 and the window is [0 ... 0, x0 .. x_{m}] */
      for (int m = 0; m < 4 * N; m++) {
          float leaving = (m >= N) ? stream[m - N] : 0.0f;
          sdft_update(&b, leaving, stream[m]);
          float win[N]; for (int n = 0; n < N; n++) { int idx = m - (N - 1) + n; win[n] = idx >= 0 ? stream[idx] : 0.0f; }
          double e = err_vs_dft(&b, win, K); if (e > worst) worst = e; }
      CHECK(worst < 1e-4 * scale, "sliding DFT vs direct DFT of the window, 256 steps: worst error %g (|X| ~ %g)", worst, scale); }

    /* --- 2. the sign: conjugating W must NOT match (the prose's whole point), and a bin on a pure tone has the book's -j-convention phase ----- */
    { sdft_bin_t b; init_bin(&b, K); b.w_im = -b.w_im; double worst = 0;
      for (int m = 0; m < 2 * N; m++) { sdft_update(&b, m >= N ? stream[m - N] : 0.0f, stream[m]);
          float win[N]; for (int n = 0; n < N; n++) { int idx = m - (N - 1) + n; win[n] = idx >= 0 ? stream[idx] : 0.0f; }
          double e = err_vs_dft(&b, win, K); if (e > worst) worst = e; }
      CHECK(worst > 0.5, "with W = exp(-j...) the recurrence does NOT track the -j-convention DFT (worst error %g): the +j sign is required", worst); }
    { sdft_bin_t b; init_bin(&b, K);                  /* pure bin-K sine: X[K] = -j N/2 once the window is full, whatever the absolute time */
      for (int m = 0; m < 3 * N + 17; m++) { float x = (float)sin(2.0 * M_PI * K * m / N); float lv = m >= N ? (float)sin(2.0 * M_PI * K * (m - N) / N) : 0.0f; sdft_update(&b, lv, x); }
      /* window starts at m0 = 2N+18: x[m0+n] = sin(2 pi K (m0+n)/N) = sin(2 pi K n / N + phi), phi = 2 pi K m0 / N */
      int m0 = 3 * N + 17 - N;   /* oldest sample of the final window */ double phi = 2.0 * M_PI * (double)((long)K * m0 % N) / N;
      /* sum sin(wn + phi) e^{-j w n} = (N/2) * (-j) * e^{j phi} */
      double wr = (N / 2.0) * sin(phi), wi = -(N / 2.0) * cos(phi);
      CHECK_NEAR(b.x_re, wr, 1e-3, "pure tone: X.re"); CHECK_NEAR(b.x_im, wi, 1e-3, "pure tone: X.im"); }

    /* --- 3. drift: |W| = 1 exactly in exact arithmetic, but W is stored as float, so |W| != 1 and rounding accumulates -------------------------- */
    {
        sdft_bin_t b; init_bin(&b, K); double w_mag = hypot((double)b.w_re, (double)b.w_im);
        printf("  [info] |W| as stored in float = %.9f  (1 + %.2e)\n", w_mag, w_mag - 1.0);
        double e_early = 0, e_mid = 0, e_late = 0;
        /* run LEN steps with correct leaving samples (window = last N samples of 'stream', first window all zeros) */
        float win[N];
        for (long m = 0; m < LEN; m++) {
            sdft_update(&b, m >= N ? stream[m - N] : 0.0f, stream[m]);
            if (m == 10 * N || m == LEN / 10 || m == LEN - 1) {
                for (int n = 0; n < N; n++) win[n] = stream[m - (N - 1) + n];
                double e = err_vs_dft(&b, win, K);
                if (m == 10 * N) e_early = e; else if (m == LEN / 10) e_mid = e; else e_late = e; } }
        printf("  [info] undamped float SDFT, N=%d k=%d: error after 640 samples %.3e, after %d: %.3e, after %d: %.3e  (|X| ~ %.1f)\n", N, K, e_early, LEN / 10, e_mid, LEN, e_late, scale);
        CHECK(e_early < 1e-4 * scale, "early error small: %g", e_early);
        CHECK(e_late > 20 * e_early, "[drift] error grows with run length: %.2e after 640 samples -> %.2e after %d (x%.0f)", e_early, e_late, LEN, e_late / e_early);
        CHECK(e_late < 1e-2 * scale, "[drift] but still small in absolute terms for this N and k: %.3g (%.3f%% of a typical |X|) after %d samples", e_late, 100 * e_late / scale, LEN);

        /* --- 4. hard reset: recompute the bin directly from the current window (the first cure the book names) -------------------------------- */
        double dr, di; dft_window(win, K, &dr, &di);
        b.x_re = (float)dr; b.x_im = (float)di;
        double e_reset = err_vs_dft(&b, win, K);
        CHECK(e_reset < 1e-5 * scale, "after a hard reset the error is back to float rounding: %g", e_reset);
        /* ...and the recurrence continues correctly from the reset value */
        double worst = 0;
        for (long m = LEN; m < LEN + N; m++) { sdft_update(&b, stream[m - N], stream[m]); if (m % 8 == 0) { for (int n = 0; n < N; n++) win[n] = stream[m - (N - 1) + n]; double e = err_vs_dft(&b, win, K); if (e > worst) worst = e; } }
        CHECK(worst < 1e-3 * scale, "after reset the next %d updates track the DFT again: worst %g", N, worst);
    }

    /* --- 5. damped variant (HARNESS, not the book's listing): X' = r W (X - r^N x_leaving + x_entering), r just under 1 -------------------------- */
    {
        const double r = 1.0 - 1e-3; const double rN = pow(r, N);
        float wr = (float)(r * cos(2.0 * M_PI * K / N)), wi = (float)(r * sin(2.0 * M_PI * K / N));
        float xr = 0, xi = 0; double dxr = 0, dxi = 0, dwr = r * cos(2.0 * M_PI * K / N), dwi = r * sin(2.0 * M_PI * K / N);
        double e_max_late = 0;
        for (long m = 0; m < LEN; m++) {
            float lv = m >= N ? stream[m - N] : 0.0f;
            { float d = stream[m] - (float)rN * lv; float re = xr + d, im = xi; xr = re * wr - im * wi; xi = re * wi + im * wr; }   /* float */
            { double d = (double)stream[m] - rN * lv; double re = dxr + d, im = dxi; dxr = re * dwr - im * dwi; dxi = re * dwi + im * dwr; }   /* double, same recurrence */
            if (m > LEN - 100000) { double e = hypot(xr - dxr, xi - dxi); if (e > e_max_late) e_max_late = e; } }
        printf("  [info] damped (r = 1 - 1e-3) float vs double of the SAME recurrence after %d samples: worst error %.2e\n", LEN, e_max_late);
        /* same experiment with r = 1 (the book's undamped listing, float vs double of the same recurrence) for comparison */
        { float ur = 0, ui = 0; double dur = 0, dui = 0; float uwr = (float)cos(2.0 * M_PI * K / N), uwi = (float)sin(2.0 * M_PI * K / N);
          double duwr = cos(2.0 * M_PI * K / N), duwi = sin(2.0 * M_PI * K / N); double e_und = 0;
          for (long m = 0; m < LEN; m++) { float lv = m >= N ? stream[m - N] : 0.0f;
              { float d = stream[m] - lv; float re = ur + d, im = ui; ur = re * uwr - im * uwi; ui = re * uwi + im * uwr; }
              { double d = (double)stream[m] - lv; double re = dur + d, im = dui; dur = re * duwr - im * duwi; dui = re * duwi + im * duwr; }
              if (m > LEN - 100000) { double e = hypot(ur - dur, ui - dui); if (e > e_und) e_und = e; } }
          printf("  [info] undamped (r = 1) float vs double of the same recurrence: worst error %.2e vs damped %.2e\n", e_und, e_max_late);
          CHECK(e_und > 5 * e_max_late, "damping bounds the error: undamped %.2e vs damped %.2e", e_und, e_max_late); }
        CHECK(e_max_late < 1e-3 * scale, "damped variant: float rounding error stays bounded (%g) over %d samples", e_max_late, LEN);
        /* and the price: its output is a decayed-weight DFT, so a steady bin-K tone reads below the undamped N/2 */
        float sxr = 0, sxi = 0; for (long m = 0; m < 20 * N; m++) { float x = (float)sin(2.0 * M_PI * K * m / N); float lv = m >= N ? (float)sin(2.0 * M_PI * K * (m - N) / N) : 0.0f;
            float d = x - (float)rN * lv; float re = sxr + d, im = sxi; sxr = re * wr - im * wi; sxi = re * wi + im * wr; }
        double mag = hypot(sxr, sxi);
        CHECK(mag < N / 2.0 && mag > 0.9 * N / 2.0, "damped bin reads %.2f instead of N/2 = %g (a small, controlled loss)", mag, N / 2.0);
    }
    free(stream);
    TEST_DONE("test_A_sdft");
}
