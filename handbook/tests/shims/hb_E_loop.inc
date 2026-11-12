/* TEST-ONLY. HB[118], second part: the driving loop, verbatim, #included inside a function after hb_E_init.inc.
 * (Needs tx_waveform, tx_dibit_stream and NUM_LOOPBACK_SAMPLES in scope.) */
for (int n = 0; n < NUM_LOOPBACK_SAMPLES; n++) {
    int32_t tx_sample = tx_waveform[n];        // TX chain output, oversampled at 2 samples/symbol
    int32_t rx_sample = radio_get_rx_sample(&radio_cfg, tx_sample);  // digital loopback, see above
    qpsk_loopback_process_sample(&rx, rx_sample, tx_dibit_stream[n / 2]);
}
// rx.ber now holds the accumulated bit-error count; ber_current_rate(&rx.ber) reports the rate.
