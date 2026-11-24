/*
 * Embedded DSP Field Manual, Chapter 5: I/Q and the Demod Chain / QPSK: mapping and the decision rule
 * Listing 27 of 61 (index 26, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_qpsk.h src/fm_qpsk.c | Checked by: tests/test_nco_qpsk.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
typedef struct { int16_t i, q; } iq_sample_t;

// Gray-coded QPSK: 2 bits -> one I/Q symbol, +/-0.707 (Q15-scaled unit-energy points)
static const iq_sample_t qpsk_table[4] = {
    { 23170,  23170},  // 00
    { 23170, -23170},  // 01
    {-23170,  23170},  // 10
    {-23170, -23170},  // 11
};
iq_sample_t qpsk_modulate_symbol(uint8_t dibit /* 0..3 */) { return qpsk_table[dibit & 0x3]; }

// After downconversion, matched filtering, and timing recovery, one I/Q sample per symbol.
// Decide by axis sign - no atan2, no angle quantization:
uint8_t qpsk_demodulate_symbol(int32_t i, int32_t q) {
    uint8_t bit1 = (i < 0) ? 1 : 0;
    uint8_t bit0 = (q < 0) ? 1 : 0;
    return (bit1 << 1) | bit0;
}
