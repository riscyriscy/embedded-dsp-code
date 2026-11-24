/* Source: Embedded DSP Handbook, Chapter 11 (2. Symbol timing recovery: finding when to sample).
 * gardner_ted_t, gardner_error(), timing_interpolate_linear().  MIT License. */
#ifndef HB_TIMING_H
#define HB_TIMING_H

#include <stdint.h>

typedef struct {
    int32_t mu;           // fractional timing offset, Q-format (0..1 within a sample period)
    int32_t prev_sample;  // previous symbol-spaced sample
    int32_t mid_sample;   // the "half-symbol" sample between decisions
} gardner_ted_t;  // TED = timing error detector

/* ---- function prototypes / shared state ---- */
int32_t gardner_error(gardner_ted_t *t, int32_t mid, int32_t curr);
int32_t timing_interpolate_linear(int32_t sample_a, int32_t sample_b, int32_t mu_q15);

#endif /* HB_TIMING_H */
