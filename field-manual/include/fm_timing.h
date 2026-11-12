/* Gardner TED, linear interpolator, timing loop filter.
 * Source: Field Manual, Chapter 5. */
#ifndef FM_TIMING_H
#define FM_TIMING_H

#include "fm_qformat.h"

typedef struct {
    int32_t mu;            // fractional timing offset, Q-format
    int32_t prev_sample;
} gardner_ted_t;

typedef struct { int32_t integrator; int32_t kp, ki; } timing_loop_t;

/* ---- function prototypes / shared state ---- */
int32_t gardner_error(gardner_ted_t *t, int32_t mid, int32_t curr);
int32_t timing_interpolate_linear(int32_t sample_a, int32_t sample_b, int32_t mu_q15);
int32_t timing_loop_update(timing_loop_t *l, int32_t error);

#endif /* FM_TIMING_H */
