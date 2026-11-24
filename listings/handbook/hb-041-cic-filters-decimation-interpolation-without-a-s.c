/*
 * Embedded DSP Handbook, Chapter 8: Multirate DSP: Decimation, Interpolation, and CIC Filters / CIC filters: decimation/interpolation without a single multiply
 * Listing 42 of 119 (index 41, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_cic.c include/hb_cic.h | Checked by: tests/test_B_cic.c: vs brute-force cascaded moving sums M=1..16, DC gain M^3, counter contract, droop vs closed form
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// One CIC integrator stage (runs at the HIGH sample rate, before decimation). The registers
// are 64-bit UNSIGNED: an integrator's sum grows without bound, and the combs cancel any
// wrap-around exactly (Hogenauer's modular arithmetic) as long as the register is at least
// as wide as the final output (34 bits for the 16-bit, N=3, M=64 case in the text). Only
// unsigned arithmetic is defined to wrap in C, hence unsigned.
typedef struct { uint64_t accum; } cic_integrator_t;

uint64_t cic_integrator_step(cic_integrator_t *s, uint64_t input) {
    s->accum += input;      // pure accumulator - no multiply
    return s->accum;
}

// One CIC comb stage (runs at the LOW sample rate, after decimation)
typedef struct { uint64_t delay_reg; } cic_comb_t;

uint64_t cic_comb_step(cic_comb_t *s, uint64_t input) {
    uint64_t output = input - s->delay_reg;   // simple difference - no multiply
    s->delay_reg = input;
    return output;
}

// A decimate-by-M, order-N CIC: N integrators at input rate, decimate by M, N combs at output rate.
// (Shown conceptually for N=3; in practice you'd loop over an array of stage structs.)
// Returns bool + out-parameter, the same fix Chapter 8's FIR decimator needed: a sentinel
// return value is easy for a careless caller to mistake for real data.
//
// decim_counter is passed BY VALUE and this function never increments it: the CALLER owns
// that counter (typically one persistent int living alongside integ[]/comb[] in whatever
// struct represents this decimator's state), incrementing it once per call before passing
// it in. A caller that assumes this function owns the phase, and always passes the same
// value, will see every sample kept or none, never the 1-in-M pattern decimation requires.
bool cic_decimate_stage(cic_integrator_t integ[3], cic_comb_t comb[3], int32_t input,
                          int decim_counter, int M, int64_t *output) {
    uint64_t x = (uint64_t)(int64_t)input;   // sign-extend first, then do the wrap-safe math
    for (int i = 0; i < 3; i++) x = cic_integrator_step(&integ[i], x);
    if (decim_counter % M != 0) return false;   // *output not written; not a kept sample this cycle
    for (int i = 0; i < 3; i++) x = cic_comb_step(&comb[i], x);
    *output = (int64_t)x;   // true result is below 2^33 in magnitude: two's-complement reading
    return true;
}

// The calling convention this contract actually implies: the caller's own state struct
// holds the counter and advances it every call, wrapping at M so it can never overflow
// (only decim_counter % M is ever read).
typedef struct {
    cic_integrator_t integ[3];
    cic_comb_t comb[3];
    int decim_counter;   // owned here, not inside cic_decimate_stage
} cic_decimator_t;

bool cic_decimator_process(cic_decimator_t *d, int32_t input, int M, int64_t *output) {
    d->decim_counter = (d->decim_counter + 1) % M;
    return cic_decimate_stage(d->integ, d->comb, input, d->decim_counter, M, output);
}
