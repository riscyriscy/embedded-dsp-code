/* Source: Embedded DSP Handbook, Chapter 12 (Pull-in range vs. pull-in time).
 * coarse_freq_estimate_hz(): delay-and-conjugate-multiply frequency estimate over a known preamble.
 * MIT License. */
#ifndef HB_COARSE_FREQ_H
#define HB_COARSE_FREQ_H

#include "hb_complex.h"
#include "hb_preamble.h"   /* PREAMBLE_LEN */

/* ---- function prototypes / shared state ---- */
float coarse_freq_estimate_hz(const complex_t *preamble_iq, int lag, float fs_hz);

#endif /* HB_COARSE_FREQ_H */
