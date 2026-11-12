/* Source: Embedded DSP Handbook, Chapter 5 (Decimation: reducing the sample rate).
 * Code from the Embedded DSP Handbook; MIT License.
 * Naive decimate-by-M on the float fir_filter_t (FIR_TAPS = 32). */
#ifndef HB_DECIMATE_H
#define HB_DECIMATE_H

#include <stdbool.h>   /* host build */
#include <stdint.h>   /* host build */
#include "hb_fir_float.h"   /* host build */

/* ---- function prototypes / shared state ---- */
bool decimate_naive(fir_filter_t *f, int16_t input, int decim_factor, int *sample_count, int32_t *output);

#endif /* HB_DECIMATE_H */
