/* Two Q15 edge cases from Chapter 12: -32768 x -32768, and a right shift of a negative value. */
#include <stdio.h>
#include <stdint.h>
#include "fm_qformat.h"

_Static_assert((-1 >> 1) == -1, "this code needs an arithmetic right shift of negative values");

static int32_t q15_mul_sat(int32_t a, int32_t b) {
    int32_t r = q15_mul_trunc(a, b);
    return r > 32767 ? 32767 : r;           /* only -32768 x -32768 reaches 32768 */
}

int main(void) {
    int32_t wide = q15_mul_trunc(-32768, -32768);
    int16_t narrow = (int16_t)wide;
    printf("q15_mul_trunc(-32768, -32768) = %ld, stored in int16_t = %d\n", (long)wide, narrow);
    printf("q15_mul_sat(-32768, -32768)   = %ld\n", (long)q15_mul_sat(-32768, -32768));
    for (int32_t x = -3; x <= 3; x += 2)
        printf("x = %2ld: x >> 1 = %2ld, x / 2 = %2ld\n", (long)x, (long)(x >> 1), (long)(x / 2));
    int64_t sum_x = 0, sum_shift = 0, sum_div = 0, sum_round = 0;
    for (int32_t x = -32768; x <= 32767; x++) {
        sum_x += x; sum_shift += x >> 4; sum_div += x / 16; sum_round += (x + 8) >> 4;
    }
    printf("over all 65536 Q15 values: mean x / 16 = %.4f\n", (double)sum_x / 16.0 / 65536.0);
    printf("  mean of x / 16 = %.4f, of x >> 4 = %.4f, of (x + 8) >> 4 = %.4f\n",
           (double)sum_div / 65536.0, (double)sum_shift / 65536.0, (double)sum_round / 65536.0);
    return 0;
}
