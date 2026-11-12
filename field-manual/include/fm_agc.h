/* AGC update/apply.
 * Source: Field Manual, Chapter 5. */
#ifndef FM_AGC_H
#define FM_AGC_H

#include "fm_qformat.h"

typedef struct {
    int32_t gain_q15;       // WIDE: 1.0 = 32768 exactly, gain can exceed 1.0 (up to 16x below)
    int32_t target_power;   // Q30 OUTPUT power; full scale squared = 1 << 30
    int32_t attack_shift;   // fast response when signal gets too strong (anti-clip) - smaller
                              // shift = faster; Chapter 10's loopback starts at 4
    int32_t release_shift;  // slow response when signal gets weak (anti-pumping) - larger
                              // shift = slower; Chapter 10's loopback starts at 8
} agc_t;

// agc_apply's return is deliberately wide and unsaturated (gain_q15 above can run up to 16x), so
// its result can exceed int16 range even mid-convergence, well before the loop has pumped anything
// visibly out of control. A bare (int16_t) cast at the call site would silently wrap instead of
// clip - the same class of bug mixer_to_q15() below already guards against one stage later.
static inline int16_t agc_sample_to_q15(int32_t wide_sample) {
    if (wide_sample > 32767) return 32767;
    if (wide_sample < -32768) return -32768;
    return (int16_t)wide_sample;
}

/* ---- function prototypes / shared state ---- */
int32_t agc_update(agc_t *a, int32_t measured_power);
int32_t agc_apply(int32_t sample, int32_t gain_q15);

#endif /* FM_AGC_H */
