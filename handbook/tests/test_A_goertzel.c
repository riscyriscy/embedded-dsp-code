/* GROUP A: Goertzel resonator (HB[13]).
 * References: the direct DFT X[k] = sum x[n] exp(-j 2 pi k n / N) in double precision, the book's own N = 8 worked example
 * (x[n] = cos(2 pi n / 8) -> X[1] = 4 + 0j), and the closed-form Dirichlet/sinc scalloping loss behind the DTMF table. */
#include <stdint.h>
#include "check.h"
#include "hb_goertzel.h"

static void dft_bin(const double *x, int N, int k, double *re, double *im) {
    double r = 0, i = 0;
    for (int n = 0; n < N; n++) { double a = -2.0 * M_PI * (double)((long)k * n % N) / N; r += x[n] * cos(a); i += x[n] * sin(a); }
    *re = r; *im = i;
}
static void run_block(const float *x, int N, int k, float *re, float *im, float *pow_out) {
    goertzel_t g; goertzel_init(&g, k, N);
    for (int n = 0; n < N; n++) goertzel_process_sample(&g, x[n]);
    goertzel_result(&g, re, im); *pow_out = goertzel_power(&g);
}

int main(void) {
    /* --- book's worked example: N = 8, x[n] = cos(2 pi n / 8), bin 1 -> 4 + 0j ------------------------------------------------ */
    { float x[8]; for (int n = 0; n < 8; n++) x[n] = (float)cos(2 * M_PI * n / 8);
      float re, im, p; run_block(x, 8, 1, &re, &im, &p);
      CHECK_NEAR(re, 4.0, 1e-5, "X[1].re (book gives 4)"); CHECK_NEAR(im, 0.0, 1e-5, "X[1].im (book gives 0)"); CHECK_NEAR(p, 16.0, 1e-4, "|X[1]|^2 = 16");
      /* sign convention (the book's point: power alone cannot catch a phase error): sin input has X[1] = -j N/2 in the e^{-j...} convention. */
      for (int n = 0; n < 8; n++) x[n] = (float)sin(2 * M_PI * n / 8);
      run_block(x, 8, 1, &re, &im, &p);
      CHECK_NEAR(re, 0.0, 1e-5, "sin: X[1].re"); CHECK_NEAR(im, -4.0, 1e-5, "sin: X[1].im = -4 (negative-exponent DFT convention)");
      /* another bin of the same cosine: zero (orthogonality) */
      for (int n = 0; n < 8; n++) x[n] = (float)cos(2 * M_PI * n / 8);
      run_block(x, 8, 3, &re, &im, &p);
      CHECK_NEAR(re, 0.0, 1e-5, "cos: X[3].re"); CHECK_NEAR(p, 0.0, 1e-4, "cos: |X[3]|^2"); }

    /* --- random blocks, several (N, k), including the non power-of-two N = 105; interior bins 1 .. N/2-1 --------------------------------- */
    { const int Ns[] = { 8, 64, 105, 205, 256, 1000 }; const int nN = 6; double worst_rel = 0, worst_rel_p = 0; lcg_state = 1234;
      for (int ni = 0; ni < nN; ni++) { int N = Ns[ni];
          for (int t = 0; t < 8; t++) {
              int k = 1 + (int)(lcg_next() % (unsigned)(N / 2 - 1));
              double xd[1000]; float xf[1000];
              for (int n = 0; n < N; n++) { xf[n] = (float)(((int)(lcg_next() & 0xFFFF) - 32768) / 32768.0); xd[n] = xf[n]; }
              double dr, di; dft_bin(xd, N, k, &dr, &di);
              float re, im, p; run_block(xf, N, k, &re, &im, &p);
              double scale = sqrt((double)N);   /* random-signal bin magnitude ~ sqrt(N) * sigma */
              double e = sqrt((re - dr) * (re - dr) + (im - di) * (im - di)) / scale; if (e > worst_rel) worst_rel = e;
              double dp = dr * dr + di * di; double ep = fabs(p - dp) / (dp + scale * scale * 0.1); if (ep > worst_rel_p) worst_rel_p = ep; } }
      CHECK(worst_rel < 1e-4, "goertzel_result vs double DFT (interior bins): worst error %g (relative to sqrt(N))", worst_rel);
      CHECK(worst_rel_p < 1e-3, "goertzel_power vs |double DFT|^2 (interior bins): worst relative error %g", worst_rel_p); }

    /* --- edge bins k = 0 and k = N/2: double pole at z = +-1, the state grows like N^1.5, so the float noise floor is higher there ------------ */
    /* Bounds are derived, not tuned: absolute error ~ a few * eps * N^1.5 for the result and ~ eps * (N^1.5)^2 for the cancelling power form;
     * asserted as <= 1e-2 and <= 1e-4 of the full-scale bin power N^2/4 respectively. */
    { enum { N = 1000 }; static float xf[N]; static double xd[N]; lcg_state = 1234;
      for (int n = 0; n < N; n++) { xf[n] = (float)(((int)(lcg_next() & 0xFFFF) - 32768) / 32768.0); xd[n] = xf[n]; }
      for (int e = 0; e < 2; e++) { int k = e ? N / 2 : 0; double dr, di; dft_bin(xd, N, k, &dr, &di); float re, im, p; run_block(xf, N, k, &re, &im, &p);
          double dp = dr * dr + di * di;
          CHECK(fabs(re - dr) < 1e-2 && fabs(im - di) < 1e-2, "k = %d: goertzel_result error (%g, %g)", k, re - dr, im - di);
          CHECK(fabs(p - dp) < 1e-4 * N * N / 4.0, "k = %d: goertzel_power %g vs exact %g", k, (double)p, dp);
          printf("  [info] N=1000 k=%d: goertzel_result error %.1e, goertzel_power %.4f vs exact %.4f\n", k, fabs(re - dr), (double)p, dp); } }

    /* --- state: goertzel_init() clears s1/s2, so back-to-back blocks are independent ------------------------------------------------ */
    { float x[105]; for (int n = 0; n < 105; n++) x[n] = (float)cos(2 * M_PI * 18 * n / 105.0);
      goertzel_t g; goertzel_init(&g, 18, 105); float r1, i1, r2, i2;
      for (int n = 0; n < 105; n++) goertzel_process_sample(&g, x[n]);
      goertzel_result(&g, &r1, &i1);
      goertzel_init(&g, 18, 105); for (int n = 0; n < 105; n++) goertzel_process_sample(&g, x[n]); goertzel_result(&g, &r2, &i2);
      CHECK(r1 == r2 && i1 == i2, "re-init gives the identical result");
      CHECK_NEAR(r1, 52.5, 1e-3, "on-bin cosine: X = N/2 = 52.5"); }

    /* --- longer block / float resonator with a pole ON the unit circle: accuracy ------------------------------------------------------ */
    { enum { N = 4096 }; static float xf[N]; static double xd[N]; for (int n = 0; n < N; n++) { xd[n] = cos(2 * M_PI * 100 * n / N + 0.4); xf[n] = (float)xd[n]; }
      double dr, di; dft_bin(xd, N, 100, &dr, &di); float re, im, p; run_block(xf, N, 100, &re, &im, &p);
      double rel = sqrt((re - dr) * (re - dr) + (im - di) * (im - di)) / sqrt(dr * dr + di * di);
      printf("  [info] N=4096 on-bin tone: Goertzel float relative error %.2e\n", rel);
      CHECK(rel < 1e-3, "N = 4096: relative error %g", rel); }

    /* --- DTMF table (N = 105, fs = 8 kHz): every number the book gives --------------------------------------------------------------- */
    { static const double f[8] = { 697, 770, 852, 941, 1209, 1336, 1477, 1633 };
      static const double ex[8] = { 9.148, 10.106, 11.182, 12.351, 15.868, 17.535, 19.386, 21.433 };
      static const int kk[8] = { 9, 10, 11, 12, 16, 18, 19, 21 };
      static const double tuned[8] = { 685.71, 761.90, 838.10, 914.29, 1219.05, 1371.43, 1447.62, 1600.00 };
      static const double err[8] = { -11.3, -8.1, -13.9, -26.7, 10.0, 35.4, -29.4, -33.0 };
      int ok = 1;
      for (int i = 0; i < 8; i++) {
          double e = 105.0 * f[i] / 8000.0; int k = (int)floor(e + 0.5); double t = k * 8000.0 / 105.0;
          if (fabs(e - ex[i]) > 5e-4 || k != kk[i] || fabs(t - tuned[i]) > 5e-3 || fabs((t - f[i]) - err[i]) > 0.05) { ok = 0; printf("    row %d: N f/fs = %.4f k = %d tuned %.3f err %.2f\n", i, e, k, t, t - f[i]); } }
      CHECK(ok, "DTMF table (N = 105): exact value, rounded k, tuned frequency, error all match the book");
      /* worst case 1336 Hz: 0.465 bin off */
      CHECK_NEAR(fabs(17.535 - 18), 0.465, 1e-3, "1336 Hz lands 0.465 bins from bin 18");
      /* scalloping loss through the actual resonator: tone at 1336 Hz vs a tone exactly on bin 18, rectangular window, same amplitude */
      for (int i = 0; i < 8; i++) {
          int k = kk[i]; float xa[105], xb[105];
          for (int n = 0; n < 105; n++) { xa[n] = (float)sin(2 * M_PI * f[i] * n / 8000.0 + 0.7); xb[n] = (float)sin(2 * M_PI * k * n / 105.0 + 0.7); }
          float ra, ia, pa, rb, ib, pb; run_block(xa, 105, k, &ra, &ia, &pa); run_block(xb, 105, k, &rb, &ib, &pb);
          double loss_db = 10 * log10(pa / pb);
          double d = fabs(105.0 * f[i] / 8000.0 - k);
          if (i == 5) {
              /* book: sin(pi d)/(pi d) -> about -3.3 dB for d = 0.465 (the sinc approximation of the Dirichlet kernel) */
              double sinc_db = 20 * log10(sin(M_PI * d) / (M_PI * d));
              CHECK_NEAR(sinc_db, -3.3, 0.1, "book's sinc estimate for 0.465 bin");
              CHECK(loss_db < -2.9 && loss_db > -4.0, "1336 Hz: measured loss through the k = 18 resonator %.2f dB (book: roughly -3.3 dB, half-bin worst case -3.92)", loss_db);
              printf("  [info] 1336 Hz tone through Goertzel(k=18, N=105): measured %.2f dB vs bin-centred tone; sinc formula %.2f dB\n", loss_db, sinc_db); }
          CHECK(loss_db < 0.5, "tone %g Hz: off-bin tone is not stronger than a bin-centred one (%.2f dB)", f[i], loss_db); }
      CHECK_NEAR(20 * log10(sin(M_PI * 0.5) / (M_PI * 0.5)), -3.92, 0.01, "half-bin scalloping loss -3.92 dB (book)"); }
    TEST_DONE("test_A_goertzel");
}
