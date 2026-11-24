/*
 * Embedded DSP Handbook, Chapter 22: Test and Validation Methodology / One complete loopback chain, wired end to end
 * Listing 119 of 119 (index 118, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: tests/shims/hb_E_init.inc | Checked by: tests/test_E_chain.c includes this block verbatim inside a function and drives the receiver with it
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// The chain's constants. Only tx_waveform[], tx_dibit_stream[] and
// NUM_LOOPBACK_SAMPLES come from the bench (generated, not listed here).
#define AGC_TARGET_POWER  (1 << 28)    // AGC power target: rms 16384
#define CARRIER_PHASE_INC 0x40000000u  // fs/4 = 4800 Hz at 19200 Hz: 2^32/4
// DECISION_AMP: |I| = |Q| at the decision instant once the AGC has settled
// (measured in lock). TIMING_KD: Gardner Kd, I and Q rails summed,
// 2 * 1.08 * A*A/2^16 for these alpha = 0.35 pulses.
#define DECISION_AMP      7800.0
#define TIMING_KD         2000.0
#define MU_PER_SYMBOL     65536.0      // Q15 steps per sample at 2 sps
#define TIMING_KP_Q16     TIMING_GAIN_Q16(0.05192, TIMING_KD, MU_PER_SYMBOL)
#define TIMING_KI_Q16     TIMING_GAIN_Q16(0.001385, TIMING_KD, MU_PER_SYMBOL)

// Chapter 9's runtime table fill, once (not needed with the const tables):
nco_table_init();
radio_config_t radio_cfg = { .loopback_enabled = true };
qpsk_loopback_rx_t rx = {
    .agc         = { .gain_q15 = 32768, .target_power = AGC_TARGET_POWER, .attack_shift = 4, .release_shift = 8 },
    .nco         = { .phase_acc = 0, .phase_inc = CARRIER_PHASE_INC, .base_inc = CARRIER_PHASE_INC },
    .timing_loop = { .kp = TIMING_KP_Q16, .ki = TIMING_KI_Q16 },  // your own Gardner Bn*T/zeta -
                                                                    // pulse shape and Kd set these,
                                                                    // not this golden vector
    .costas      = { .kp = COSTAS_KP_Q16(0.05192, 2.0 * DECISION_AMP),
                     .ki = COSTAS_KI_Q16(0.001385, 2.0 * DECISION_AMP, 2) },
};  // Chapter 12's worked example (Bn*T = 0.02, zeta = 0.707) in the units
    // costas_update() uses
// rrc_i / rrc_q are initialized the same way any other fir_q15_t in this book is: fir_init_q15()
// with the RRC coefficients from Chapter 11's pulse-shape design section
// (alpha = 0.35).
fir_init_q15(&rx.rrc_i, rrc_coeffs_q15);
fir_init_q15(&rx.rrc_q, rrc_coeffs_q15);

for (int n = 0; n < NUM_LOOPBACK_SAMPLES; n++) {
    int32_t tx_sample = tx_waveform[n];        // TX chain output, oversampled at 2 samples/symbol
    int32_t rx_sample = radio_get_rx_sample(&radio_cfg, tx_sample);  // digital loopback, see above
    qpsk_loopback_process_sample(&rx, rx_sample, tx_dibit_stream[n / 2]);
}
// rx.ber now holds the accumulated bit-error count; ber_current_rate(&rx.ber) reports the rate.
