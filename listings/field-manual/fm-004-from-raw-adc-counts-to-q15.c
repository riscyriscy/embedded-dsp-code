/*
 * Embedded DSP Field Manual, Chapter 1: Sampling, Q-Format, and DMA / From raw ADC counts to Q15
 * Listing 5 of 57 (index 4, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_adc.h src/fm_adc.c | Checked by: tests/test_filters.c (all 4096 codes against (adc - 2048) * 16)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// raw12: right-justified 12-bit ADC reading, offset binary, [0, 4095], midscale 2048.
// Returns a signed Q15 sample using the ADC's full dynamic range as [-1, 1).
int16_t adc_to_q15(uint16_t raw12) {
    int32_t s = (int32_t)raw12 - 2048;   // remove the 12-bit midscale offset
    // s < 0 below midscale: multiply, since s << 4 is undefined there.
    return (int16_t)(s * 16);            // left-justify into 16 bits - Q15, 4 LSBs unused
}
