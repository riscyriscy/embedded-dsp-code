/*
 * Embedded DSP Handbook, Chapter 19: Test and Validation Methodology / Statistical confidence in a BER measurement
 * Listing 113 of 119 (index 112, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_testkit.c include/hb_testkit.h | Checked by: tests/test_D_testkit.c: book example 1e7 bits exact, rounding up for round targets
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Rough required test length for a BER measurement, given a target BER to verify
// and a minimum error count for statistical confidence (10 is a common minimum; use
// more for a tighter confidence interval). Rounded up: truncating can come out a bit
// short (10 errors at 1e-5 divides to 999999.9999999999), and a test must not be
// shorter than asked.
uint64_t required_bits_for_ber_confidence(double target_ber, uint32_t min_errors_wanted) {
    return (uint64_t)ceil(min_errors_wanted / target_ber);
}
// Example: verifying a BER of 1e-6 with 10 errors of confidence needs 10,000,000 bits tested.
// At a 1 Mbps link, that's 10 seconds of continuous, error-tagged data. Plan test duration
// around this number, not around a fixed wall-clock time picked without reference to it.
