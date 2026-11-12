/*
 * Embedded DSP Field Manual, Chapter 4: Multirate and CIC / PDM microphones: the CIC you'll actually meet
 * Listing 21 of 57 (index 20, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_cic.h src/fm_cic.c | Checked by: tests/test_filters.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
static inline int32_t pdm_bit_to_pm1(uint8_t bit) { return bit ? 1 : -1; }

// Reuses cic_integrator_t/cic_comb_t/cic_decimator_t above; feeds one +-1 pulse per PDM
// clock instead of a multi-bit ADC sample.
void pdm_cic_feed_bit(cic_decimator_t *d, uint8_t pdm_bit, int M, bool *sample_ready,
                       int32_t *pcm_out) {
    int32_t pulse = pdm_bit_to_pm1(pdm_bit);
    *sample_ready = cic_decimator_process(d, pulse, M, pcm_out);
}
