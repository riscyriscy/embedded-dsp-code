/*
 * Embedded DSP Field Manual, Chapter 2: Filters You Ship / Three filters that don't need a design tool
 * Listing 14 of 61 (index 13, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_simple_filters.h src/fm_simple_filters.c | Checked by: tests/test_filters.c (DC blocker removes positive and negative DC; vs double-precision reference)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// DC blocker: removes DC offset (sensor bias, mixer LO leakage). R close to 1.0
// (0.995-0.9997) sets the cutoff - closer to 1.0 = lower cutoff, slower settling.
// prev_y is the previous output with 15 extra fraction bits. Held as a plain
// integer, the feedback product would be rounded down by up to 1 LSB per
// sample, and the loop's 1/(1-R) gain (200x at R = 0.995) would turn that
// into a dead band that never removes a negative DC offset.
typedef struct { int32_t prev_x; int64_t prev_y; int32_t r_q15; } dc_blocker_t;

int32_t dc_blocker_process(dc_blocker_t *f, int32_t x) {
    int64_t y = (int64_t)(x - f->prev_x) * 32768       // input step, Q15
              + (((int64_t)f->r_q15 * f->prev_y) >> 15);
    f->prev_x = x;
    f->prev_y = y;
    return (int32_t)((y + (1 << 14)) >> 15);   // round to the integer output
}

// One-pole lowpass: one multiply-add, no design tool. alpha (Q15) sets the cutoff -
// smaller alpha = more smoothing.
typedef struct { int32_t state; int32_t alpha_q15; } one_pole_lpf_t;

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
