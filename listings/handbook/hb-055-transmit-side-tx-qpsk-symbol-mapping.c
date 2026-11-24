/*
 * Embedded DSP Handbook, Chapter 9: Modulation / Demodulation and I/Q Signals / Transmit side (TX): QPSK symbol mapping
 * Listing 56 of 119 (index 55, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_qpsk.c include/hb_qpsk.h | Checked by: tests/test_B_qpsk.c: table, Gray ring by atan2, energy, upconvert (saturating) vs I cos - Q sin, loopback through quadrature_mix
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#include <stdint.h>

typedef struct { int16_t i, q; } iq_sample_t;

// Gray-coded QPSK: 2 bits -> one I/Q symbol at unit energy, +/-0.707 per axis
// (scale to your DAC/Q-format as needed)
static const iq_sample_t qpsk_table[4] = {
    { 23170,  23170},  // 00 ->  I=+0.707, Q=+0.707 (Q15 scaled)
    { 23170, -23170},  // 01 ->  I=+0.707, Q=-0.707
    {-23170,  23170},  // 10 ->  I=-0.707, Q=+0.707
    {-23170, -23170},  // 11 ->  I=-0.707, Q=-0.707
};

iq_sample_t qpsk_modulate_symbol(uint8_t dibit /* 0..3 */) {
    return qpsk_table[dibit & 0x3];
}

// A TX chain then pulse-shapes each I/Q symbol (e.g. root-raised-cosine FIR, see FIR section)
// before quadrature-upconverting to the carrier: same math as quadrature_mix() above, run in reverse
// (multiply I by cos, Q by sin, and SUM instead of separately lowpass-filtering).
void quadrature_upconvert(nco_t *nco, int32_t i, int32_t q, int16_t *rf_out) {
    uint8_t idx = (uint8_t)(nco->phase_acc >> 24);
    int64_t s = ((int64_t)i * cos_table[idx] - (int64_t)q * sin_table[idx]) >> 15;
    if (s > 32767) s = 32767;      // pulse-shaped I/Q overshoots the unit circle:
    if (s < -32768) s = -32768;    // saturate like mixer_to_q15(), never wrap
    *rf_out = (int16_t)s;
    nco->phase_acc += nco->phase_inc;
}
