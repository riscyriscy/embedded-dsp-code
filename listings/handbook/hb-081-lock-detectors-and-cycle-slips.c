/*
 * Embedded DSP Handbook, Chapter 12: Loop-Filter Tuning for Costas and Gardner Loops / Lock detectors and cycle slips
 * Listing 82 of 119 (index 81, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_lock_detector.h src/hb_lock_detector.c | Checked by: tests/test_C_ch9_lock.c: dwell count, extra-fraction-bit average, lock/unlock thresholds
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
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
