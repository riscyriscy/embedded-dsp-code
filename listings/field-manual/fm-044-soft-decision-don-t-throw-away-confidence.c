/*
 * Embedded DSP Field Manual, Chapter 9: FEC and the CPU Budget / Soft decision: don't throw away confidence
 * Listing 45 of 61 (index 44, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_fec.h src/fm_fec.c | Checked by: tests/test_fec.c (LLR signs/bit order); LLR_SCALE_Q is defined in the listing and overridable
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// LLR gain in Q15: 16384 = 0.5, so the LLR is half the Q15 axis value.
// Match it to your decoder's metric range; i * LLR_SCALE_Q must fit an int32_t.
#define LLR_SCALE_Q 16384

// Bit order matches qpsk_demodulate_symbol (Chapter 5) exactly: I is bit1, Q is bit0. A decoder
// fed llr_bit1/llr_bit0 in the other order silently swaps every symbol's two bits.
void qpsk_soft_llr(int32_t i, int32_t q, int32_t *llr_bit1, int32_t *llr_bit0) {
    *llr_bit1 = (i * LLR_SCALE_Q) >> 15;   // confidence for the I-axis bit
    *llr_bit0 = (q * LLR_SCALE_Q) >> 15;   // confidence for the Q-axis bit
}
