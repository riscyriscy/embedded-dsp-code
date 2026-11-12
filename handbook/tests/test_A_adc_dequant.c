/* GROUP A: adc_to_q15 (HB[17]) and the two dequantize functions (HB[26]).
 * References: the bit-by-bit definition (adc - 2048) * 16 in plain int arithmetic over every possible 12-bit code, and closed-form
 * scale * (q - zero_point). */
#include <stdint.h>
#include "check.h"
#include "hb_adc_q15.h"
#include "hb_dequant.h"

int main(void) {
    /* --- adc_to_q15 over the whole 12-bit range -------------------------------------------------------------------------------------- */
    int bad = 0, nonmono = 0; int prev = -100000;
    for (int a = 0; a <= 4095; a++) {
        int want = (a - 2048) * 16; int16_t got = adc_to_q15((uint16_t)a);
        if (got != want) bad++;
        if (got <= prev) nonmono++;
        prev = got;
    }
    CHECK(bad == 0, "adc_to_q15 differs from (adc - 2048) * 16 for %d of 4096 codes", bad);
    CHECK(nonmono == 0, "adc_to_q15 is strictly increasing");
    CHECK(adc_to_q15(2048) == 0, "mid-scale 2048 -> 0");
    CHECK(adc_to_q15(0) == -32768, "code 0 -> -32768 (-1.0)");
    CHECK(adc_to_q15(4095) == 32752, "code 4095 -> %d: 15 counts below INT16_MAX (gain 0.99954, not exactly full scale)", adc_to_q15(4095));
    CHECK(adc_to_q15(2049) - adc_to_q15(2048) == 16, "one ADC count = 16 Q15 counts (4 LSBs unused)");
    /* 'The shift amount is 4, not 3 ... a 3-bit shift would waste one full bit, 6 dB' */
    { int max3 = (4095 - 2048) << 3, max4 = (4095 - 2048) << 4;
      CHECK(max3 == 16376 && max4 == 32752, "peak output with shift 3 / shift 4: %d / %d", max3, max4);
      CHECK_NEAR(20 * log10((double)max4 / max3), 6.02, 0.01, "shift 3 would waste 6.02 dB (one bit)");
      CHECK((4095 - 2048) << 5 > 32767, "shift 5 would overflow int16_t (65504): the largest safe shift is 4"); }
    /* Domain note (documented in the listing): a raw value outside 12 bits is not saturated, it wraps, so the caller must mask to 12 bits. */
    CHECK(adc_to_q15(4096) == -32768, "adc_to_q15(4096) = %d: out-of-range input wraps instead of saturating", adc_to_q15(4096));

    /* --- dequantize ------------------------------------------------------------------------------------------------------------------- */
    CHECK_NEAR(dequantize_q15(16384), 0.5, 0, "0.5"); CHECK_NEAR(dequantize_q15(-32768), -1.0, 0, "-1.0");
    CHECK_NEAR(dequantize_q15(32767), 32767.0 / 32768.0, 0, "just under +1"); CHECK_NEAR(dequantize_q15(0), 0.0, 0, "0");
    { int ok = 1; for (int v = -32768; v <= 32767; v += 7) if (dequantize_q15((int16_t)v) != (float)((double)v / 32768.0)) ok = 0; CHECK(ok, "dequantize_q15 == v / 2^15 (exact in float) over the int16 range"); }
    /* symmetric around zero (Q15's defining property, book): negation is exact except for -32768 */
    CHECK(dequantize_q15(-12345) == -dequantize_q15(12345), "symmetric around zero");
    /* affine int8 */
    CHECK_NEAR(dequantize_affine_int8(10, 0.1f, 0), 1.0, 1e-6, "scale 0.1, zp 0, val 10 -> 1.0");
    CHECK_NEAR(dequantize_affine_int8(-128, 0.05f, -128), 0.0, 0, "val == zero_point -> exactly 0");
    CHECK_NEAR(dequantize_affine_int8(127, 0.05f, -128), 0.05 * 255, 1e-5, "full uint8-style range with zp = -128: 255 steps");
    CHECK_NEAR(dequantize_affine_int8(-128, 0.05f, 127), 0.05 * -255, 1e-5, "zero point at the other end");
    { /* the representable range is NOT symmetric about zero for a non-zero zero_point (the book's point) */
      double lo = dequantize_affine_int8(-128, 0.1f, -100), hi = dequantize_affine_int8(127, 0.1f, -100);
      CHECK(lo > -3.0 && hi > 20.0 && fabs(lo) < fabs(hi) / 5, "zp = -100: range [%.1f, %.1f] sits mostly on one side of zero", lo, hi); }
    /* Q15 is the zero_point = 0, scale = 2^-15 special case of the affine form (in a 16-bit container; the int8 function is given float scale). */
    CHECK_NEAR(dequantize_affine_int8(100, 1.0f / 128, 0), dequantize_q15(100 * 256), 0, "int8 affine with zp 0, scale 2^-7 equals the Q15 value of the left-justified byte");
    TEST_DONE("test_A_adc_dequant");
}
