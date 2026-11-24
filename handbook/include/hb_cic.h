/* Source: Embedded DSP Handbook, Chapter 8 (CIC filters; Bit growth: sizing the accumulator; PDM microphones).
 * Code from the Embedded DSP Handbook; MIT License.
 * Integrator/comb stages, order-3 decimator, growth-bits rescale and the PDM front end (listings 39, 41, 42 in one unit).
 * Register growth of an order-N CIC decimating by M (HB[42]):
 *   growth_bits = ceil(N * log2(M))
 * Host build: cic_growth_bits() is file-static, so it lives in src/hb_cic.c. */
#ifndef HB_CIC_H
#define HB_CIC_H

#include <stdbool.h>   /* host build */
#include <stdint.h>   /* host build */

// One CIC integrator stage (runs at the HIGH sample rate, before decimation). The registers
// are 64-bit UNSIGNED: an integrator's sum grows without bound, and the combs cancel any
// wrap-around exactly (Hogenauer's modular arithmetic) as long as the register is at least
// as wide as the final output (34 bits for the 16-bit, N=3, M=64 case in the text). Only
// unsigned arithmetic is defined to wrap in C, hence unsigned.
typedef struct { uint64_t accum; } cic_integrator_t;

// One CIC comb stage (runs at the LOW sample rate, after decimation)
typedef struct { uint64_t delay_reg; } cic_comb_t;

// The calling convention this contract actually implies: the caller's own state struct
// holds the counter and advances it every call, wrapping at M so it can never overflow
// (only decim_counter % M is ever read).
typedef struct {
    cic_integrator_t integ[3];
    cic_comb_t comb[3];
    int decim_counter;   // owned here, not inside cic_decimate_stage
} cic_decimator_t;

// Map one raw PDM bit (0 or 1) to a signed unit pulse before the first integrator stage.
static inline int32_t pdm_bit_to_pm1(uint8_t bit) {
    return bit ? 1 : -1;
}

/* ---- function prototypes / shared state ---- */
uint64_t cic_integrator_step(cic_integrator_t *s, uint64_t input);
uint64_t cic_comb_step(cic_comb_t *s, uint64_t input);
bool cic_decimate_stage(cic_integrator_t integ[3], cic_comb_t comb[3], int32_t input, int decim_counter, int M, int64_t *output);
bool cic_decimator_process(cic_decimator_t *d, int32_t input, int M, int64_t *output);
bool cic_decimate_stage_scaled(cic_integrator_t integ[3], cic_comb_t comb[3], int32_t input, int decim_counter, int M, int32_t *output);
bool cic_decimator_process_scaled(cic_decimator_t *d, int32_t input, int M, int32_t *output);
void pdm_cic_feed_bit(cic_decimator_t *d, uint8_t pdm_bit, int M, bool *sample_ready, int32_t *pcm_out);

#endif /* HB_CIC_H */
