/* Source: Embedded DSP Handbook, Chapter 2 (section 2, IIR Filters; limit-cycle section; "Filters you actually ship").
 * Float biquad (DF2T), q15_mul_round, DC blocker, one-pole LPF, notch designer. */
#include "hb_filters.h"

float biquad_process(biquad_t *bq, float input) {
    float output = bq->b0 * input + bq->z1;
    bq->z1 = bq->b1 * input - bq->a1 * output + bq->z2;
    bq->z2 = bq->b2 * input - bq->a2 * output;
    return output;
}

// q15_mul_trunc, defined back in Chapter 1's Q-format basics, is exactly the truncating
// shift with the bias problem described above. Rounding removes that bias with
// one extra add before the same shift, giving zero-mean error. It does not by
// itself stop limit cycles (see the discussion above):
int32_t q15_mul_round(int32_t a, int32_t b) {
    int64_t product = (int64_t)a * b;
    return (int32_t)((product + (1 << 14)) >> 15);
}

// y[n] = x[n] - x[n-1] + R*y[n-1]. The feedback state y_q15 = y * 2^15 keeps
// 15 extra fractional bits; only the returned value is rounded to an integer.
// Flooring R*y to whole counts every sample would bias it by -1/2 LSB, which
// the loop amplifies by 1/(1-R) (200 for R = 0.995): about -100 counts of DC
// on the output, and a dead band that never removes a small negative offset.
// The int64_t state cannot overflow for any int32_t input (|y| <= 2*max|x|).
int32_t dc_blocker_process(dc_blocker_t *f, int32_t x) {
    int64_t dx = (int64_t)x - f->prev_x;
    f->y_q15 = dx * 32768 + (((int64_t)f->r_q15 * f->y_q15) >> 15);
    f->prev_x = x;
    // The output stays in the wide int32_t container (never narrowed here), so
    // Step 8's saturate-don't-wrap rule doesn't apply inside this function
    // itself; it applies wherever a caller later narrows this back to a packed
    // int16_t.
    return (int32_t)((f->y_q15 + 16384) >> 15);   // round to nearest
}

int32_t one_pole_lpf_process(one_pole_lpf_t *f, int32_t x) {
    int32_t diff = x - f->state;
    f->state += (int32_t)(((int64_t)f->alpha_q15 * diff) >> 15);
    return f->state;   // same wide-container reasoning as dc_blocker_process above: nothing
                        // narrows here, so there's nothing to saturate yet.
}

// Notch: a biquad (this chapter's earlier Direct Form II Transposed structure) with coefficients
// chosen to null one specific frequency, the standard fix for mains hum (50/60 Hz) riding on
// a sensor signal. Q sets how narrow the notch is; higher Q rejects the target frequency more
// precisely but takes longer to settle and is more sensitive to the exact frequency being right.
void design_notch_biquad(biquad_t *bq, float f0_hz, float fs_hz, float q) {
    float w0 = 2.0f * 3.14159265f * f0_hz / fs_hz;
    float alpha = sinf(w0) / (2.0f * q);
    float cos_w0 = cosf(w0);
    float a0 = 1.0f + alpha;
    bq->b0 = 1.0f / a0;
    bq->b1 = (-2.0f * cos_w0) / a0;
    bq->b2 = 1.0f / a0;
    bq->a1 = (-2.0f * cos_w0) / a0;
    bq->a2 = (1.0f - alpha) / a0;
    bq->z1 = bq->z2 = 0.0f;
}
