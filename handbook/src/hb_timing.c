/* Source: Embedded DSP Handbook, Chapter 11 (2. Symbol timing recovery: finding when to sample).
 * gardner_ted_t, gardner_error(), timing_interpolate_linear().  MIT License. */
#include "hb_timing.h"

// Called once per symbol interval. mid and curr are both produced by the
// interpolator below, at t->mu and t->mu + 0.5 symbols respectively; this
// function does not derive them itself, only combines them into an error.
// Sign convention: mu follows the interpolators below (mu = 0 is the older
// sample, larger mu is LATER in time). With that convention this error is
// POSITIVE when the sampling instant is late and NEGATIVE when it is early,
// so the loop filter moves mu the opposite way. Its slope at lock is the
// detector gain Kd, in error units per symbol of timing error; it scales with
// the signal amplitude squared.
int32_t gardner_error(gardner_ted_t *t, int32_t mid, int32_t curr) {
    t->mid_sample = mid;  // stored for inspection; the error uses the argument directly
    // error = (curr - prev) * mid   - classic Gardner TED
    int64_t err = (int64_t)(curr - t->prev_sample) * mid;
    t->prev_sample = curr;
    return (int32_t)(err >> 16);  // scale to your Q-format
}

// Linear interpolator: the piece that actually applies mu, producing a sample at
// fractional offset mu (Q15, 0..1) between two adjacent oversampled buffer entries.
// A Farrow structure gives better accuracy at more multiplies; this is the minimum
// version that closes the loop end to end. sample_a is the older sample.
int32_t timing_interpolate_linear(int32_t sample_a, int32_t sample_b, int32_t mu_q15) {
    int64_t diff = (int64_t)(sample_b - sample_a) * mu_q15;
    return sample_a + (int32_t)(diff >> 15);
}
