/* Source: Embedded DSP Handbook, Chapter 22 (Test and Validation Methodology): HB[111] BER counter, HB[112] required test length, HB[113] constellation dump,
 * HB[114] eye-diagram dump, HB[115] EVM, HB[116] digital-loopback helper.  MIT License.
 * Host build: stdint/stdio/stdbool/math and hb_complex.h are included; adc_read_rx_sample() (the board's ADC read) is declared. */
#ifndef HB_TESTKIT_H
#define HB_TESTKIT_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include "hb_complex.h"

// BER test loop: compare recovered bits against the known TX pattern, accumulating over
// the whole run (zero the struct to start a new measurement). The counters are 64-bit:
// verifying a BER of 1e-9 takes 1e10 bits (see below), more than a uint32_t's 4.29e9
// (71.6 minutes at 1 Mbps) before it wraps to zero.
typedef struct {
    uint64_t bit_errors;
    uint64_t bits_tested;
} ber_counter_t;

// Example: verifying a BER of 1e-6 with 10 errors of confidence needs 10,000,000 bits tested.
// At a 1 Mbps link, that's 10 seconds of continuous, error-tagged data. Plan test duration
// around this number, not around a fixed wall-clock time picked without reference to it.

// Digital loopback: bypass the RF front end entirely and feed modulated I/Q samples
// straight into the demodulation chain. Verifies the DSP chain (Chapters 9, 11, 12)
// independent of any RF hardware fault.
typedef struct {
    bool loopback_enabled;
} radio_config_t;

// RF loopback (with an external cable/attenuator between TX and RX connectors) additionally
// exercises the mixer, PA, LNA, and ADC, catching hardware faults digital loopback cannot.

/* ---- function prototypes / shared state ---- */
void ber_test_bit(ber_counter_t *ber, uint8_t tx_bit, uint8_t rx_bit);
double ber_current_rate(ber_counter_t *ber);
uint64_t required_bits_for_ber_confidence(double target_ber, uint32_t min_errors_wanted);
void dump_iq_buffer_uart(int32_t *i_samples, int32_t *q_samples, int count);
void dump_eye_diagram_uart(int32_t *waveform, int total_samples, int samples_per_symbol);
float compute_evm_percent(complex_t *received, complex_t *ideal_nearest, int num_symbols);
int32_t radio_get_rx_sample(radio_config_t *cfg, int32_t tx_sample);

#endif /* HB_TESTKIT_H */
