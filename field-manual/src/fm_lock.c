/* Lock detector.
 * Source: Field Manual, Chapter 6. */
#include "fm_lock.h"

bool lock_detector_update(lock_detector_t *ld, int32_t error) {
    int32_t abs_error = (error < 0) ? -error : error;
    int32_t diff = abs_error - ld->smoothed_abs_error;
    ld->smoothed_abs_error += (int32_t)(((int64_t)ld->alpha_q15 * diff) >> 15);

    if (ld->smoothed_abs_error < ld->threshold) ld->consecutive_count++;
    else ld->consecutive_count = 0;

    return ld->consecutive_count >= ld->required_count;
}
