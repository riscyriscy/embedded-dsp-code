/*
 * Embedded DSP Field Manual, Chapter 10: Testing the Chain / BER: the number of bits needed
 * Listing 50 of 61 (index 49, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_ber.h src/fm_ber.c | Checked by: tests/test_nco_qpsk.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
typedef struct { uint32_t bit_errors; uint32_t bits_tested; } ber_counter_t;

void ber_test_bit(ber_counter_t *ber, uint8_t tx_bit, uint8_t rx_bit) {
    ber->bits_tested++;
    if (tx_bit != rx_bit) ber->bit_errors++;
}

float ber_current_rate(ber_counter_t *ber) {
    return (float)ber->bit_errors / (float)ber->bits_tested;
}
