/* DC blocker, one-pole lowpass, notch designer, rounded Q15 multiply.
 * Source: Field Manual, Chapter 2. */
#include "fm_simple_filters.h"

int32_t dc_blocker_process(dc_blocker_t *f, int32_t x) {
    int64_t y = (int64_t)(x - f->prev_x) * 32768       // input step, Q15
              + (((int64_t)f->r_q15 * f->prev_y) >> 15);
    f->prev_x = x;
    f->prev_y = y;
    return (int32_t)((y + (1 << 14)) >> 15);   // round to the integer output
}

int32_t one_pole_lpf_process(one_pole_lpf_t *f, int32_t x) {
    int32_t diff = x - f->state;
    f->state += (int32_t)(((int64_t)f->alpha_q15 * diff) >> 15);
    return f->state;
}

// Notch: a biquad tuned to null one frequency - the standard fix for 50/60 Hz mains
// hum. Higher Q rejects more precisely but settles slower and is pickier about the
// exact frequency being right. Same biquad_q15_t this chapter already ships (above) -
// this just computes a different set of coefficients for it, in Q2.14, the same
// FLOAT_TO_Q2_14 quantization as the Butterworth example above.
void design_notch_biquad(biquad_q15_t *bq, float f0_hz, float fs_hz, float q) {
    float w0 = 2.0f * 3.14159265f * f0_hz / fs_hz;
    float alpha = sinf(w0) / (2.0f * q);
    float cos_w0 = cosf(w0);
    float a0 = 1.0f + alpha;
    bq->b0 = FLOAT_TO_Q2_14(1.0f / a0);
    bq->b1 = FLOAT_TO_Q2_14((-2.0f * cos_w0) / a0);
    bq->b2 = FLOAT_TO_Q2_14(1.0f / a0);
    bq->a1 = FLOAT_TO_Q2_14((-2.0f * cos_w0) / a0);
    bq->a2 = FLOAT_TO_Q2_14((1.0f - alpha) / a0);
    bq->z1 = bq->z2 = 0;
}

int32_t q15_mul_round(int32_t a, int32_t b) {
    int64_t product = (int64_t)a * b;
    return (int32_t)((product + (1 << 14)) >> 15);
}
