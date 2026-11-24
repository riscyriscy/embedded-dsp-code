/* Source: Embedded DSP Handbook, Chapter 11 (2. Symbol timing recovery): timing_loop_t, timing_loop_update().
 * MIT License. */
#include "hb_timing_loop.h"

// error is gardner_error()'s output, positive when the sampling instant is
// late. Returns the adjustment to ADD to t->mu (larger mu is later), so it is
// minus the PI output: late means move earlier. The Q16 products are far
// wider than 32 bits, so they are formed in 64 bits; adding half before the
// shift rounds to nearest (a bare >> 16 floors and biases the integrator
// downward).
int32_t timing_loop_update(timing_loop_t *l, int32_t error) {
    int64_t di = (int64_t)l->ki * error + (1 << 15);
    int64_t dp = (int64_t)l->kp * error + (1 << 15);
    l->integrator += (int32_t)(di >> 16);
    return -((int32_t)(dp >> 16) + l->integrator);   // added to t->mu
}
