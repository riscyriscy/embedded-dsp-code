/* GROUP A: Q15 FIR (HB[22]), its generated coefficients (HB[20]/[21], tools/design_filter.py -> include/generated/coeffs.h),
 * the usage snippet (HB[23]) and the accumulator-width snippet (HB[19], included verbatim inside a harness function).
 * fir_process_q15 rounds to nearest (half an LSB is added before the shift) and saturates.  References: direct integer
 * convolution in int64 (no ring buffer), floor()/round by definition, the coefficient list in HB[21], and a double-precision
 * filter built from scipy's float taps. */
#include <stdint.h>
#include "check.h"
#include "hb_fir_q15.h"

/* HB[21] as listed in the book (copied from the listing text, independent of the generated header). */
static const int16_t book_coeffs[19] = { -212, 5, 560, 678, -474, -1977, -1180, 3418, 9498, 12339, 9498, 3418, -1180, -1977, -474, 678, 560, 5, -212 };
/* The float design the tool rounds: scipy.signal.remez(19, [0, 1000, 2000, 4000], [1, 0], fs=8000), first 10 taps (the rest mirror). */
static const double remez_half[10] = { -0.006459770503776832, 0.0001431227423469689, 0.0170851906082522, 0.02068733502133348, -0.014457595378433595,
    -0.0603267964957422, -0.036023055385060836, 0.1043126649786665, 0.289855230659019, 0.37654663827151547 };

#define NS 6000
static int16_t xin[NS];

static int64_t acc_ref(const int16_t *x, int t) {      /* sum_k c[k] x[t-k], zero history before t = 0 */
    int64_t a = 0; for (int k = 0; k < 19; k++) if (t - k >= 0) a += (int64_t)book_coeffs[k] * x[t - k]; return a;
}
static int16_t sat16(int64_t v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : (int16_t)v; }
static int64_t floor_div32768(int64_t a) { return a >= 0 ? a / 32768 : -((-a + 32767) / 32768); }

