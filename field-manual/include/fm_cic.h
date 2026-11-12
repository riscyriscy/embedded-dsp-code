/* CIC decimator (order 3) and PDM front end.
 * Source: Field Manual, Chapter 4. */
#ifndef FM_CIC_H
#define FM_CIC_H

#include "fm_qformat.h"

// int64_t, not int32_t: the bit-growth formula below works out to 18 extra bits for this
// book's own order-3, decimate-by-64 example, and a 16-bit input needs a 34-bit-capable
// accumulator to survive that - already past what int32_t guarantees on its own, before a
// larger M or higher order pushes it further. Compute growth_bits for your actual N/M; a
// bare int32_t here is a real overflow trap, not a conservative default.
typedef struct { int64_t accum; } cic_integrator_t;

typedef struct { int64_t delay_reg; } cic_comb_t;

typedef struct {
    cic_integrator_t integ[3];
    cic_comb_t comb[3];
    uint32_t decim_counter;   // owned and reset by cic_decimate_stage, above - not the caller
} cic_decimator_t;

static inline int32_t pdm_bit_to_pm1(uint8_t bit) { return bit ? 1 : -1; }

/* ---- function prototypes / shared state ---- */
int64_t cic_integrator_step(cic_integrator_t *s, int32_t input);
int64_t cic_comb_step(cic_comb_t *s, int64_t input);
bool cic_decimate_stage(cic_integrator_t integ[3], cic_comb_t comb[3], int32_t input, uint32_t *decim_counter, int M, int32_t *output);
bool cic_decimator_process(cic_decimator_t *d, int32_t input, int M, int32_t *output);
void pdm_cic_feed_bit(cic_decimator_t *d, uint8_t pdm_bit, int M, bool *sample_ready, int32_t *pcm_out);

#endif /* FM_CIC_H */
