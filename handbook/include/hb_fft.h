/* Source: Embedded DSP Handbook, Chapter 7 (How it becomes the whole FFT).
 * Iterative radix-2 float FFT (bit_reverse_permute + fft). Uses cosf/sinf per butterfly. */
#ifndef HB_FFT_H
#define HB_FFT_H

#include <math.h>
#include "hb_complex.h"

/* ---- function prototypes / shared state ---- */
void bit_reverse_permute(complex_t *x, int n);
void fft(complex_t *input, complex_t *output, int n);

#endif /* HB_FFT_H */
