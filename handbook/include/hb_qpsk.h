/* Source: Embedded DSP Handbook, Chapter 6 (Transmit side: QPSK symbol mapping; Receive side: symbol recovery and decision).
 * Code from the Embedded DSP Handbook; MIT License.
 * Host build: quadrature_upconvert() is static inline because it indexes hb_nco.h's file-static cos_table/sin_table;
 * an out-of-line copy would see its own, never-initialised tables. Call nco_table_init() first. */
#ifndef HB_QPSK_H
#define HB_QPSK_H

#include "hb_nco.h"   /* host build */

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

// A TX chain then pulse-shapes each I/Q symbol (e.g. root-raised-cosine FIR, see FIR section)
// before quadrature-upconverting to the carrier: same math as quadrature_mix() above, run in reverse
// (multiply I by cos, Q by sin, and SUM instead of separately lowpass-filtering).
static inline void quadrature_upconvert(nco_t *nco, int32_t i, int32_t q, int16_t *rf_out) {
    uint8_t idx = (uint8_t)(nco->phase_acc >> 24);
    int64_t s = ((int64_t)i * cos_table[idx] - (int64_t)q * sin_table[idx]) >> 15;
    if (s > 32767) s = 32767;      // pulse-shaped I/Q overshoots the unit circle:
    if (s < -32768) s = -32768;    // saturate like mixer_to_q15(), never wrap
    *rf_out = (int16_t)s;
    nco->phase_acc += nco->phase_inc;
}

// A soft-decision variant (feeds a Viterbi/LDPC decoder instead of a hard bit) just outputs
// the raw i and q values (as log-likelihood-ish soft metrics) instead of collapsing to a bit.

/* ---- function prototypes / shared state ---- */
iq_sample_t qpsk_modulate_symbol(uint8_t dibit );
uint8_t qpsk_demodulate_symbol(int32_t i, int32_t q);

#endif /* HB_QPSK_H */
