/* Source: Embedded DSP Handbook, Chapter 2 (A different fixed-point convention: TinyML and CMSIS-NN int8).
 * MIT License. */
#include "hb_dequant.h"

// Q15 (this chapter): a pure scaling factor, symmetric around zero, fixed at design time.
float dequantize_q15(int16_t val) {
    return val / 32768.0f;  // val * 2^-15, nothing else
}

// CMSIS-NN int8: an affine mapping, scale AND a zero-point offset, both chosen per-tensor
// during quantization (not fixed like Q15's 2^-n), letting the representable range sit
// anywhere, not just symmetric around zero.
float dequantize_affine_int8(int8_t val, float scale, int8_t zero_point) {
    return scale * (val - zero_point);
}
