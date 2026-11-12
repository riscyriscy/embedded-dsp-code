/* BER counter, required-bits estimate, I/Q UART dump.
 * Source: Field Manual, Chapter 10. */
#ifndef FM_BER_H
#define FM_BER_H

#include <stdio.h>
#include "fm_qformat.h"

typedef struct { uint32_t bit_errors; uint32_t bits_tested; } ber_counter_t;

// Verifying 1e-6 BER with 10 errors of confidence needs 10,000,000 bits - 10 seconds
// of continuous data at 1 Mbps. Size test duration against this number, not a guess.

/* ---- function prototypes / shared state ---- */
void ber_test_bit(ber_counter_t *ber, uint8_t tx_bit, uint8_t rx_bit);
float ber_current_rate(ber_counter_t *ber);
uint64_t required_bits_for_ber_confidence(double target_ber, uint32_t min_errors_wanted);
void dump_iq_buffer_uart(int32_t *i_samples, int32_t *q_samples, int count);

#endif /* FM_BER_H */
