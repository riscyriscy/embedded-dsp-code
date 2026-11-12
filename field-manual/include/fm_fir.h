/* Q15 FIR (direct and symmetric) and zero-stuff interpolator.
 * Source: Field Manual, Chapter 2 (FIR) and Chapter 4 (interpolate_by_L).
 * Coefficients come from tools/design_filter.py via include/generated/coeffs.h. */
#ifndef FM_FIR_H
#define FM_FIR_H

#include "fm_qformat.h"
#include "generated/coeffs.h"

// Q15 runtime type - native int16_t, matching what design_filter.py exports.
typedef struct {
    int16_t coeffs[FIR_TAPS];
    int16_t history[FIR_TAPS];
    int idx;
} fir_q15_t;

/* ---- function prototypes / shared state ---- */
void fir_init_q15(fir_q15_t *f, const int16_t *coeffs);
int16_t fir_process_q15(fir_q15_t *f, int16_t input);
int16_t fir_process_symmetric_q15(fir_q15_t *f, int16_t input);
void interpolate_by_L(fir_q15_t *f, int16_t input, int L, int16_t *outputs);

#endif /* FM_FIR_H */
