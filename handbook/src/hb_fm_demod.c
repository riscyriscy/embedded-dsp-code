/* Source: Embedded DSP Handbook, Chapter 6 (section 3, FM demodulation via I/Q).
 * Quadrature FM discriminator (cross product, no atan2). */
#include "hb_fm_demod.h"

int32_t fm_demod_process(fm_demod_t *d, int32_t i, int32_t q) {
    // cross product approximates sin(phase delta) for small angles between samples
    int64_t cross = (int64_t)d->i_prev * q - (int64_t)d->q_prev * i;
    d->i_prev = i;
    d->q_prev = q;
    return (int32_t)(cross >> 16);  // scale back down, adjust shift to your Q-format
}
