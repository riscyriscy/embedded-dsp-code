/* Source: Embedded DSP Handbook, Chapter 22 (Test and Validation Methodology): HB[111] BER counter, HB[112] required test length, HB[113] constellation dump,
 * HB[114] eye-diagram dump, HB[115] EVM, HB[116] digital-loopback helper.  MIT License.
 * Host build: stdint/stdio/stdbool/math and hb_complex.h are included; adc_read_rx_sample() (the board's ADC read) is declared. */
#include "hb_testkit.h"

/* HB[116] calls adc_read_rx_sample(), the board's ADC read (HAL-specific).  Declared here;
 * the firmware / test must provide the definition. */
int32_t adc_read_rx_sample(void);

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

// Rough required test length for a BER measurement, given a target BER to verify
// and a minimum error count for statistical confidence (10 is a common minimum; use
// more for a tighter confidence interval). Rounded up: truncating can come out a bit
// short (10 errors at 1e-5 divides to 999999.9999999999), and a test must not be
// shorter than asked.
uint64_t required_bits_for_ber_confidence(double target_ber, uint32_t min_errors_wanted) {
    return (uint64_t)ceil(min_errors_wanted / target_ber);
}

// Dump a captured I/Q buffer over UART for host-side constellation plotting.
// (Simple text format; a binary format is more efficient for large capture buffers.)
void dump_iq_buffer_uart(int32_t *i_samples, int32_t *q_samples, int count) {
    for (int n = 0; n < count; n++) {
        printf("%ld,%ld\n", (long)i_samples[n], (long)q_samples[n]);  // routed to UART/semihosting
    }
}

// Building an eye diagram from a captured, oversampled waveform buffer: wrap every
// symbol period into the same fixed-width trace and let a host plot overlay them.
// samples_per_symbol should match the RX oversampling rate (e.g. 4 or 8 samples/symbol).
void dump_eye_diagram_uart(int32_t *waveform, int total_samples, int samples_per_symbol) {
    for (int start = 0; start + samples_per_symbol <= total_samples; start += samples_per_symbol) {
        for (int k = 0; k < samples_per_symbol; k++) {
            printf("%d,%ld\n", k, (long)waveform[start + k]);  // (time-within-symbol, amplitude)
        }
    }
    // Plotting every (k, amplitude) pair as a scatter, or connecting each symbol's k=0..N-1
    // trace as its own line, produces the classic overlaid "eye" shape on the host.
}

// EVM computation: RMS error vs. nearest ideal constellation point, normalized by average symbol power.
float compute_evm_percent(complex_t *received, complex_t *ideal_nearest, int num_symbols) {
    float error_sum_sq = 0.0f, ref_sum_sq = 0.0f;
    for (int n = 0; n < num_symbols; n++) {
        float err_i = received[n].re - ideal_nearest[n].re;
        float err_q = received[n].im - ideal_nearest[n].im;
        error_sum_sq += err_i * err_i + err_q * err_q;
        ref_sum_sq += ideal_nearest[n].re * ideal_nearest[n].re
                    + ideal_nearest[n].im * ideal_nearest[n].im;
    }
    return 100.0f * sqrtf(error_sum_sq / ref_sum_sq);
}

int32_t radio_get_rx_sample(radio_config_t *cfg, int32_t tx_sample) {
    if (cfg->loopback_enabled) {
        return tx_sample;   // digital loopback: skip the ADC/RF path entirely
    }
    return adc_read_rx_sample();   // normal path
}
