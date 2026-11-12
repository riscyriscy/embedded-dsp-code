/*
 * Embedded DSP Handbook, Chapter 19: Test and Validation Methodology / Bit error rate (BER) testing
 * Listing 112 of 119 (index 111, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_testkit.c include/hb_testkit.h | Checked by: tests/test_D_testkit.c: counts/rate, 64-bit counters across 2^32, no-bits case
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// BER test loop: compare recovered bits against the known TX pattern, accumulating over
// the whole run (zero the struct to start a new measurement). The counters are 64-bit:
// verifying a BER of 1e-9 takes 1e10 bits (see below), more than a uint32_t's 4.29e9
// (71.6 minutes at 1 Mbps) before it wraps to zero.
typedef struct {
    uint64_t bit_errors;
    uint64_t bits_tested;
} ber_counter_t;

void ber_test_bit(ber_counter_t *ber, uint8_t tx_bit, uint8_t rx_bit) {
    ber->bits_tested++;
    if (tx_bit != rx_bit) {
        ber->bit_errors++;
    }
}

// Returns -1.0 until a first bit has been tested, rather than dividing 0 by 0.
double ber_current_rate(ber_counter_t *ber) {
    if (ber->bits_tested == 0) return -1.0;
    return (double)ber->bit_errors / (double)ber->bits_tested;
}
