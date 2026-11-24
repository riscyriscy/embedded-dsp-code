/*
 * Embedded DSP Field Manual, Chapter 6: Loop Tuning and Bring-Up / Bring-up order: AGC, then Gardner, then Costas
 * Listing 40 of 61 (index 39, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_lock.h src/fm_lock.c | Checked by: tests/test_nco_qpsk.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
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

bool lock_detector_update(lock_detector_t *ld, int32_t error) {
    int32_t abs_error = (error < 0) ? -error : error;
    int32_t diff = abs_error - ld->smoothed_abs_error;
    ld->smoothed_abs_error += (int32_t)(((int64_t)ld->alpha_q15 * diff) >> 15);

    if (ld->smoothed_abs_error < ld->threshold) ld->consecutive_count++;
    else ld->consecutive_count = 0;

    return ld->consecutive_count >= ld->required_count;
}
