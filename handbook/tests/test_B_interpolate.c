/* HB[40]: interpolate_by_L() on Chapter 5's fir_q15_t (src/hb_interpolate.c + src/hb_fir_q15.c; FIR_TAPS = 19 from generated/coeffs.h).
 * References: direct convolution of the zero-stuffed sequence in int64; the closed-form frequency response of the 19 coefficients
 * (double precision) for the passband gain and the image levels the prose talks about. */
#include <stdint.h>
#include <complex.h>
#include "check.h"
#include "hb_fir_q15.h"
#include "hb_interpolate.h"

static int16_t clamp16(int64_t v) { return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }

/* H(e^{jw}) of the Q15 coefficients, w in cycles/sample at the FILTER's rate */
static double complex H(double f) { double complex s = 0; for (int k = 0; k < FIR_TAPS; k++) s += (fir_coeffs_q15[k] / 32768.0) * cexp(-2.0 * M_PI * I * f * k); return s; }

int main(void) {
    double H0 = 0; for (int k = 0; k < FIR_TAPS; k++) H0 += fir_coeffs_q15[k] / 32768.0;
    printf("  [info] sum of the 19 coefficients = %.6f (unity-gain design target 1.0)\n", H0);
    CHECK_NEAR(H0, 1.0, 0.01, "Chapter 5 lowpass has ~unity DC gain (the prose's 'ordinary unity-gain lowpass')");

    /* ---- exact output against a direct-convolution reference, L = 1..8, including saturating inputs ---- */
    for (int L = 1; L <= 8; L++) {
        enum { NIN = 120 };
        static int16_t x[NIN]; static int16_t out[NIN * 8]; static int16_t ref[NIN * 8]; static int16_t s[NIN * 8];
        for (int pass = 0; pass < 2; pass++) {
            for (int n = 0; n < NIN; n++) x[n] = (pass == 0) ? (int16_t)((int)(lcg_next() % 4001) - 2000) : (int16_t)((int)(lcg_next() & 0xFFFF) - 32768);
            fir_q15_t f; fir_init_q15(&f, fir_coeffs_q15);
            for (int n = 0; n < NIN; n++) interpolate_by_L(&f, x[n], L, &out[n * L]);
            for (int m = 0; m < NIN * L; m++) s[m] = (m % L == 0) ? x[m / L] : 0;
            for (int m = 0; m < NIN * L; m++) {
                int64_t acc = 0; for (int k = 0; k < FIR_TAPS; k++) if (m - k >= 0) acc += (int64_t)fir_coeffs_q15[k] * s[m - k];
                int16_t y = clamp16((acc + 16384) >> 15);    /* FIR output (Chapter 5 rounds to nearest), saturated to int16 */
                ref[m] = clamp16((int64_t)y * L);              /* then scaled by L, saturated again */
            }
            int bad = 0; for (int m = 0; m < NIN * L; m++) if (out[m] != ref[m]) bad++;
            CHECK(bad == 0, "interpolate_by_L L=%d %s: %d of %d outputs differ from the reference", L, pass ? "full-scale random" : "moderate random", bad, NIN * L);
        }
    }
    /* L outputs per call; output[0] is the first slot (the real sample), outputs[1..L-1] stuffed zeros */
    { fir_q15_t f; fir_init_q15(&f, fir_coeffs_q15); int16_t o[4] = { 1, 2, 3, 4 };
      interpolate_by_L(&f, 10000, 4, o);
      /* impulse response: o[k] = clamp(c[k]*10000>>15) * 4 */
      for (int k = 0; k < 4; k++) CHECK(o[k] == clamp16((int64_t)clamp16(((int64_t)fir_coeffs_q15[k] * 10000 + 16384) >> 15) * 4), "impulse response slot %d = %d", k, o[k]); }

    /* ---- the 'too quiet by L' claim and its fix: DC input ---- */
    for (int L = 2; L <= 8; L *= 2) {
        fir_q15_t f; fir_init_q15(&f, fir_coeffs_q15); int16_t o[8]; double mean = 0; int cnt = 0;
        for (int n = 0; n < 60; n++) { interpolate_by_L(&f, 4000, L, o); if (n >= 20) { for (int k = 0; k < L; k++) mean += o[k]; cnt += L; } }
        mean /= cnt;
        /* every stuffed impulse of size 4000 deposits 4000*H0 in total over the output; scaled by L, the block mean is 4000*H0 */
        CHECK_NEAR(mean, 4000.0 * H0, L + 1.0, "scaled-by-L block mean restores the input level (prose: handled at runtime by *L)");
        double unscaled = mean / L;
        CHECK(unscaled < 4000.0 * H0 / L + 2.0, "without the *L the DC level would be ~1/L of the input (%g vs %g)", unscaled, 4000.0 * H0);
        printf("  [info] L=%d: DC 4000 in -> mean out %.1f with the *L scaling (4000*H0 = %.1f); unscaled would be %.1f\n", L, mean, 4000.0 * H0, unscaled);
    }
    /* ---- image rejection: the zero-stuffed signal repeats the spectrum at multiples of the input rate; the filter must remove them.
     * Predicted line amplitudes (closed form): fundamental A|H(f0/L)|, image k at (k +/- f0)/L: A|H(f)|.  Compared with a DFT of the output. */
    { enum { L = 4, m = 3, NI = 64, NO = NI * L }; const double A = 8000.0;
      fir_q15_t f; fir_init_q15(&f, fir_coeffs_q15); static int16_t last[NO];
      for (int n = 0; n < NI * 3; n++) {   /* 3 periods; keep the last one, after the FIR transient */
          int16_t x = (int16_t)lrint(A * cos(2.0 * M_PI * m * n / NI)); int16_t blk[L];
          interpolate_by_L(&f, x, L, blk);
          if (n >= NI * 2) for (int k = 0; k < L; k++) last[(n - NI * 2) * L + k] = blk[k];
      }
      double worst_err = 0; double max_image = 0;
      for (int bin = 0; bin < NO / 2; bin++) {
          double complex acc = 0; for (int t = 0; t < NO; t++) acc += last[t] * cexp(-2.0 * M_PI * I * bin * t / NO);
          double amp = (bin == 0 ? 1.0 : 2.0) * cabs(acc) / NO;   /* DC line is not mirrored */
          /* expected: tone lines at bin = m + NI*k (k = 0..L-1) and bin = NI*k - m, amplitude A |H(bin/NO)| ; 'bin/NO' is in output-rate cycles/sample */
          double want = 0;
          for (int k = 0; k < L; k++) { if (bin == m + NI * k || bin == NI * k - m) want = A * cabs(H((double)bin / NO)); }
          if (bin == m + NI * 0) { CHECK_NEAR(amp, A, 0.02 * A, "fundamental amplitude restored to A"); }
          else if (bin == NI - m || bin == NI + m) { if (amp > max_image) max_image = amp; }
          double e = fabs(amp - want); if (e > worst_err) worst_err = e;
      }
      printf("  [info] interpolate L=4, tone bin %d: strongest image at %.1f vs fundamental %.1f (%.1f dB); closed-form (|H|) vs measured worst line error %.2f LSB\n", m, max_image, A, 20 * log10(max_image / A), worst_err);
      CHECK(worst_err < 3.0, "every spectral line (fundamental + images) matches A|H(f)| from the coefficients' frequency response: worst error %g", worst_err);
      CHECK(max_image < 0.1 * A, "the nearest images (+-%d bins around fs_in) are at least 20 dB down: %.1f vs %.1f", m, max_image, A); }
    TEST_DONE("test_B_interpolate");
}
