/* Source: Embedded DSP Handbook, Chapter 2 (A fixed-point biquad: why Q1.15 coefficients don't work).
 * Q2.14 biquad, DF2T, wide state. */
#include "hb_biquad_q15.h"

int16_t biquad_process_q15(biquad_q15_t *bq, int16_t input) {
    int32_t out_wide = q15_wide_add_sat(q2_14_mul_round(bq->b0, input), bq->z1);
    int16_t output;
    if (out_wide > 32767) output = 32767;
    else if (out_wide < -32768) output = -32768;
    else output = (int16_t)out_wide;

    bq->z1 = q15_wide_add_sat(q2_14_mul_round(bq->b1, input) - q2_14_mul_round(bq->a1, output), bq->z2);
    bq->z2 = q2_14_mul_round(bq->b2, input) - q2_14_mul_round(bq->a2, output);

    return output;
}
