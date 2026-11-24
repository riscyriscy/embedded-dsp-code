/*
 * Embedded DSP Field Manual, Chapter 4: Multirate and CIC / CIC: decimation with no multiplier at all
 * Listing 19 of 61 (index 18, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_cic.h src/fm_cic.c | Checked by: tests/test_filters.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// int64_t, not int32_t: the bit-growth formula below works out to 18 extra bits for this
// book's own order-3, decimate-by-64 example, and a 16-bit input needs a 34-bit-capable
// accumulator to survive that - already past what int32_t guarantees on its own, before a
// larger M or higher order pushes it further. Compute growth_bits for your actual N/M; a
// bare int32_t here is a real overflow trap, not a conservative default.
typedef struct { int64_t accum; } cic_integrator_t;
int64_t cic_integrator_step(cic_integrator_t *s, int32_t input) {
    s->accum += input;                   // pure accumulator, no multiply
    return s->accum;
}

typedef struct { int64_t delay_reg; } cic_comb_t;
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

typedef struct {
    cic_integrator_t integ[3];
    cic_comb_t comb[3];
    uint32_t decim_counter;   // owned and reset by cic_decimate_stage, above - not the caller
} cic_decimator_t;

bool cic_decimator_process(cic_decimator_t *d, int32_t input, int M, int32_t *output) {
    return cic_decimate_stage(d->integ, d->comb, input, &d->decim_counter, M, output);
}
