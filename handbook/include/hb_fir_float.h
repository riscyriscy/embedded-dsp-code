/* Source: Embedded DSP Handbook, Chapter 2 (section 1, FIR Filters + "Exploiting symmetry for speed").
 * Float FIR, FIR_TAPS = 32.  NOTE: Chapter 5 redefines FIR_TAPS (19, Q15) -- never include hb_fir_float.h and hb_fir_q15.h together. */
#ifndef HB_FIR_FLOAT_H
#define HB_FIR_FLOAT_H

#include "hb_qformat.h"

#define FIR_TAPS 32

typedef struct {
    float coeffs[FIR_TAPS];
    float history[FIR_TAPS];
    int idx;
} fir_filter_t;

/* ---- function prototypes / shared state ---- */
void fir_init(fir_filter_t *f, const float *coeffs);
float fir_process(fir_filter_t *f, float input);
float fir_process_symmetric(fir_filter_t *f, float input);

#endif /* HB_FIR_FLOAT_H */
