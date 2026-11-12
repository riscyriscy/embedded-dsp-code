/* DC blocker, one-pole lowpass, notch designer, rounded Q15 multiply.
 * Source: Field Manual, Chapter 2. */
#ifndef FM_SIMPLE_FILTERS_H
#define FM_SIMPLE_FILTERS_H

#include "fm_qformat.h"
#include "fm_biquad.h"
#include <math.h>

// DC blocker: removes DC offset (sensor bias, mixer LO leakage). R close to 1.0
// (0.995-0.9997) sets the cutoff - closer to 1.0 = lower cutoff, slower settling.
// prev_y is the previous output with 15 extra fraction bits. Held as a plain
// integer, the feedback product would be rounded down by up to 1 LSB per
// sample, and the loop's 1/(1-R) gain (200x at R = 0.995) would turn that
// into a dead band that never removes a negative DC offset.
typedef struct { int32_t prev_x; int64_t prev_y; int32_t r_q15; } dc_blocker_t;

// One-pole lowpass: one multiply-add, no design tool. alpha (Q15) sets the cutoff -
// smaller alpha = more smoothing.
typedef struct { int32_t state; int32_t alpha_q15; } one_pole_lpf_t;

/* ---- function prototypes / shared state ---- */
int32_t dc_blocker_process(dc_blocker_t *f, int32_t x);
int32_t one_pole_lpf_process(one_pole_lpf_t *f, int32_t x);
void design_notch_biquad(biquad_q15_t *bq, float f0_hz, float fs_hz, float q);
int32_t q15_mul_round(int32_t a, int32_t b);

#endif /* FM_SIMPLE_FILTERS_H */
