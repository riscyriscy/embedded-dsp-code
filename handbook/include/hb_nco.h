/* Source: Embedded DSP Handbook, Chapter 9 (section 2, the quadrature mixer).
 * NCO, quadrature mixer, Q30->Q15 rescale. Tables are filled at runtime by nco_table_init().
 * Host build: the tables are file-static, so nco_table_init() lives in the header as static inline;
 * M_PI needs _DEFAULT_SOURCE/_GNU_SOURCE with -std=c11.
 * The equations behind the code:
 *   s(t) = I(t)*cos(2*pi*fc*t) - Q(t)*sin(2*pi*fc*t)
 *   I(t) = LPF[ x(t) * cos(2*pi*fc*t) ]
 *   Q(t) = LPF[ x(t) * -sin(2*pi*fc*t) ] */
#ifndef HB_NCO_H
#define HB_NCO_H

#include <stdint.h>
#include <math.h>
#include "hb_qformat.h"

#define NCO_TABLE_SIZE 256
static int16_t cos_table[NCO_TABLE_SIZE];
static int16_t sin_table[NCO_TABLE_SIZE];

static inline void nco_table_init(void) {
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

uint32_t nco_phase_inc_for_freq(float fc, float fs);

#endif /* HB_NCO_H */
