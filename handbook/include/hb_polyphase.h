/* Source: Embedded DSP Handbook, Chapter 5 (Decimation: reducing the sample rate), polyphase.
 * Code from the Embedded DSP Handbook; MIT License.
 * Polyphase decimate-by-M (POLY_M = 4, 8 taps per phase) and the coefficient loader polyphase_init(). */
#ifndef HB_POLYPHASE_H
#define HB_POLYPHASE_H

#include <stdint.h>   /* host build */

// Polyphase decimate-by-M: split an N-tap prototype filter's coefficients into M sub-filters,
// phase p holding h[M-1-p], h[M-1-p+M], h[M-1-p+2M], ... Each of the M sub-filters gets its
// OWN delay line, fed by a commutator that routes each incoming sample to exactly one phase in
// round-robin order; every M inputs, summing all M phases' partial outputs together produces one
// decimated output sample. This is what actually gives the 1/M multiply-accumulate saving over
// filtering every sample at the full N-tap cost and then discarding M-1 out of every M outputs,
// on average: most calls (the history-shift-only ones) cost nothing, one call in every M pays
// the full N-multiply combine, averaging to L = N/M multiplies per input over the whole cycle.
#define POLY_M 4          // decimation factor

#define POLY_TAPS_PER_PHASE 8   // L = N/M taps per phase, N = POLY_M * POLY_TAPS_PER_PHASE total

typedef struct {
    int32_t coeffs[POLY_M][POLY_TAPS_PER_PHASE];   // coeffs[phase][tap], precomputed once at init
    int32_t history[POLY_M][POLY_TAPS_PER_PHASE];  // one independent delay line PER PHASE
    int phase;                                      // which phase the next input commutates to
} polyphase_decim_t;

/* ---- function prototypes / shared state ---- */
void polyphase_init(polyphase_decim_t *f, const int32_t *h);
int32_t polyphase_decimate(polyphase_decim_t *f, int32_t input, int *got_output);

#endif /* HB_POLYPHASE_H */
