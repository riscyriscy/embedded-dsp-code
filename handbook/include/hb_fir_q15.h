/* Source: Embedded DSP Handbook, Chapter 2 ("Closing the loop: exporting coefficients").
 * Q15 FIR, FIR_TAPS = 19 from tools/design_filter.py -> include/generated/coeffs.h. */
#ifndef HB_FIR_Q15_H
#define HB_FIR_Q15_H

#include "hb_qformat.h"
#include "generated/coeffs.h"

// The Q15 counterpart to Chapter 1's fir_filter_t: coefficients and history are native
// int16_t, matching exactly what design_filter.py exports, with the wider accumulator
// appearing only inside fir_process_q15 itself, never in the stored state.
typedef struct {
    int16_t coeffs[FIR_TAPS];
    int16_t history[FIR_TAPS];
    int idx;
} fir_q15_t;

/* ---- function prototypes / shared state ---- */
void fir_init_q15(fir_q15_t *f, const int16_t *coeffs);
int16_t fir_process_q15(fir_q15_t *f, int16_t input);

#endif /* HB_FIR_Q15_H */
