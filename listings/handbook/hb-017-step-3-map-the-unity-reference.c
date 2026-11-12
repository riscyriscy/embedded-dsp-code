/*
 * Embedded DSP Handbook, Chapter 2: Fixed-Point Design Workflow / Step 3: Map the “unity” reference
 * Listing 18 of 119 (index 17, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_adc_q15.h src/hb_adc_q15.c | Checked by: tests/test_A_adc_dequant.c (23 checks): all 4096 codes vs (adc-2048)*16; shift 3 vs 4 (6 dB); no left shift of a negative value
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// 12-bit ADC (0-4095) -> Q15 (full 16-bit signed range). Same name and the
// same results as the Field Manual's adc_to_q15() (its Chapter 1). adc_raw
// must already be masked to 12 bits.
int16_t adc_to_q15(uint16_t adc_raw) {
    int32_t centered = (int32_t)adc_raw - 2048;  // center around 0: -2048..2047
    // x16, not << 4: left-shifting a negative int is undefined behaviour in C.
    return (int16_t)(centered * 16);   // 11-bit magnitude -> full Q15 range
}
