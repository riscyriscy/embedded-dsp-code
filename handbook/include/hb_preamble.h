/* Source: Embedded DSP Handbook, Chapter 8 (Preamble acquisition: finding the packet before locking onto anything).
 * preamble_corr_t, preamble_correlate().  MIT License. */
#ifndef HB_PREAMBLE_H
#define HB_PREAMBLE_H

#include <stdint.h>

// Sliding correlator: continuously compares the last N samples against a known preamble
// waveform, one multiply-accumulate per candidate alignment. Real implementations often use
// an FFT-based correlation for long preambles, this is the direct-form version for a short one.
#define PREAMBLE_LEN 32   // preamble length in samples (the N above)

typedef struct {
    int32_t history[PREAMBLE_LEN];
    int idx;
} preamble_corr_t;

/* ---- function prototypes / shared state ---- */
int32_t preamble_correlate(preamble_corr_t *c, const int32_t *known_preamble, int32_t new_sample);

#endif /* HB_PREAMBLE_H */
