/* Source: Embedded DSP Handbook, Chapter 11 (2. Symbol timing recovery): cubic Farrow interpolator (float) and its fixed-point version.
 * MIT License.
 * Host build: the table definition is include/generated/farrow_weights.h, written by tools/gen_farrow_weights.py. */
#ifndef HB_FARROW_H
#define HB_FARROW_H

#include <stdint.h>

// Fixed-point Farrow: mu quantized to a small number of steps, each step's four Lagrange
// weights computed once (offline, in Python/MATLAB, the same coefficient-export workflow
// from Chapter 5) and stored as a Q15 table, never recomputed from a polynomial at runtime.
#define FARROW_MU_STEPS 32   // mu resolution; more steps = finer timing resolution, more table space

// weights_q15[step][0..3] = { p_m1, p_0, p_1, p_2 } for mu = step / FARROW_MU_STEPS, in Q15,
// generated once offline from the exact same Lagrange formulas shown above.
// (A weight of exactly 1.0, p_0 at mu = 0, does not fit an int16_t: the table
// holds 32767.)
extern const int16_t farrow_weights_q15[FARROW_MU_STEPS][4];

/* ---- function prototypes / shared state ---- */
int32_t timing_interpolate_farrow_cubic(int32_t x_m1, int32_t x_0, int32_t x_1, int32_t x_2, int32_t mu_q15);
int32_t timing_interpolate_farrow_cubic_q15(int32_t x_m1, int32_t x_0, int32_t x_1, int32_t x_2, int32_t mu_q15);

#endif /* HB_FARROW_H */
