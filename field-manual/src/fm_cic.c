/* CIC decimator (order 3) and PDM front end.
 * Source: Field Manual, Chapter 4. */
#include "fm_cic.h"

int64_t cic_integrator_step(cic_integrator_t *s, int32_t input) {
    s->accum += input;                   // pure accumulator, no multiply
    return s->accum;
}

int64_t cic_comb_step(cic_comb_t *s, int64_t input) {
    int64_t output = input - s->delay_reg;   // simple difference, no multiply
    s->delay_reg = input;
    return output;
}

// Decimate-by-M, order-3 CIC: 3 integrators at input rate, decimate by M, 3 combs at output rate.
// decim_counter is bounded to [0, M) by this function itself, not left to grow forever: a plain
// incrementing int, checked with `% M`, is a real signed-overflow bug on a decimator that runs
// for the life of the device (overflowing a signed int is undefined behavior in C, not just a
// wraparound). Resetting it here instead means the counter never leaves a small, fixed range.
bool cic_decimate_stage(cic_integrator_t integ[3], cic_comb_t comb[3], int32_t input,
                          uint32_t *decim_counter, int M, int32_t *output) {
    int64_t x = input;
    for (int i = 0; i < 3; i++) x = cic_integrator_step(&integ[i], x);
    if (++(*decim_counter) < (uint32_t)M) return false;    // not a kept sample yet this cycle
    *decim_counter = 0;                                      // bounded to [0, M) - never grows
    for (int i = 0; i < 3; i++) x = cic_comb_step(&comb[i], x);
    *output = (int32_t)x;    // narrows back down - fine once M/N are sized so the comb-stage
                              // output itself fits int32_t; the wide type above only protects
                              // the integrator stages, which is where the unbounded growth is
    return true;
}

bool cic_decimator_process(cic_decimator_t *d, int32_t input, int M, int32_t *output) {
    return cic_decimate_stage(d->integ, d->comb, input, &d->decim_counter, M, output);
}

// Reuses cic_integrator_t/cic_comb_t/cic_decimator_t above; feeds one +-1 pulse per PDM
// clock instead of a multi-bit ADC sample.
void pdm_cic_feed_bit(cic_decimator_t *d, uint8_t pdm_bit, int M, bool *sample_ready,
                       int32_t *pcm_out) {
    int32_t pulse = pdm_bit_to_pm1(pdm_bit);
    *sample_ready = cic_decimator_process(d, pulse, M, pcm_out);
}
