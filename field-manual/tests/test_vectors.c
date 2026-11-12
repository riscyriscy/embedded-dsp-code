/* The book's own golden-vector program (Field Manual, Chapter 12, "How to actually run these,
 * with no project template") with the host-build changes marked, plus the extra rows its comments mention:
 * the 19-tap RRC array, and a Goertzel N=8 check against an independent double-precision DFT. */
#include <stdint.h>
#include "fm_qformat.h"
#include "fm_fir.h"
#include "fm_biquad.h"
#include "fm_goertzel.h"
#include "generated/rrc_coeffs.h"

/* ---- begin listing (Chapter 12). Host build: main() is renamed book_vectors() so the
 * extra rows below can share the process, and its closing printf/return are handled by the new main(). ---- */
#include <stdio.h>
#include <string.h>

static int all_ok = 1;
static void check_i16(const char *name, int idx, int16_t got, int16_t want) {
    if (got != want) { printf("FAIL %s[%d]: got %d, want %d\n", name, idx, got, want); all_ok = 0; }
}

int book_vectors(void) {
    static const int16_t fir_vector[FIR_TAPS] = {
        -212, 5, 560, 678, -474, -1977, -1180, 3418, 9498,
        12339, 9498, 3418, -1180, -1977, -474, 678, 560, 5, -212
    };
    for (int n = 0; n < FIR_TAPS; n++) check_i16("fir_coeffs", n, fir_coeffs_q15[n], fir_vector[n]);

    static const int16_t biquad_vector[8] = { 329, 1172, 1948, 2289, 2324, 2160, 1881, 1551 };
    biquad_q15_t bq = { .b0 = 329, .b1 = 658, .b2 = 329, .a1 = -25576, .a2 = 10508, .z1 = 0, .z2 = 0
      };
    for (int n = 0; n < 8; n++) {
        int16_t in = (n == 0) ? FLOAT_TO_Q15(0.5f) : 0;
        check_i16("biquad", n, biquad_process_q15(&bq, in), biquad_vector[n]);
    }
    return all_ok;   /* host build: main() below prints the verdict */
}
/* ---- end listing ---- */

/* extra rows */
int main(void) {
    int ok = book_vectors();

    static const int16_t rrc_vector[FIR_TAPS] = {
        -269, 47, 222, -590, 594, 1324, -3132, -1962, 14084,
        25388, 14084, -1962, -3132, 1324, 594, -590, 222, 47, -269
    };
    for (int n = 0; n < FIR_TAPS; n++) check_i16("rrc_coeffs", n, rrc_coeffs_q15[n], rrc_vector[n]);
    ok = ok && all_ok;

    /* Goertzel, N = 8, bin 1, input x[n] = n+1: compare against a direct DFT in double. */
    enum { N = 8, KBIN = 1 };
    float x[N]; for (int n = 0; n < N; n++) x[n] = (float)(n + 1);
    goertzel_t g; goertzel_init(&g, KBIN, N);
    for (int n = 0; n < N; n++) goertzel_process_sample(&g, x[n]);
    float re, im; goertzel_result(&g, &re, &im);
    double dre = 0, dim = 0;
    for (int n = 0; n < N; n++) { double a = -2.0 * M_PI * KBIN * n / N; dre += x[n] * cos(a); dim += x[n] * sin(a); }
    double dpow = dre * dre + dim * dim;
    int g_ok = fabs(re - dre) < 1e-3 && fabs(im - dim) < 1e-3
            && fabs(goertzel_power(&g) - dpow) < 1e-2 * dpow + 1e-3;
    if (!g_ok) { printf("FAIL goertzel N=8: got (%g,%g) pow %g, DFT (%g,%g) pow %g\n", re, im, goertzel_power(&g), dre, dim, dpow); ok = 0; }
    printf(ok ? "ALL VECTORS OK\n" : "SOME VECTORS FAILED\n");
    return ok ? 0 : 1;
}
