/* GROUP A: Q-format helpers (HB[1]) and the FLOAT_TO_Q15 call-site snippet (HB[18]).
 * References: exact rational arithmetic via double (every product here is < 2^53), floor/round by definition,
 * and the worked numbers the book gives (0.75, 0.1, 0.0731). */
#include <stdint.h>
#include "check.h"
#include "hb_qformat.h"

int main(void) {
    /* --- FLOAT_TO_Q15 ------------------------------------------------------------------------------------ */
    CHECK(FLOAT_TO_Q15(0.75f) == 24576, "0.75 -> %d, book gives 24576 (0x6000)", (int)FLOAT_TO_Q15(0.75f));
    CHECK(FLOAT_TO_Q15(0.1f) == 3277, "0.1 -> %d, book gives 3277", (int)FLOAT_TO_Q15(0.1f));
    CHECK_NEAR(Q15_TO_FLOAT(3277), 0.100006, 5e-7, "3277 decodes to ~0.100006 (book)");
    CHECK(FLOAT_TO_Q15(0.0731f) == 2395, "HB[18] FLOAT_TO_Q15(0.0731f) = %d, 0.0731*32768 = 2395.4 -> 2395", (int)FLOAT_TO_Q15(0.0731f));
    { int16_t coeff_f = FLOAT_TO_Q15(0.0731f);   /* the HB[18] line, verbatim: the int32_t result fits int16_t */
      CHECK(coeff_f == 2395, "HB[18] stored in int16_t: %d", coeff_f); }
    CHECK(FLOAT_TO_Q15(1.0f) == 32767, "+1.0 clamps to 32767, not 32768");
    CHECK(FLOAT_TO_Q15(-1.0f) == -32768, "-1.0 -> -32768 exactly");
    CHECK(FLOAT_TO_Q15(5.0f) == 32767 && FLOAT_TO_Q15(-5.0f) == -32768, "out-of-range clamps both ends");
    CHECK(FLOAT_TO_Q15(0.5f) == 16384, "0.5 -> 16384");
    CHECK(FLOAT_TO_Q15(0.0f) == 0, "0 -> 0");
    /* Round-to-nearest: over a sweep, |q - f*32768| <= 0.5 for in-range values (independent: plain double arithmetic). */
    double worst = 0; uint32_t s = 1;
    for (int i = 0; i < 20000; i++) {
        s = s * 1664525u + 1013904223u;
        float f = ((int32_t)(s >> 8) % 65535) / 65536.0f;       /* [-1, 1) approx, float-exact grid of 1/65536 */
        double err = fabs((double)FLOAT_TO_Q15(f) - (double)f * 32768.0);
        if (err > worst) worst = err;
    }
    CHECK(worst <= 0.5 + 1e-9, "FLOAT_TO_Q15 rounding error up to %g LSB, want <= 0.5", worst);
    /* lrintf rounds ties to even, not away from zero: pin this so nobody assumes round() semantics. */
    CHECK(FLOAT_TO_Q15(0.5f / 32768.0f) == 0 && FLOAT_TO_Q15(1.5f / 32768.0f) == 2 && FLOAT_TO_Q15(2.5f / 32768.0f) == 2,
          "ties-to-even: 0.5->%d 1.5->%d 2.5->%d", (int)FLOAT_TO_Q15(0.5f / 32768.0f), (int)FLOAT_TO_Q15(1.5f / 32768.0f), (int)FLOAT_TO_Q15(2.5f / 32768.0f));
    CHECK_NEAR(Q15_TO_FLOAT(24576), 0.75, 0, "Q15_TO_FLOAT(24576)");
    CHECK_NEAR(Q15_TO_FLOAT(-32768), -1.0, 0, "Q15_TO_FLOAT(-32768)");

    /* --- q15_mul_trunc ----------------------------------------------------------------------------------- */
    CHECK(q15_mul_trunc(24576, -16384) == -12288, "0.75 * -0.5 -> %d, book gives -12288", (int)q15_mul_trunc(24576, -16384));
    CHECK_NEAR(Q15_TO_FLOAT(q15_mul_trunc(24576, -16384)), -0.375, 0, "...decodes to exactly -0.375");
    CHECK((int64_t)19661 * -16384 == -322125824LL, "book's Q30 product for 0.6 * -0.5");
    CHECK(q15_mul_trunc(19661, -16384) == -9831, "0.6 * -0.5 truncates (floors) to %d, book gives -9831", (int)q15_mul_trunc(19661, -16384));
    CHECK_NEAR(Q15_TO_FLOAT(-9831), -0.300018, 5e-7, "-9831 decodes to ~-0.300018 (book)");
    /* Bit-by-bit definition: q15_mul_trunc == floor(a*b / 2^15); error vs exact is in (-1, 0]; mean about -0.5 LSB (the bias the book describes). */
    int bad = 0; double sum_err = 0; const int N = 50000;
    for (int i = 0; i < N; i++) {
        int32_t a = (int32_t)(lcg_next() & 0xFFFF) - 32768, b = (int32_t)(lcg_next() & 0xFFFF) - 32768;
        double exact = (double)a * (double)b / 32768.0;
        double want = floor(exact);
        int32_t got = q15_mul_trunc(a, b);
        if ((double)got != want) bad++;
        sum_err += (double)got - exact;
    }
    CHECK(bad == 0, "q15_mul_trunc differs from floor(a*b/2^15) on %d of %d", bad, N);
    CHECK_NEAR(sum_err / N, -0.5, 0.02, "mean truncation error (LSB) -- the one-directional bias");
    /* -1.0 * -1.0 = +1.0 is not representable in Q15: the wide container returns 32768, the int16_t range ends at 32767. */
    CHECK(q15_mul_trunc(-32768, -32768) == 32768, "(-1)*(-1) -> %d: lands one past INT16_MAX (caller must saturate)", (int)q15_mul_trunc(-32768, -32768));

    /* --- saturating adds ------------------------------------------------------------------------------- */
    CHECK(q15_wide_add_sat(INT32_MAX, 1) == INT32_MAX && q15_wide_add_sat(INT32_MIN, -1) == INT32_MIN, "wide add saturates at int32 limits");
    CHECK(q15_wide_add_sat(40000, 40000) == 80000, "wide add does NOT clamp to Q15 range: %d", (int)q15_wide_add_sat(40000, 40000));
    CHECK(q15_wide_add_sat(-INT32_MAX, -5) == INT32_MIN, "wide add negative saturation");
    CHECK(q15_add_sat(32767, 1) == 32767 && q15_add_sat(30000, 30000) == 32767, "narrow add clamps at +32767");
    CHECK(q15_add_sat(-32768, -1) == -32768 && q15_add_sat(-30000, -30000) == -32768, "narrow add clamps at -32768");
    /* The clamp range is Q15's full [-32768, 32767], as the listing's comment says. */
    CHECK(q15_add_sat(-32768, 0) == -32768, "q15_add_sat keeps -32768 (the range is [-32768, 32767])");
    CHECK(q15_add_sat(100, 200) == 300 && q15_add_sat(-100, 50) == -50, "in-range sums pass through");
    TEST_DONE("test_A_qformat");
}
