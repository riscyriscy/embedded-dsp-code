/*
 * Embedded DSP Handbook, Chapter 6: Modulation / Demodulation and I/Q Signals / Receive side (RX): symbol recovery and decision
 * Listing 57 of 119 (index 56, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_qpsk.c include/hb_qpsk.h | Checked by: tests/test_B_qpsk.c: round trip, sign boundaries, 1-bit/2-bit error costs, Monte-Carlo BER vs Q(A/sigma)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// After quadrature downconversion (quadrature_mix), lowpass filtering, matched filtering
// (root-raised-cosine FIR), and symbol-timing recovery, you have one I/Q sample per symbol.
// The decision step for QPSK is just picking the nearest constellation point by sign:

uint8_t qpsk_demodulate_symbol(int32_t i, int32_t q) {
    uint8_t bit1 = (i < 0) ? 1 : 0;   // I sign -> first bit
    uint8_t bit0 = (q < 0) ? 1 : 0;   // Q sign -> second bit
    return (bit1 << 1) | bit0;
}

// A soft-decision variant (feeds a Viterbi/LDPC decoder instead of a hard bit) just outputs
// the raw i and q values (as log-likelihood-ish soft metrics) instead of collapsing to a bit.
