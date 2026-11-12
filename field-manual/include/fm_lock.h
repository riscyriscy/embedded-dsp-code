/* Lock detector.
 * Source: Field Manual, Chapter 6. */
#ifndef FM_LOCK_H
#define FM_LOCK_H

#include "fm_qformat.h"

// Smoothed |error| below a threshold for N consecutive symbols - the actual test behind "gate on
// that stage's own lock detector" above. Same shape for Gardner (timing_error) and Costas
// (phase_error); only threshold/alpha/N differ, since the two loops settle on different timescales
// and their errors live in different units.
typedef struct {
    int32_t smoothed_abs_error;   // one-pole smoothed |error|, same Q-format as the error it tracks
    int32_t alpha_q15;             // smoothing coefficient - one_pole_lpf_t's own update, inlined
    int32_t threshold;
    int     consecutive_count;
    int     required_count;        // N - smoothed samples in a row before declaring lock
} lock_detector_t;

/* ---- function prototypes / shared state ---- */
bool lock_detector_update(lock_detector_t *ld, int32_t error);

#endif /* FM_LOCK_H */
