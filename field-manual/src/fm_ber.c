/* BER counter, required-bits estimate, I/Q UART dump.
 * Source: Field Manual, Chapter 10. */
#include "fm_ber.h"

void ber_test_bit(ber_counter_t *ber, uint8_t tx_bit, uint8_t rx_bit) {
    ber->bits_tested++;
    if (tx_bit != rx_bit) ber->bit_errors++;
}

float ber_current_rate(ber_counter_t *ber) {
    return (float)ber->bit_errors / (float)ber->bits_tested;
}

uint64_t required_bits_for_ber_confidence(double target_ber, uint32_t min_errors_wanted) {
    return (uint64_t)(min_errors_wanted / target_ber);
}

void dump_iq_buffer_uart(int32_t *i_samples, int32_t *q_samples, int count) {
    for (int n = 0; n < count; n++)
        printf("%ld,%ld\n", (long)i_samples[n], (long)q_samples[n]);
}
