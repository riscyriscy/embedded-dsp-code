/*
 * Embedded DSP Handbook, Chapter 21: Forward Error Correction (FEC) / Hard-decision vs. soft-decision decoding
 * Listing 105 of 119 (index 104, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_fec.c include/hb_fec.h | Checked by: tests/test_D_fec.c: qpsk_soft_llr per-axis values/signs
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// QPSK soft decision: I and Q each independently produce one bit's LLR, exactly the same
// BPSK-style computation run twice, once per axis, not a fundamentally different calculation.
void qpsk_soft_llr(int32_t i, int32_t q, int32_t *llr_bit0, int32_t *llr_bit1) {
    *llr_bit0 = (i * LLR_SCALE_Q) >> 15;   // confidence for the I-axis bit
    *llr_bit1 = (q * LLR_SCALE_Q) >> 15;   // confidence for the Q-axis bit
}
