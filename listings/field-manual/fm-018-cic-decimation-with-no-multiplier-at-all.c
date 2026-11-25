/*
 * Embedded DSP Field Manual, Chapter 4: Multirate and CIC / CIC: decimation with no multiplier at all
 * Listing 19 of 61 (index 18, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_cic.h src/fm_cic.c | Checked by: tests/test_filters.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// uint64_t, not int32_t and not a signed type. The bit-growth formula below works out to 18
// extra bits for the order-3, decimate-by-64 example, and a 16-bit input needs a 34-bit-capable
// register to survive that, already past what int32_t guarantees. Compute growth_bits for your
// actual N/M. The integrators are allowed to wrap: each one sums without bound, and the combs
// cancel the wrap exactly (modular arithmetic) as long as the register is at least as wide as
// the output. Only unsigned arithmetic is defined to wrap in C; a signed int64_t that overflows
// is undefined behavior. Every stage takes and returns all 64 bits, so no stage truncates what
// the stage before it produced.
typedef struct { uint64_t accum; } cic_integrator_t;
uint64_t cic_integrator_step(cic_integrator_t *s, uint64_t input) {
    s->accum += input;                   // pure accumulator, no multiply
    return s->accum;
}

typedef struct { uint64_t delay_reg; } cic_comb_t;
uint64_t cic_comb_step(cic_comb_t *s, uint64_t input) {
    uint64_t output = input - s->delay_reg;   // simple difference, no multiply
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
    uint64_t x = (uint64_t)(int64_t)input;   // sign-extend first, then do the wrap-safe math
    for (int i = 0; i < 3; i++) x = cic_integrator_step(&integ[i], x);
    if (++(*decim_counter) < (uint32_t)M) return false;    // not a kept sample yet this cycle
    *decim_counter = 0;                                      // bounded to [0, M) - never grows
    for (int i = 0; i < 3; i++) x = cic_comb_step(&comb[i], x);
    *output = (int32_t)(int64_t)x;   // the true result, read back as two's complement; it fits
                                     // int32_t once M and N are sized so the output does. The
                                     // integrators may have wrapped, and the combs cancelled it
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
