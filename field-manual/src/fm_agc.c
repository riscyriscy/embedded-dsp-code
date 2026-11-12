/* AGC update/apply.
 * Source: Field Manual, Chapter 5. */
#include "fm_agc.h"

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
