/* Source: Embedded DSP Handbook, Chapter 6 (Transmit side: QPSK symbol mapping; Receive side: symbol recovery and decision).
 * Code from the Embedded DSP Handbook; MIT License.
 * Host build: quadrature_upconvert() is static inline because it indexes hb_nco.h's file-static cos_table/sin_table;
 * an out-of-line copy would see its own, never-initialised tables. Call nco_table_init() first. */
#include "hb_qpsk.h"

iq_sample_t qpsk_modulate_symbol(uint8_t dibit /* 0..3 */) {
    return qpsk_table[dibit & 0x3];
}

// After quadrature downconversion (quadrature_mix), lowpass filtering, matched filtering
// (root-raised-cosine FIR), and symbol-timing recovery, you have one I/Q sample per symbol.
// The decision step for QPSK is just picking the nearest constellation point by sign:

uint8_t qpsk_demodulate_symbol(int32_t i, int32_t q) {
    uint8_t bit1 = (i < 0) ? 1 : 0;   // I sign -> first bit
    uint8_t bit0 = (q < 0) ? 1 : 0;   // Q sign -> second bit
    return (bit1 << 1) | bit0;
}
