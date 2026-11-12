/* Source: Embedded DSP Handbook, Chapter 9 (The standard second-order loop design equations).
 * FLOAT_TO_Q16(), COSTAS_KP_Q16(), COSTAS_KI_Q16(), TIMING_GAIN_Q16().  MIT License.
 * Host build: the two `int32_t kp_q16 = ...;` statements that follow the macros live in tests/shims/hb_C_snippet_79.inc
 * (TIMING_GAIN_Q16 still goes through lrintf, which is not a constant expression). */
#ifndef HB_LOOP_GAINS_H
#define HB_LOOP_GAINS_H

#include <stdint.h>
#include <math.h>

#define FLOAT_TO_Q16(f) ((int32_t)lrintf((f) * 65536.0f))

// Costas: kd is the detector's slope in detector counts per radian (2*A for
// costas_phase_error(), where A = |I| = |Q| at the decision instant, in
// sample units), and the NCO takes 2^32 phase words per circle, so one
// radian of correction is 2^32 / (2*pi) words. Ki is per symbol, but c->freq
// is added to phase_inc on every SAMPLE, so ki also divides by the samples
// per symbol. Same arguments and same arithmetic as the Field Manual's
// macros: round-to-nearest for positive gains, and a constant expression, so
// they can initialize a struct.
#define NCO_WORDS_PER_RAD (4294967296.0 / 6.283185307179586)
#define COSTAS_KP_Q16(kp, kd) \
    ((int32_t)((kp) * NCO_WORDS_PER_RAD / (kd) * 65536.0 + 0.5))
#define COSTAS_KI_Q16(ki, kd, sps) \
    ((int32_t)((ki) * NCO_WORDS_PER_RAD / ((kd) * (sps)) * 65536.0 + 0.5))

// Timing: kd is the Gardner detector's output per symbol of timing error,
// measured at the operating amplitude; mu counts Q15 steps per sample, so
// mu_per_symbol is 65536 at 2 samples/symbol. Both gains are per symbol and
// timing_loop_update() runs once per symbol, so there is no sps term.
#define TIMING_GAIN_Q16(g, kd, mu_per_symbol) \
    FLOAT_TO_Q16((g) / (kd) * (mu_per_symbol))

#endif /* HB_LOOP_GAINS_H */
