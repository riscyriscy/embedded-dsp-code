/* Source: Embedded DSP Handbook, Chapter 5 (Interpolation: increasing the sample rate).
 * Code from the Embedded DSP Handbook; MIT License.
 * Zero-stuff by L and filter with Chapter 2's fir_q15_t (FIR_TAPS = 19). Never link together with hb_fir_float / hb_decimate. */
#ifndef HB_INTERPOLATE_H
#define HB_INTERPOLATE_H

#include <stdint.h>   /* host build */
#include "hb_fir_q15.h"   /* host build */

/* ---- function prototypes / shared state ---- */
void interpolate_by_L(fir_q15_t *f, int16_t input, int L, int16_t *outputs);

#endif /* HB_INTERPOLATE_H */
