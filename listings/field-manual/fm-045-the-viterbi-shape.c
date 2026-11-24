/*
 * Embedded DSP Field Manual, Chapter 9: FEC and the CPU Budget / The Viterbi shape
 * Listing 46 of 61 (index 45, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_fec.h src/fm_fec.c | Checked by: tests/test_fec.c (golden vector exact; encoder vs the 171/133 tap definition)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#define K 7
uint32_t shift_reg = 0;

// The newest bit enters at bit 0, so the masks are the usual 171/133 octal
// generators (leftmost tap = the current input) written bit-reversed:
// 1111001 -> 1001111 = 0x4F, and 1011011 -> 1101101 = 0x6D.
void conv_encode_bit(uint8_t input_bit, uint8_t *out0, uint8_t *out1) {
    shift_reg = ((shift_reg << 1) | (input_bit & 1)) & ((1u << K) - 1);
    *out0 = __builtin_parity(shift_reg & 0x4F);   // 0171 octal generator
    *out1 = __builtin_parity(shift_reg & 0x6D);   // 0133 octal generator
}
