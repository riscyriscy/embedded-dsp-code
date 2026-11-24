/* Source: Embedded DSP Handbook, Chapter 11 (AGC: as code, not a footnote).
 * agc_t, agc_update(), agc_apply().  MIT License. */
#include "hb_agc.h"

// Called once per sample (or once per block, with measured_power averaged over the block).
// measured_power is the power of the signal AFTER agc_apply() has scaled it:
// the loop must see the effect of its own gain, or there is no feedback.
// target_power must be > 0.
// Returns the updated gain to apply to the next sample/block.
int32_t agc_update(agc_t *a, int32_t measured_power) {
    int64_t error = (int64_t)a->target_power - measured_power;
    // error < 0 means measured_power exceeds target (signal too strong): attack, cut gain fast.
    // error >= 0 means measured_power is below target (signal weak): release, raise gain slowly.
    int32_t shift = (error < 0) ? a->attack_shift : a->release_shift;
    // Shifting a negative error directly (error >> shift) rounds asymmetrically: C's >> on a
    // negative signed value floors toward negative infinity, so a small negative error like -1
    // never rounds down to a 0 update - gain keeps decrementing by at least 1 every call even
    // after the loop has essentially converged, while an equally small positive error floors to
    // 0 and produces no update at all. Shifting the magnitude and reapplying the sign gives the
    // same symmetric deadband on both sides, so the loop can actually settle.
    int64_t magnitude = (error < 0) ? -error : error;
    // The error is a POWER (Q30 for Q15 samples) but the gain is Q15, so turn
    // it into a gain step first. Power goes as gain squared, so near the
    // target the relative gain error is half the relative power error: step =
    // gain * |error| / (2 * target). Without this the step would depend on
    // the absolute target level, and the shift would not be a time constant.
    int64_t delta = magnitude * a->gain_q15 / (2 * (int64_t)a->target_power);
    // The shift now sets the time constant, about 2^shift samples. It rounds
    // to nearest, so the last few Q15 steps of a slow release are not
    // truncated away.
    delta = (delta + ((1LL << shift) >> 1)) >> shift;
    int64_t gain = a->gain_q15 + ((error < 0) ? -delta : delta);
    if (gain < 1) gain = 1;                // never quite zero or negative
    // cap max gain (avoid noise-only runaway)
    if (gain > 32768 * 16) gain = 32768 * 16;
    a->gain_q15 = (int32_t)gain;
    return a->gain_q15;
}

// Apply the current gain to an incoming sample before it reaches the rest of the chain.
int32_t agc_apply(int32_t sample, int32_t gain_q15) {
    return (int32_t)(((int64_t)sample * gain_q15) >> 15);
}
