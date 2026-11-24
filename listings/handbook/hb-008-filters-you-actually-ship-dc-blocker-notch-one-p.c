/*
 * Embedded DSP Handbook, Chapter 2: FIR and IIR Filters / Filters you actually ship: DC blocker, notch, one-pole LPF
 * Listing 9 of 119 (index 8, counting from 0 in book order), as printed in the 1.0 edition of the book.
 * Status: tested | Built as: include/hb_filters.h src/hb_filters.c | Checked by: tests/test_A_filters.c: DC blocker removes positive and negative DC to zero (vs double-precision reference); notch null/bandwidth/settling; one-pole step response
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 * In the book (November 2026), one_pole_lpf_process is replaced by a comment that points to Field Manual listing fm-013. The text below is the full listing, kept so the compiled module still matches it.
 */
// DC blocker: removes DC offset (sensor bias, mixer LO leakage) while passing everything else
// through with minimal distortion. R close to 1.0 (0.995-0.9997 depending on sample rate) sets
// how low a frequency counts as "DC" to be removed; closer to 1.0 = lower cutoff, slower settling.
typedef struct { int32_t prev_x; int64_t y_q15; int32_t r_q15; } dc_blocker_t;

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

// One-pole lowpass: the simplest possible smoothing filter, one multiply-add, no design tool
// needed. alpha (Q15, 0..32768) sets the cutoff: smaller alpha = more smoothing, slower response.
// 32768 (alpha=1.0, no smoothing at all) is allowed ONLY because alpha_q15 below is int32_t,
// a wide holder, not the packed int16_t Q15 container Appendix D's "1.0 stored as 32768" bug
// warns about; the same value would overflow if this field were ever narrowed to int16_t.
// This is the same shape of filter the AGC and CIC-adjacent envelope estimators use elsewhere
// in this book, worth naming explicitly since it's this common.
// The update floors, so a rising step stops up to about 1/alpha counts short
// of its target (50 for alpha = 0.02, 1 for alpha = 0.8) while a falling step
// lands exactly; for small alpha, keep extra fractional bits in the state as
// dc_blocker_t does.
typedef struct { int32_t state; int32_t alpha_q15; } one_pole_lpf_t;

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
