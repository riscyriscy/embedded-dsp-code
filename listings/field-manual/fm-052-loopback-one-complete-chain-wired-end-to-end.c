/*
 * Embedded DSP Field Manual, Chapter 10: Testing the Chain / Loopback: one complete chain, wired end to end
 * Listing 53 of 61 (index 52, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_loopback.h src/fm_loopback.c | Checked by: tests/test_chain.c (golden vector: 45 Hz offset, 0 bit errors from symbol 250)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#define GARDNER_ONLY_SYMBOLS 40   // let timing converge before the (narrower) carrier loop
                                   // comes online - adapting both loops from sample 1 fights itself

typedef struct {
    agc_t          agc;
    nco_t          nco;
    fir_q15_t      rrc_i, rrc_q;          // matched filter state, one per rail (Chapter 5)
    int32_t        hist_i[2], hist_q[2];  // [0] mid-symbol, [1] decision sample - at 2 sps this
                                           // pair IS what Gardner needs, no separate interpolator
    gardner_ted_t  ted_i, ted_q;
    timing_loop_t  timing_loop;
    costas_loop_t  costas;
    int32_t        mu;                    // diagnostic only: running Gardner timing-loop output.
                                           // This loopback is sample-synchronous (2 sps, no
                                           // fractional interpolator), so mu is NOT fed back into a
                                           // resampler here, and the reset block below leaves the
                                           // timing loop's kp/ki at 0, so mu stays 0 until you set
                                           // them. It is a hook to log, not a sub-sample position.
    int            sample_parity;
    uint32_t       symbols_processed;
    bool           costas_enabled;
    ber_counter_t  ber;
} qpsk_loopback_rx_t;

static uint8_t qpsk_loopback_process_symbol(qpsk_loopback_rx_t *rx, uint8_t tx_dibit) {
    int32_t i_mid = rx->hist_i[0], i_curr = rx->hist_i[1];
    int32_t q_mid = rx->hist_q[0], q_curr = rx->hist_q[1];

    // Gardner on I and Q independently, errors summed - locking against one rail alone
    // leaves the other one jittering.
    int32_t timing_error = gardner_error(&rx->ted_i, i_mid, i_curr)
                          + gardner_error(&rx->ted_q, q_mid, q_curr);
    rx->mu += timing_loop_update(&rx->timing_loop, timing_error);

    if (rx->costas_enabled) {
        int32_t phase_error = costas_phase_error(i_curr, q_curr);
        costas_update(&rx->costas, phase_error);
        costas_apply_correction(&rx->costas, &rx->nco);
    }

    uint8_t dibit = qpsk_demodulate_symbol(i_curr, q_curr);
    ber_test_bit(&rx->ber, (tx_dibit >> 1) & 1, (dibit >> 1) & 1);
    ber_test_bit(&rx->ber, tx_dibit & 1, dibit & 1);

    if (++rx->symbols_processed == GARDNER_ONLY_SYMBOLS) rx->costas_enabled = true;
    return dibit;
}

void qpsk_loopback_process_sample(qpsk_loopback_rx_t *rx, int32_t raw_sample, uint8_t tx_dibit) {
    // Apply the current gain first, then let the AGC measure what came out:
    // its feedback loop closes on the OUTPUT power (Q30, at most 1 << 30).
    int32_t agc_sample = agc_apply(raw_sample, rx->agc.gain_q15);
    int16_t out = agc_sample_to_q15(agc_sample);
    agc_update(&rx->agc, out * out);

    int32_t i_q30, q_q30;
    quadrature_mix(&rx->nco, out, &i_q30, &q_q30);   // Q15 x Q15 -> Q30
    int16_t i = mixer_to_q15(i_q30), q = mixer_to_q15(q_q30);        // rescale + saturate to Q15
    int32_t mf_i = fir_process_q15(&rx->rrc_i, i);
    int32_t mf_q = fir_process_q15(&rx->rrc_q, q);

    rx->hist_i[rx->sample_parity] = mf_i;
    rx->hist_q[rx->sample_parity] = mf_q;
    if (rx->sample_parity == 1) qpsk_loopback_process_symbol(rx, tx_dibit);
    rx->sample_parity ^= 1;
}

// Same "not from the real-time path" rule as dump_iq_buffer_uart above: call this from the
// main loop once per symbol, not from inside process_sample/process_symbol themselves. Two
// columns, symbol index and the two convergence numbers described above - enough for a host
// script to plot both time-series without touching the hot path at all.
void dump_loop_state_uart(const qpsk_loopback_rx_t *rx) {
    printf("%lu,%ld,%ld\n", (unsigned long)rx->symbols_processed,
           (long)rx->mu, (long)rx->costas.freq);
}
