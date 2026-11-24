/* Source: Embedded DSP Handbook, Chapter 2 (section 2, IIR Filters; limit-cycle section; "Filters you actually ship").
 * Float biquad (DF2T), q15_mul_round, DC blocker, one-pole LPF, notch designer. */
#ifndef HB_FILTERS_H
#define HB_FILTERS_H

#include "hb_qformat.h"
#include <math.h>

typedef struct {
    float b0, b1, b2;  // feedforward coeffs
    float a1, a2;      // feedback coeffs (a0 normalized to 1)
    float z1, z2;      // state (Direct Form II Transposed)
} biquad_t;

// DC blocker: removes DC offset (sensor bias, mixer LO leakage) while passing everything else
// through with minimal distortion. R close to 1.0 (0.995-0.9997 depending on sample rate) sets
// how low a frequency counts as "DC" to be removed; closer to 1.0 = lower cutoff, slower settling.
typedef struct { int32_t prev_x; int64_t y_q15; int32_t r_q15; } dc_blocker_t;

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

/* ---- function prototypes / shared state ---- */
float biquad_process(biquad_t *bq, float input);
int32_t q15_mul_round(int32_t a, int32_t b);
int32_t dc_blocker_process(dc_blocker_t *f, int32_t x);
int32_t one_pole_lpf_process(one_pole_lpf_t *f, int32_t x);
void design_notch_biquad(biquad_t *bq, float f0_hz, float fs_hz, float q);

#endif /* HB_FILTERS_H */
