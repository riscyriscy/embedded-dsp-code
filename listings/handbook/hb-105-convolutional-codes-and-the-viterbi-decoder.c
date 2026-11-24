/*
 * Embedded DSP Handbook, Chapter 21: Forward Error Correction (FEC) / Convolutional codes and the Viterbi decoder
 * Listing 106 of 119 (index 105, counting from 0 in book order), as printed in the 1.0 edition of the book.
 * Status: tested | Built as: src/hb_fec.c include/hb_fec.h | Checked by: tests/test_D_fec.c: exhaustive trellis consistency, impulse response, d_free 10, encoder vs the 171/133 tap definition
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 * No longer printed in the book (November 2026): this listing is now a paragraph that points to Field Manual listing fm-045 (conv_encode_bit). The text below is the former Handbook listing, kept so the compiled module still matches it.
 */
// Rate-1/2, constraint-length-7 convolutional encoder (the classic "NASA standard" code,
// used historically in satellite/deep-space and Wi-Fi systems)
#define K 7  // constraint length

// The generators 171 and 133 (octal) are 1111001 and 1011011 in binary, with the newest
// input bit at the LEFT end. This register shifts the newest bit in at bit 0, so each
// mask is that pattern bit-reversed: 0x4F = 1001111 (117 octal) and 0x6D = 1101101
// (155 octal) are the taps of 171 and 133 as this register sees them.

uint32_t shift_reg = 0;

void conv_encode_bit(uint8_t input_bit, uint8_t *out0, uint8_t *out1) {
    shift_reg = ((shift_reg << 1) | (input_bit & 1)) & ((1u << K) - 1);  // keep only K bits
    // Generator polynomials 171 and 133 (octal) - standard rate-1/2 K=7 code
    *out0 = __builtin_parity(shift_reg & 0x4F);  // 171 octal, bit-reversed
    *out1 = __builtin_parity(shift_reg & 0x6D);  // 133 octal, bit-reversed
}
