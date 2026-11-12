/* Source: Embedded DSP Handbook, Chapter 6 (section 3, FM demodulation via I/Q).
 * Quadrature FM discriminator (cross product, no atan2). */
#ifndef HB_FM_DEMOD_H
#define HB_FM_DEMOD_H

#include <stdint.h>

// Complex sample: I(t), Q(t) at time n and n-1
// FM demod output proportional to d(phase)/dt, computed without an atan2 call:
//   output[n] = I[n-1]*Q[n] - Q[n-1]*I[n]   (approximates sin(delta_phase), good for small delta_phase)
// This is the "quadrature FM demodulator" used in essentially every analog FM SDR receiver.

typedef struct {
    int32_t i_prev, q_prev;
} fm_demod_t;

/* ---- function prototypes / shared state ---- */
int32_t fm_demod_process(fm_demod_t *d, int32_t i, int32_t q);

#endif /* HB_FM_DEMOD_H */
