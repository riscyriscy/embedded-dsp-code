/* Source: Embedded DSP Handbook, Chapter 9 (Lock detectors and cycle slips).
 * lock_detector_t, lock_detector_update().  MIT License. */
#ifndef HB_LOCK_DETECTOR_H
#define HB_LOCK_DETECTOR_H

#include <stdint.h>

typedef struct {
    int32_t error_magnitude_avg;   // running average of |loop error|, Q-format
    int32_t error_magnitude_acc;   // the average times 16 (4 fraction bits)
    int32_t lock_threshold;
    int32_t lock_dwell_counter;
    int32_t lock_dwell_target;     // consecutive good samples required before declaring "locked"
    int32_t unlock_dwell_counter;
    int32_t unlock_dwell_target;   // consecutive bad samples required before declaring "unlocked"
    uint8_t locked;
} lock_detector_t;

/* ---- function prototypes / shared state ---- */
uint8_t lock_detector_update(lock_detector_t *d, int32_t loop_error);

#endif /* HB_LOCK_DETECTOR_H */
