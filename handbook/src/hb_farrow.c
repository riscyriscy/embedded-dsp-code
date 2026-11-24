/* Source: Embedded DSP Handbook, Chapter 11 (2. Symbol timing recovery): cubic Farrow interpolator (float) and its fixed-point version.
 * MIT License. */
#include "hb_farrow.h"
#include "generated/farrow_weights.h"

// Cubic Farrow-style interpolator (four-point Lagrange interpolation): uses one sample on
// either side of the linear version's two, for a meaningfully better fractional-delay fit.
// x_m1, x_0, x_1, x_2 are four consecutive samples; mu in [0,1) is the fractional offset
// between x_0 and x_1 (mu=0 returns x_0, mu approaching 1 approaches x_1).
int32_t timing_interpolate_farrow_cubic(int32_t x_m1, int32_t x_0, int32_t x_1, int32_t x_2,
                                          int32_t mu_q15) {
    // Work in float internally for the polynomial evaluation; a fixed-point production version
    // would precompute these Lagrange weights as Q15 constants per mu step instead.
    float mu = mu_q15 / 32768.0f;
    float p_m1 = -mu * (mu - 1.0f) * (mu - 2.0f) / 6.0f;
    float p_0  =  (mu + 1.0f) * (mu - 1.0f) * (mu - 2.0f) / 2.0f;
    float p_1  = -(mu + 1.0f) * mu * (mu - 2.0f) / 2.0f;
    float p_2  =  (mu + 1.0f) * mu * (mu - 1.0f) / 6.0f;
    float y = x_m1 * p_m1 + x_0 * p_0 + x_1 * p_1 + x_2 * p_2;
    return (int32_t)y;
}

int32_t timing_interpolate_farrow_cubic_q15(int32_t x_m1, int32_t x_0, int32_t x_1, int32_t x_2,
                                              int32_t mu_q15) {
    // Truncate mu to the table step at or below it: the timing resolution is
    // 1/FARROW_MU_STEPS of a sample. (Rounding to nearest would need one more
    // row, for mu = 1.0.)
    int step = (mu_q15 * FARROW_MU_STEPS) >> 15;
    if (step >= FARROW_MU_STEPS) step = FARROW_MU_STEPS - 1;
    const int16_t *w = farrow_weights_q15[step];
    int64_t acc = (int64_t)x_m1 * w[0] + (int64_t)x_0 * w[1]
                + (int64_t)x_1 * w[2] + (int64_t)x_2 * w[3];
    return (int32_t)(acc >> 15);
}
