/*
 * Embedded DSP Handbook, Chapter 5: Multirate DSP: Decimation, Interpolation, and CIC Filters / PDM microphones: the CIC use case most firmware engineers actually meet
 * Listing 45 of 119 (index 44, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_cic.c include/hb_cic.h | Checked by: tests/test_B_cic.c: density -> Q15 output, 75% pattern exact, sigma-delta sine recovered with SNR 49.8 dB and closed-form droop
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Map one raw PDM bit (0 or 1) to a signed unit pulse before the first integrator stage.
static inline int32_t pdm_bit_to_pm1(uint8_t bit) {
    return bit ? 1 : -1;
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
