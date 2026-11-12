/* Source: Embedded DSP Handbook, Chapter 2 (A different fixed-point convention: TinyML and CMSIS-NN int8).
 * MIT License. */
#ifndef HB_DEQUANT_H
#define HB_DEQUANT_H

#include <stdint.h>

/* ---- function prototypes / shared state ---- */
float dequantize_q15(int16_t val);
float dequantize_affine_int8(int8_t val, float scale, int8_t zero_point);

#endif /* HB_DEQUANT_H */
