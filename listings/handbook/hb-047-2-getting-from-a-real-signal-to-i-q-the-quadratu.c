/*
 * Embedded DSP Handbook, Chapter 6: Modulation / Demodulation and I/Q Signals / 2. Getting from a real signal to I/Q: the quadrature mixer
 * Listing 48 of 119 (index 47, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_nco.h src/hb_nco.c | Checked by: tests/test_B_nco.c: tables vs double math (correctly rounded), phase advance/wrap, Q30 products, mixer_to_q15 rounding+saturation, tone mixed to DC
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#include <stdint.h>
#include <math.h>

#define NCO_TABLE_SIZE 256
static int16_t cos_table[NCO_TABLE_SIZE];
static int16_t sin_table[NCO_TABLE_SIZE];

void nco_table_init(void) {
    for (int i = 0; i < NCO_TABLE_SIZE; i++) {
        // double, not float: a float angle mis-rounds one cosine entry
        double phase = 2.0 * M_PI * i / NCO_TABLE_SIZE;
        cos_table[i] = FLOAT_TO_Q15(cos(phase));    // rounded, not truncated - Chapter 1's own
        sin_table[i] = FLOAT_TO_Q15(sin(phase));    // macro, same convention as every other
    }                                                 // float-to-Q15 table in this book
}

typedef struct {
    uint32_t phase_acc;      // Q-format phase accumulator
    uint32_t phase_inc;      // current per-sample phase step, base_inc + any active correction
    uint32_t base_inc;       // the free-running step for the nominal carrier frequency, set once
                              // at init and never touched afterward; corrections are added on
                              // top of this, never accumulated into phase_inc directly.
} nco_t;

// Quadrature-mix one real sample into I/Q components (before lowpass filtering). sample and
// the table are both Q15, so a bare Q15 x Q15 multiply lands the product in Q30, NOT Q15 -
// mixer_to_q15() below shifts that back down before anything downstream (a Q15 FIR, in
// particular) touches it.
static inline void quadrature_mix(nco_t *nco, int16_t sample, int32_t *i_out, int32_t *q_out) {
    uint8_t idx = (uint8_t)(nco->phase_acc >> 24);  // top 8 bits index the table
    *i_out = (int32_t)sample * cos_table[idx];       // Q15 x Q15 -> Q30
    *q_out = -(int32_t)sample * sin_table[idx];
    nco->phase_acc += nco->phase_inc;               // advance NCO phase, wraps naturally in uint32_t
}

// Q30 mixer product -> Q15, rounded (the same add-half-then-shift discipline as
// Chapter 1's q15_mul_round) and saturated: the product's magnitude can land
// fractionally above INT16_MAX at the table's extreme entries. Truncating instead, or -
// worse - narrowing the raw Q30 value straight to int16_t with a bare cast, keeps only its
// low 16 bits: noise, not a scaled-down sample. Same name, same body, as the Embedded DSP
// Field Manual's own mixer_to_q15() (its Chapter 5) - the two books share this function
// rather than each defining their own.
static inline int16_t mixer_to_q15(int32_t wide_q30) {
    int32_t v = (wide_q30 + (1 << 14)) >> 15;
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    return (int16_t)v;
}
