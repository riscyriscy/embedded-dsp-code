/* Source: Embedded DSP Handbook, Chapter 8 (5. Equalization).
 * lms_equalizer_t, lms_update(), lms_filter(), lms_compute_error().  MIT License. */
#ifndef HB_LMS_EQ_H
#define HB_LMS_EQ_H

#include <stdint.h>

#define EQ_TAPS 8   // equalizer length in symbol-spaced taps

typedef struct {
    int32_t weights_i[EQ_TAPS], weights_q[EQ_TAPS];  // complex weights, real/imag parts
    int32_t history_i[EQ_TAPS], history_q[EQ_TAPS];
    int32_t mu;  // step size: an integer, the real step is mu / 2^31
} lms_equalizer_t;

/* ---- function prototypes / shared state ---- */
void lms_update(lms_equalizer_t *eq, int32_t error_i, int32_t error_q);
void lms_filter(lms_equalizer_t *eq, int32_t input_i, int32_t input_q, int32_t *out_i, int32_t *out_q);
void lms_compute_error(int32_t out_i, int32_t out_q, int32_t decided_i, int32_t decided_q, int32_t *error_i, int32_t *error_q);

#endif /* HB_LMS_EQ_H */
