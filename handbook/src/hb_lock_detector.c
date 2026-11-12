/* Source: Embedded DSP Handbook, Chapter 9 (Lock detectors and cycle slips).
 * lock_detector_t, lock_detector_update().  MIT License. */
#include "hb_lock_detector.h"

uint8_t lock_detector_update(lock_detector_t *d, int32_t loop_error) {
    int32_t mag = (loop_error < 0) ? -loop_error : loop_error;
    // Simple exponential average, avg += (mag - avg) / 16, carried as acc =
    // 16 * avg. A bare (mag - avg) >> 4 floors, so every upward step smaller
    // than 16 counts is lost and the average reads low; the extra fraction
    // bits keep them.
    d->error_magnitude_acc += mag - (d->error_magnitude_acc >> 4);
    d->error_magnitude_avg = d->error_magnitude_acc >> 4;

    if (d->error_magnitude_avg < d->lock_threshold) {
        d->unlock_dwell_counter = 0;         // a good sample resets the *other* side's dwell
        if (!d->locked && ++d->lock_dwell_counter >= d->lock_dwell_target)
            d->locked = 1;
    } else {
        d->lock_dwell_counter = 0;           // a bad sample resets the *other* side's dwell
        if (d->locked && ++d->unlock_dwell_counter >= d->unlock_dwell_target)
            d->locked = 0;
    }
    return d->locked;
}
