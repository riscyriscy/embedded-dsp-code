/* Gray-coded QPSK map and hard-decision slicer.
 * Source: Field Manual, Chapter 5. */
#include "fm_qpsk.h"

iq_sample_t qpsk_modulate_symbol(uint8_t dibit /* 0..3 */) { return qpsk_table[dibit & 0x3]; }

// After downconversion, matched filtering, and timing recovery, one I/Q sample per symbol.
// Decide by axis sign - no atan2, no angle quantization:
uint8_t qpsk_demodulate_symbol(int32_t i, int32_t q) {
    uint8_t bit1 = (i < 0) ? 1 : 0;
    uint8_t bit0 = (q < 0) ? 1 : 0;
    return (bit1 << 1) | bit0;
}
