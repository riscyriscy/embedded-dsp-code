/*
 * Embedded DSP Field Manual, Chapter 5: I/Q and the Demod Chain / AGC: as code, not a footnote
 * Listing 29 of 57 (index 28, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_agc.h src/fm_agc.c | Checked by: tests/test_nco_qpsk.c (step units); closed loop: tests/test_chain.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
typedef struct {
    int32_t gain_q15;       // WIDE: 1.0 = 32768 exactly, gain can exceed 1.0 (up to 16x below)
    int32_t target_power;   // Q30 OUTPUT power; full scale squared = 1 << 30
    int32_t attack_shift;   // fast response when signal gets too strong (anti-clip) - smaller
                              // shift = faster; Chapter 10's loopback starts at 4
    int32_t release_shift;  // slow response when signal gets weak (anti-pumping) - larger
                              // shift = slower; Chapter 10's loopback starts at 8
} agc_t;

int32_t agc_update(agc_t *a, int32_t measured_power) {
    // Powers are Q30, the gain is Q15: the >> 15 below brings the power error
    // into gain units, and the shift then sets the speed. measured_power must
    // come from the sample AFTER agc_apply() - measured on the input there is
    // no feedback, and the gain just ramps to a rail.
    int32_t error = a->target_power - measured_power;
    int32_t shift = (error < 0) ? a->attack_shift : a->release_shift;
    // Shift the MAGNITUDE and reapply sign - shifting a negative error directly floors
    // toward -infinity, so a tiny negative error never rounds to a 0 update while an
    // equally tiny positive one does, and the loop never actually settles.
    int32_t magnitude = (error < 0) ? -error : error;
    int32_t delta = (magnitude >> 15) >> shift;
    a->gain_q15 += (error < 0) ? -delta : delta;
    if (a->gain_q15 < 1) a->gain_q15 = 1;
    if (a->gain_q15 > (32768 * 16)) a->gain_q15 = 32768 * 16;
    return a->gain_q15;
}

int32_t agc_apply(int32_t sample, int32_t gain_q15) {
    return (int32_t)(((int64_t)sample * gain_q15) >> 15);
}

// agc_apply's return is deliberately wide and unsaturated (gain_q15 above can run up to 16x), so
// its result can exceed int16 range even mid-convergence, well before the loop has pumped anything
// visibly out of control. A bare (int16_t) cast at the call site would silently wrap instead of
// clip - the same class of bug mixer_to_q15() below already guards against one stage later.
static inline int16_t agc_sample_to_q15(int32_t wide_sample) {
    if (wide_sample > 32767) return 32767;
    if (wide_sample < -32768) return -32768;
    return (int16_t)wide_sample;
}
