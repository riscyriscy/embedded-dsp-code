/* Source: Embedded DSP Handbook, Chapter 4 (RFFT packing, worked in code).
 * Code from the Embedded DSP Handbook; MIT License.
 * Pack two real N-point signals into one complex buffer; unpack the two spectra after fft(). */
#ifndef HB_RFFT_PACK_H
#define HB_RFFT_PACK_H

#include "hb_complex.h"   /* host build */

/* ---- function prototypes / shared state ---- */
void rfft_pack(const float *a, const float *b, complex_t *packed, int N);
void rfft_unpack(const complex_t *Z, complex_t *A, complex_t *B, int N);

#endif /* HB_RFFT_PACK_H */