int main(void) {
    CHECK(FIR_TAPS == 19, "FIR_TAPS = %d", FIR_TAPS);
    /* --- generated coefficients == HB[21] == round(32768 * remez) ------------------------------------------------- */
    int diff = 0; for (int i = 0; i < 19; i++) if (fir_coeffs_q15[i] != book_coeffs[i]) diff++;
    CHECK(diff == 0, "include/generated/coeffs.h differs from the book's HB[21] in %d entries", diff);
    int sym = 0; for (int i = 0; i < 19; i++) if (book_coeffs[i] != book_coeffs[18 - i]) sym++;
    CHECK(sym == 0, "coefficients are exactly symmetric (book: linear phase)");
    int rd = 0; for (int i = 0; i < 19; i++) { double f = remez_half[i < 10 ? i : 18 - i]; if ((int)lrint(f * 32768.0) != book_coeffs[i]) rd++; }
    CHECK(rd == 0, "%d coefficients differ from round(32768 * remez taps)", rd);
    CHECK(book_coeffs[9] == 12339 && (12339 / 32768.0) > 0.376 && (12339 / 32768.0) < 0.377, "centre tap 12339 -> 0.377 (book)");
    int in_range = 1; for (int i = 0; i < 19; i++) { double v = book_coeffs[i] / 32768.0; if (v < -0.3 || v > 1.0) in_range = 0; }
    CHECK(in_range, "all coefficients inside the book's predicted '-0.3 to +1.0' range");
    int32_t csum = 0, cabs = 0; for (int i = 0; i < 19; i++) { csum += book_coeffs[i]; cabs += abs(book_coeffs[i]); }
    CHECK(csum == 32971 && cabs == 48343, "sum(c) = %d, sum|c| = %d", (int)csum, (int)cabs);

    /* --- frequency response of the quantised coefficients against the stated spec ----------------------------------- */
    { double pmin = 1e9, pmax = -1e9, smax = -1e9;
      for (int i = 0; i <= 2000; i++) { double f = 4000.0 * i / 2000.0, w = 2 * M_PI * f / 8000.0, re = 0, im = 0;
          for (int n = 0; n < 19; n++) { re += book_coeffs[n] * cos(w * n); im -= book_coeffs[n] * sin(w * n); }
          double db = 20 * log10(sqrt(re * re + im * im) / 32768.0);
          if (f <= 1000.0) { if (db < pmin) pmin = db; if (db > pmax) pmax = db; } else if (f >= 2000.0 && db > smax) smax = db; }
      CHECK(smax < -40.0, "stopband (>= 2 kHz) peak %.2f dB, spec 40 dB", smax);
      CHECK(pmin > -0.2 && pmax < 0.2, "passband (<= 1 kHz) gain %.3f .. %.3f dB", pmin, pmax);
      printf("  [info] quantised 19-tap design: stopband peak %.2f dB, passband %.3f..%.3f dB\n", smax, pmin, pmax); }

    lcg_state = 2024; for (int i = 0; i < NS; i++) xin[i] = (int16_t)((int)(lcg_next() & 0xFFFF) - 32768);

    /* --- impulse response: c * 32767 / 32768 rounds to c for every |c| < 16384, so the response is the coefficient list, symmetric --------- */
    { fir_q15_t f; fir_init_q15(&f, fir_coeffs_q15); int bad = 0;
      for (int n = 0; n < 25; n++) { int16_t y = fir_process_q15(&f, n == 0 ? 32767 : 0); int c = n < 19 ? book_coeffs[n] : 0;
          int want = (int)floor_div32768((int64_t)c * 32767 + 16384);
          if (y != want || y != c) bad++; }
      CHECK(bad == 0, "impulse (32767) response differs from the coefficients at %d taps", bad); }

    /* --- bit-exact against an independent integer convolution (round half up, saturating), including full-scale extremes ---------- */
    { fir_q15_t f; fir_init_q15(&f, fir_coeffs_q15); int bad = 0; double sum_err = 0, worst = 0; long cnt = 0;
      for (int n = 0; n < NS; n++) {
          int16_t y = fir_process_q15(&f, xin[n]); int64_t a = acc_ref(xin, n);
          if (y != sat16(floor_div32768(a + 16384))) bad++;
          double exact = (double)a / 32768.0;
          if (exact > -32768.0 && exact < 32767.0) { double e = (double)y - exact; sum_err += e; if (fabs(e) > worst) worst = fabs(e); cnt++; } }
      CHECK(bad == 0, "fir_process_q15 differs from round-and-saturate integer convolution on %d of %d samples", bad, NS);
      CHECK_NEAR(sum_err / cnt, 0.0, 0.03, "mean output error (LSB): rounding leaves no DC bias");
      CHECK(worst <= 0.5 + 1e-9, "every unsaturated output is within half an LSB of the exact sum: worst %.4f", worst);
      printf("  [info] FIR vs exact sum: mean error %.4f LSB, worst %.4f LSB\n", sum_err / cnt, worst); }
    /* zero input after the last non-zero sample: a non-recursive filter returns to exactly 0 (no limit cycle). */
    { fir_q15_t f; fir_init_q15(&f, fir_coeffs_q15); int16_t y = 0;
      for (int n = 0; n < 200; n++) y = fir_process_q15(&f, xin[n]);
      for (int n = 0; n < 19; n++) y = fir_process_q15(&f, 0);
      CHECK(y == 0, "FIR flushes to exactly 0 after %d zero samples: %d", 19, y); }

    /* --- saturation: 'saturate, don't wrap' (Step 8) --------------------------------------------------------------------- */
    { fir_q15_t f; fir_init_q15(&f, fir_coeffs_q15); int16_t y = 0, yn = 0;
      for (int n = 0; n < 60; n++) y = fir_process_q15(&f, 32767);
      fir_init_q15(&f, fir_coeffs_q15); for (int n = 0; n < 60; n++) yn = fir_process_q15(&f, -32768);
      /* DC gain of this design is 32971/32768 = 1.0062 > 1, so full-scale DC genuinely exceeds the range. */
      int64_t unsat = floor_div32768((int64_t)32971 * 32767);
      CHECK(unsat == 32969, "unsaturated full-scale DC output would be %lld", (long long)unsat);
      CHECK((int16_t)unsat == -32567, "a bare int16_t cast would wrap that to %d", (int16_t)unsat);
      CHECK(y == 32767, "full-scale +DC saturates to %d (not wrapped)", y);
      CHECK(yn == -32768, "full-scale -DC saturates to %d (not wrapped)", yn); }

    /* --- Step 7 claim: peak error vs the float design is 'a few LSBs, well under -70 dBFS' ----------------------------------- */
    { fir_q15_t f; fir_init_q15(&f, fir_coeffs_q15); double peak = 0, ss = 0; long cnt = 0; double h[19];
      for (int i = 0; i < 19; i++) h[i] = remez_half[i < 10 ? i : 18 - i];
      for (int n = 0; n < NS; n++) { int16_t y = fir_process_q15(&f, xin[n]); double r = 0;
          for (int k = 0; k < 19; k++) if (n - k >= 0) r += h[k] * xin[n - k];
          double e = (double)y - r; if (r > -32768.0 && r < 32767.0) { if (fabs(e) > peak) peak = fabs(e); ss += e * e; cnt++; } }
      printf("  [info] random full-scale input vs float design: peak error %.2f LSB (%.1f dBFS), rms %.2f LSB\n", peak, 20 * log10(peak / 32768.0), sqrt(ss / cnt));
      CHECK(peak < 10.0, "peak error %.2f LSB (a few LSBs)", peak);
      CHECK(20 * log10(peak / 32768.0) < -70.0, "peak error %.1f dBFS is below -70 dBFS", 20 * log10(peak / 32768.0)); }

    /* --- Step 6: a 32-bit accumulator suffices only if sum|h| < 2 (here 1.48); int64_t is the safe default ------------------------------ */
    { int64_t worst = 0;
      for (int k = 0; k < 19; k++) worst += (int64_t)abs(book_coeffs[k]) * 32768;          /* every tap hits its worst-case sign at +/-full scale */
      CHECK(worst == 1584103424LL, "worst-case |acc| = %lld", (long long)worst);
      CHECK(worst < INT32_MAX && worst / 2147483648.0 > 0.73 && worst / 2147483648.0 < 0.75, "worst-case |acc| = %lld = %.3f * 2^31: fits in int32_t for the exported 19-tap design", (long long)worst, worst / 2147483648.0);
      CHECK_NEAR(cabs / 32768.0, 1.475, 0.001, "sum of coefficient magnitudes (the condition is sum|h| < 2)");
      /* ...so int64_t is the safe default rather than a necessity here; for 32 full-scale coefficients (HB[19]) it is a necessity. */
      int64_t gen = 32LL * 32767 * 32767; CHECK(gen > INT32_MAX, "32 taps of full-scale coefficient x full-scale data need %lld > INT32_MAX", (long long)gen); }

    /* --- HB[19] snippet, verbatim, inside a harness ------------------------------------------------------------------------------- */
    { int16_t coeffs[32], history[32]; lcg_state = 8;
      for (int i = 0; i < 32; i++) { coeffs[i] = (int16_t)(((int)(lcg_next() & 0xFFFF) - 32768) / 16); history[i] = (int16_t)((int)(lcg_next() & 0xFFFF) - 32768); }
      int64_t ref = 0; for (int i = 0; i < 32; i++) ref += (int64_t)coeffs[i] * history[i];
#include "../reference/hb_acc_snippet.inc"
      CHECK(output == (int16_t)floor_div32768(ref + 16384) && ref / 32768 > -32768 && ref / 32768 < 32767, "HB[19] snippet result %d vs round(sum/2^15) %lld", output, (long long)floor_div32768(ref + 16384));
      for (int i = 0; i < 32; i++) { coeffs[i] = 32767; history[i] = 32767; }
      acc = 0; for (int i = 0; i < 32; i++) acc += (int64_t)coeffs[i] * (int64_t)history[i];
      output = (int16_t)((acc + (1 << 14)) >> 15);
      CHECK(output != 32767, "HB[19] snippet's cast does not clamp (output %d for a 1.05e6 result): as its comment says, clamp acc first if the sum can leave int16_t", output); }

    /* --- HB[23] usage idiom, verbatim, inside a harness ------------------------------------------------------------------------------ */
    {
#include "../reference/hb_fir_q15_usage_snippet.inc"
      int ok = 1; for (int i = 0; i < 19; i++) if (my_filter.coeffs[i] != book_coeffs[i] || my_filter.history[i] != 0) ok = 0;
      CHECK(ok && my_filter.idx == 0, "HB[23] fir_init_q15(&my_filter, fir_coeffs_q15) loads the exported coefficients"); }
    TEST_DONE("test_A_fir_q15");
}
