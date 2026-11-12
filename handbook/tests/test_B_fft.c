/* HB[32]: bit_reverse_permute() and fft() (src/hb_fft.c).
 * References (independent of the code under test): a bit-by-bit index reversal, and a naive O(N^2) DFT in double precision
 * (X[k] = sum x[n] e^{-j 2 pi k n / N}).  Also the book's golden vector: N=8, x[n]=cos(2 pi n/8) -> X[1]=X[7]=4+0j. */
#include <stdint.h>
#include <complex.h>
#include "check.h"
#include "hb_fft.h"

static double frand(void) { return ((int)(lcg_next() & 0xFFFF) - 32768) / 32768.0; }   /* [-1, 1) */

static int rev_bits(int i, int bits) { int r = 0; for (int b = 0; b < bits; b++) if ((i >> b) & 1) r |= 1 << (bits - 1 - b); return r; }

static void naive_dft(const double complex *x, double complex *X, int n) {
    for (int k = 0; k < n; k++) {
        long double re = 0, im = 0;
        for (int m = 0; m < n; m++) {
            long double ang = -2.0L * 3.14159265358979323846L * ((long double)((long long)k * m % n)) / n;   /* reduce k*m mod n for accuracy */
            re += creal(x[m]) * cosl(ang) - cimag(x[m]) * sinl(ang);
            im += creal(x[m]) * sinl(ang) + cimag(x[m]) * cosl(ang);
        }
        X[k] = (double)re + I * (double)im;
    }
}

#define NMAX 1024
static complex_t in_f[NMAX], out_f[NMAX];
static double complex xin[NMAX], Xref[NMAX];

static double max_err_vs_ref(int n) {
    double e = 0; for (int k = 0; k < n; k++) { double d = hypot(out_f[k].re - creal(Xref[k]), out_f[k].im - cimag(Xref[k])); if (d > e) e = d; }
    return e;
}
static double rms(const double complex *v, int n) { double s = 0; for (int k = 0; k < n; k++) s += creal(v[k]) * creal(v[k]) + cimag(v[k]) * cimag(v[k]); return sqrt(s / n); }

