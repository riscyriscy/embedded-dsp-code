/* Source: Embedded DSP Handbook, Chapter 11 (AGC: as code, not a footnote).
 * agc_t, agc_update(), agc_apply().  MIT License. */
#ifndef HB_AGC_H
#define HB_AGC_H

#include <stdint.h>

typedef struct {
    int32_t gain_q15;       // current gain; this container is a wide int32_t, not a packed
                             // int16_t, specifically so gain can exceed 1.0 (up to 16x below),
                             // so 1.0 = 32768 (2^15) exactly, kept as a power of two so the
                             // >> 15 rescale in agc_apply below stays a plain shift. A packed
                             // int16_t Q15 value would instead cap at 32767 and couldn't
                             // represent gains above 1.0 at all.
    int32_t target_power;   // desired average signal power (same units as measured_power)
    int32_t attack_shift;   // fast response when the signal gets too strong (cut gain quickly, anti-clip)
    int32_t release_shift;  // slow response when the signal gets weak (raise gain slowly, anti-pumping)
} agc_t;

/* ---- function prototypes / shared state ---- */
int32_t agc_update(agc_t *a, int32_t measured_power);
int32_t agc_apply(int32_t sample, int32_t gain_q15);

#endif /* HB_AGC_H */
