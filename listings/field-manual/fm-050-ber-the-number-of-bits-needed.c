/*
 * Embedded DSP Field Manual, Chapter 10: Testing the Chain / BER: the number of bits needed
 * Listing 51 of 57 (index 50, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_ber.h src/fm_ber.c | Checked by: tests/test_nco_qpsk.c (10 errors at 1e-6 -> 10,000,000 bits)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
uint64_t required_bits_for_ber_confidence(double target_ber, uint32_t min_errors_wanted) {
    return (uint64_t)(min_errors_wanted / target_ber);
}
// Verifying 1e-6 BER with 10 errors of confidence needs 10,000,000 bits - 10 seconds
// of continuous data at 1 Mbps. Size test duration against this number, not a guess.
