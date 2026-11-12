/* Gardner TED, linear interpolator, timing loop filter.
 * Source: Field Manual, Chapter 5. */
#include "fm_timing.h"

// mid, curr from a linear (or better, Farrow) interpolator at mu and mu+0.5 symbols.
int32_t gardner_error(gardner_ted_t *t, int32_t mid, int32_t curr) {
    int64_t err = (int64_t)(curr - t->prev_sample) * mid;   // classic Gardner TED
    t->prev_sample = curr;
    return (int32_t)(err >> 16);
}

// Applies fractional offset mu (Q15) between two adjacent oversampled buffer entries.
int32_t timing_interpolate_linear(int32_t sample_a, int32_t sample_b, int32_t mu_q15) {
    int64_t diff = (int64_t)(sample_b - sample_a) * mu_q15;
    return sample_a + (int32_t)(diff >> 15);
}

int32_t timing_loop_update(timing_loop_t *l, int32_t error) {
    l->integrator += (l->ki * error) >> 16;
    return ((l->kp * error) >> 16) + l->integrator;   // adjustment added to t->mu
}
