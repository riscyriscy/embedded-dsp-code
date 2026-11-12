/* HB[34]: rfft_pack() / rfft_unpack() (src/hb_rfft_pack.c) used with fft() (src/hb_fft.c).
 * Reference: naive double-precision DFTs of the two real sequences separately. */
#include <stdint.h>
#include <string.h>
#include <complex.h>
#include "check.h"
#include "hb_fft.h"
#include "hb_rfft_pack.h"

#define NMAX 512
static float a[NMAX], b[NMAX];
static complex_t packed[NMAX], Z[NMAX], A[NMAX / 2 + 1], B[NMAX / 2 + 1];

static double frand(void) { return ((int)(lcg_next() & 0xFFFF) - 32768) / 32768.0; }
static double complex dft_bin(const float *x, int n, int k) {
    long double re = 0, im = 0;
    for (int m = 0; m < n; m++) { long double ang = -2.0L * 3.14159265358979323846L * (long double)((long long)k * m % n) / n; re += x[m] * cosl(ang); im += x[m] * sinl(ang); }
    return (double)re + I * (double)im;
}

int main(void) {
    /* pack: re = a, im = b */
    for (int n = 0; n < 16; n++) { a[n] = (float)n; b[n] = (float)(100 + n); }
    rfft_pack(a, b, packed, 16);
    for (int n = 0; n < 16; n++) CHECK(packed[n].re == a[n] && packed[n].im == b[n], "pack[%d]", n);

    /* round trip through fft + unpack against two separate naive DFTs, N = 4..512 */
    for (int N = 4; N <= NMAX; N *= 2) {
        for (int n = 0; n < N; n++) { a[n] = (float)frand(); b[n] = (float)frand(); }
        rfft_pack(a, b, packed, N);
        fft(packed, Z, N);
        rfft_unpack(Z, A, B, N);
        double worst = 0, scale = 0;
        for (int k = 0; k <= N / 2; k++) {
            double complex ra = dft_bin(a, N, k), rb = dft_bin(b, N, k);
            double ea = hypot(A[k].re - creal(ra), A[k].im - cimag(ra)), eb = hypot(B[k].re - creal(rb), B[k].im - cimag(rb));
            if (ea > worst) worst = ea;
            if (eb > worst) worst = eb;
            if (cabs(ra) > scale) scale = cabs(ra);
            if (cabs(rb) > scale) scale = cabs(rb);
        }
        CHECK(worst < 1e-5 * (1.0 + scale), "N=%d: worst unpack error %g (peak |X| = %g)", N, worst, scale);
        /* a real signal's DC and Nyquist bins are purely real: the special-cased k=0 and k=N/2 must deliver that */
        CHECK_NEAR(A[0].im, 0.0, 1e-5, "A[0] real"); CHECK_NEAR(B[0].im, 0.0, 1e-5, "B[0] real");
        CHECK_NEAR(A[N / 2].im, 0.0, 1e-5, "A[N/2] real"); CHECK_NEAR(B[N / 2].im, 0.0, 1e-5, "B[N/2] real");
    }
    /* known-answer: a = DC 1, b = cosine at bin 1 (N=8):  A[0] = 8, others 0;  B[1] = 4 */
    { const int N = 8; for (int n = 0; n < N; n++) { a[n] = 1.0f; b[n] = (float)cos(2.0 * M_PI * n / N); }
      rfft_pack(a, b, packed, N); fft(packed, Z, N); rfft_unpack(Z, A, B, N);
      CHECK_NEAR(A[0].re, 8.0, 1e-5, "A[0] = 8"); for (int k = 1; k <= N / 2; k++) CHECK_NEAR(hypot(A[k].re, A[k].im), 0.0, 1e-5, "A[k>0] = 0");
      CHECK_NEAR(B[1].re, 4.0, 1e-5, "B[1] = 4"); CHECK_NEAR(B[1].im, 0.0, 1e-5, "B[1] imag 0");
      CHECK_NEAR(hypot(B[0].re, B[0].im), 0.0, 1e-5, "B[0] = 0"); }
    /* a pure sine in b (bin 2, N=16): B[2] = -j*N/2 = 0 - 8j (checks the 2j division / sign) */
    { const int N = 16; for (int n = 0; n < N; n++) { a[n] = 0.0f; b[n] = (float)sin(2.0 * M_PI * 2.0 * n / N); }
      rfft_pack(a, b, packed, N); fft(packed, Z, N); rfft_unpack(Z, A, B, N);
      CHECK_NEAR(B[2].re, 0.0, 1e-5, "B[2] re"); CHECK_NEAR(B[2].im, -8.0, 1e-5, "B[2] = -8j"); }

    /* packed is a separate buffer: a[] and b[] are left untouched, packed[n] = a[n] + j b[n], and a complex_t is exactly an
     * interleaved (re, im) float pair, so an already-interleaved float buffer can serve as packed directly. */
    { enum { N = 8 }; float pa[N], pb[N], ca[N], cb[N]; complex_t pk[N];
      for (int n = 0; n < N; n++) { pa[n] = ca[n] = (float)(n + 1); pb[n] = cb[n] = (float)(-(n + 1)); }
      rfft_pack(pa, pb, pk, N);
      int ok = 1; for (int n = 0; n < N; n++) if (pk[n].re != ca[n] || pk[n].im != cb[n] || pa[n] != ca[n] || pb[n] != cb[n]) ok = 0;
      CHECK(ok, "rfft_pack(): packed[n] = a[n] + j b[n] and the inputs are unchanged");
      CHECK(sizeof(complex_t) == 2 * sizeof(float), "complex_t is an interleaved (re, im) float pair (%zu bytes)", sizeof(complex_t)); }
    TEST_DONE("test_B_rfft_pack");
}