int main(void) {
    /* ---- bit_reverse_permute against the bit-by-bit definition, N = 1..1024 ---- */
    for (int n = 1; n <= NMAX; n *= 2) {
        int bits = 0; while ((1 << bits) < n) bits++;
        for (int i = 0; i < n; i++) { in_f[i].re = (float)i; in_f[i].im = (float)(-i); }
        bit_reverse_permute(in_f, n);
        int ok = 1;
        for (int i = 0; i < n; i++) if (in_f[rev_bits(i, bits)].re != (float)i || in_f[rev_bits(i, bits)].im != (float)(-i)) ok = 0;
        CHECK(ok, "bit_reverse_permute n=%d: element i must land at rev(i)", n);
        bit_reverse_permute(in_f, n);   /* involution */
        ok = 1; for (int i = 0; i < n; i++) if (in_f[i].re != (float)i) ok = 0;
        CHECK(ok, "bit_reverse_permute n=%d applied twice must be the identity", n);
    }
    /* the book's own N=8 example: index 1 = 001 swaps with 4 = 100 */
    for (int i = 0; i < 8; i++) { in_f[i].re = (float)i; in_f[i].im = 0; }
    bit_reverse_permute(in_f, 8);
    static const int want8[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
    for (int i = 0; i < 8; i++) CHECK(in_f[i].re == (float)want8[i], "N=8 permutation[%d]: got %g want %d", i, in_f[i].re, want8[i]);

    /* ---- golden vector: x[n] = cos(2 pi n / 8) -> X[1] = X[7] = 4+0j, everything else ~0 ---- */
    for (int n = 0; n < 8; n++) { in_f[n].re = (float)cos(2.0 * M_PI * n / 8.0); in_f[n].im = 0.0f; }
    fft(in_f, out_f, 8);
    CHECK_NEAR(out_f[1].re, 4.0, 1e-6, "golden X[1].re"); CHECK_NEAR(out_f[1].im, 0.0, 1e-6, "golden X[1].im");
    CHECK_NEAR(out_f[7].re, 4.0, 1e-6, "golden X[7].re"); CHECK_NEAR(out_f[7].im, 0.0, 1e-6, "golden X[7].im");
    double off = 0;
    for (int k = 0; k < 8; k++) if (k != 1 && k != 7) { double m = hypot(out_f[k].re, out_f[k].im); if (m > off) off = m; }
    printf("  [info] golden vector: largest off-bin magnitude = %.3g (prose: on the order of 1e-7 for float)\n", off);
    CHECK(off < 1e-6 && off > 1e-8, "golden vector off-bin magnitude %g must be float rounding noise, of order 1e-7", off);

    /* ---- impulse: x[m] = delta -> X[k] = e^{-j 2 pi k m / N} ---- */
    for (int n = 8; n <= NMAX; n *= 2) {
        for (int m = 0; m < n; m += (n / 4 > 0 ? n / 4 : 1)) {
            for (int i = 0; i < n; i++) { in_f[i].re = (i == m); in_f[i].im = 0; }
            fft(in_f, out_f, n);
            double e = 0;
            for (int k = 0; k < n; k++) {
                double ang = -2.0 * M_PI * (double)((long long)k * m % n) / n;
                double d = hypot(out_f[k].re - cos(ang), out_f[k].im - sin(ang)); if (d > e) e = d;
            }
            CHECK(e < 5e-6, "impulse n=%d m=%d: max |X-ref| = %g", n, m, e);
        }
    }
    /* ---- complex tone on bin b: X[b] = N, rest 0 ---- */
    for (int n = 8; n <= NMAX; n *= 2) {
        int b = n / 8 + 1;
        for (int i = 0; i < n; i++) { double ang = 2.0 * M_PI * (double)((long long)b * i % n) / n; in_f[i].re = (float)cos(ang); in_f[i].im = (float)sin(ang); }
        fft(in_f, out_f, n);
        double e = 0;
        for (int k = 0; k < n; k++) { double wr = (k == b) ? n : 0.0; double d = hypot(out_f[k].re - wr, out_f[k].im); if (d > e) e = d; }
        CHECK(e < 1e-5 * n, "tone n=%d bin %d: max error %g", n, b, e);
        CHECK_NEAR(out_f[b].re, n, 1e-5 * n, "tone peak = N");
    }
    /* ---- random complex input, N = 8..1024, against the naive double-precision DFT, and Parseval ---- */
    for (int n = 8; n <= NMAX; n *= 2) {
        for (int i = 0; i < n; i++) { in_f[i].re = (float)frand(); in_f[i].im = (float)frand(); xin[i] = (double)in_f[i].re + I * (double)in_f[i].im; }
        naive_dft(xin, Xref, n);
        fft(in_f, out_f, n);
        double e = max_err_vs_ref(n), scale = rms(Xref, n);
        /* float arithmetic, N*log2(N) operations, twiddles from cosf/sinf of a float angle (3.14159265f is ~9e-8 off): a
         * relative error budget of 5e-6 of the RMS spectrum magnitude is comfortably above float rounding and far below any bug. */
        printf("  [info] n=%4d: max |fft - DFT| = %.3g  (rms |X| = %.3g, ratio %.2g)\n", n, e, scale, e / scale);
        CHECK(e < 5e-6 * scale, "random n=%d: max error %g vs rms %g", n, e, scale);
        /* Parseval: sum |X|^2 = N * sum |x|^2 */
        double sx = 0, sX = 0;
        for (int i = 0; i < n; i++) { sx += (double)in_f[i].re * in_f[i].re + (double)in_f[i].im * in_f[i].im; sX += (double)out_f[i].re * out_f[i].re + (double)out_f[i].im * out_f[i].im; }
        CHECK_NEAR(sX / (n * sx), 1.0, 1e-4, "Parseval ratio");
        /* input array must be left untouched (fft copies into output) */
        int same = 1; for (int i = 0; i < n; i++) if ((double)in_f[i].re != creal(xin[i]) || (double)in_f[i].im != cimag(xin[i])) same = 0;
        CHECK(same, "fft(input, output, n) must not modify input (n=%d)", n);
    }
    /* ---- real input: conjugate symmetry X[N-k] = conj(X[k]) ---- */
    for (int i = 0; i < 64; i++) { in_f[i].re = (float)frand(); in_f[i].im = 0; }
    fft(in_f, out_f, 64);
    for (int k = 1; k < 64; k++) { CHECK_NEAR(out_f[64 - k].re, out_f[k].re, 3e-5, "real input: Re symmetric"); CHECK_NEAR(out_f[64 - k].im, -out_f[k].im, 3e-5, "real input: Im antisymmetric"); }
    /* ---- linearity ---- */
    { static complex_t a[64], b[64], s[64], Fa[64], Fb[64], Fs[64];
      for (int i = 0; i < 64; i++) { a[i].re = (float)frand(); a[i].im = (float)frand(); b[i].re = (float)frand(); b[i].im = (float)frand(); s[i].re = a[i].re + 2.0f * b[i].re; s[i].im = a[i].im + 2.0f * b[i].im; }
      fft(a, Fa, 64); fft(b, Fb, 64); fft(s, Fs, 64);
      for (int k = 0; k < 64; k++) { CHECK_NEAR(Fs[k].re, Fa[k].re + 2.0 * Fb[k].re, 3e-5, "linearity re"); CHECK_NEAR(Fs[k].im, Fa[k].im + 2.0 * Fb[k].im, 3e-5, "linearity im"); } }
    /* ---- in-place use (input == output) ---- */
    for (int i = 0; i < 64; i++) { in_f[i].re = (float)frand(); in_f[i].im = (float)frand(); xin[i] = (double)in_f[i].re + I * (double)in_f[i].im; }
    naive_dft(xin, Xref, 64);
    fft(in_f, in_f, 64);
    for (int k = 0; k < 64; k++) { out_f[k] = in_f[k]; }
    CHECK(max_err_vs_ref(64) < 5e-6 * rms(Xref, 64), "fft(x, x, n) in place still correct");
    TEST_DONE("test_B_fft");
}
