/* Source: Embedded DSP Handbook, Chapter 8 (CIC filters; Bit growth: sizing the accumulator; PDM microphones).
 * Code from the Embedded DSP Handbook; MIT License.
 * Integrator/comb stages, order-3 decimator, growth-bits rescale and the PDM front end (listings 39, 41, 42 in one unit).
 * Host build: cic_growth_bits() is file-static, so it lives in this file. */
#include "hb_cic.h"

uint64_t cic_integrator_step(cic_integrator_t *s, uint64_t input) {
    s->accum += input;      // pure accumulator - no multiply
    return s->accum;
}

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

bool cic_decimator_process(cic_decimator_t *d, int32_t input, int M, int64_t *output) {
    d->decim_counter = (d->decim_counter + 1) % M;
    return cic_decimate_stage(d->integ, d->comb, input, d->decim_counter, M, output);
}

// Right-shift amount that undoes a CIC's inherent M^N passband gain (N=3 here, matching the
// integ[3]/comb[3] arrays above), bringing a decimated output back to roughly the same numeric
// scale as the input. Exact only when M is itself a power of 2 (log2(M) lands on an integer);
// for any other M this rounds UP to the next power of 2, so the shift removes MORE than the
// true M^N gain - by close to a factor of 2 per stage in the worst case (M just above a
// power-of-2 boundary), a real, fixed, predictable attenuation worth sizing for (or avoiding
// outright by choosing a power-of-2 M), not a bug to go chase down later.
static int cic_growth_bits(int N, int M) {
    int bits = 0;
    while ((1 << bits) < M) bits++;
    return N * bits;
}

// Same as cic_decimate_stage above, with the missing rescale step added: after the comb
// stages produce a decimated sample, round and shift it back down by growth_bits before
// handing it to the caller, instead of returning the still-M^N-scaled raw value.
bool cic_decimate_stage_scaled(cic_integrator_t integ[3], cic_comb_t comb[3], int32_t input,
                                 int decim_counter, int M, int32_t *output) {
    int64_t raw;
    if (!cic_decimate_stage(integ, comb, input, decim_counter, M, &raw)) return false;

    int gb = cic_growth_bits(3, M);
    int64_t rounded = (gb > 0) ? (raw + ((int64_t)1 << (gb - 1))) : raw;
    *output = (int32_t)(rounded >> gb);
    return true;
}

// The decimator-state wrapper for the scaled path: same counter handling as
// cic_decimator_process, but the output is back on the input's numeric scale.
bool cic_decimator_process_scaled(cic_decimator_t *d, int32_t input, int M, int32_t *output) {
    d->decim_counter = (d->decim_counter + 1) % M;
    return cic_decimate_stage_scaled(d->integ, d->comb, input, d->decim_counter, M, output);
}

// Reuses the cic_integrator_t / cic_comb_t / cic_decimator_t machinery already defined
// above; a PDM front end just feeds it one +1/-1 pulse per PDM clock instead of a
// multi-bit ADC sample, at whatever order and decimation ratio (M) the target sample
// rate requires. A full-density stream gives a raw output of +-M^3, so the PCM sample is
// rescaled to Q15 here: raw * 2^15 / 2^growth_bits, rounded (+-32768 at full density; use
// a power-of-2 M >= 2). Rescaling to the +-1 input scale instead would leave only {-1, 0, 1}.
void pdm_cic_feed_bit(cic_decimator_t *d, uint8_t pdm_bit, int M, bool *sample_ready,
                       int32_t *pcm_out) {
    int32_t pulse = pdm_bit_to_pm1(pdm_bit);
    int64_t raw;
    *sample_ready = cic_decimator_process(d, pulse, M, &raw);
    if (*sample_ready) {
        int gb = cic_growth_bits(3, M);
        *pcm_out = (int32_t)((raw * 32768 + ((int64_t)1 << (gb - 1))) >> gb);
    }
}
