/*
 * Embedded DSP Handbook, Chapter 5: Fixed-Point Design Workflow / A different fixed-point convention: TinyML and CMSIS-NN's int8
 * Listing 27 of 119 (index 26, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_dequant.h src/hb_dequant.c | Checked by: tests/test_A_adc_dequant.c: dequantize_q15 == v/2^15 exactly over int16, affine int8 vs scale*(q-zp)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
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
