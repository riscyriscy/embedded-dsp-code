/*
 * Embedded DSP Handbook, Chapter 11: Digital Demodulation In Depth / AGC: as code, not a footnote
 * Listing 68 of 119 (index 67, counting from 0 in book order), as printed in the 1.0 edition of the book.
 * Status: tested | Built as: include/hb_agc.h src/hb_agc.c | Checked by: tests/test_C_ch8_agc.c: closed-loop settling to the attack/release balance point, step sizes, rails, clamp
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 * In the book (November 2026), agc_apply is replaced by a comment that points to Field Manual listing fm-028. The text below is the full listing, kept so the compiled module still matches it.
 */
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
