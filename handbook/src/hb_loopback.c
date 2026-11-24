/* Source: Embedded DSP Handbook, Chapter 22 ("One complete loopback chain, wired end to end").
 * Complete digital-loopback QPSK receiver (AGC -> mixer -> RRC -> Gardner -> Costas -> slicer -> BER).
 * MIT License. */
#include "hb_loopback.h"

// Symbol-rate stage: called once the mid/current sample pair for one symbol is ready.
static uint8_t qpsk_loopback_process_symbol(qpsk_loopback_rx_t *rx, uint8_t tx_dibit) {
    int32_t i_mid = rx->hist_i[0], i_curr = rx->hist_i[1];
    int32_t q_mid = rx->hist_q[0], q_curr = rx->hist_q[1];

    // Gardner, run independently on I and Q and the two errors summed.
    // gardner_error() alone, as defined earlier in this chapter, is a
    // real-valued formula, so a QPSK/QAM receiver applies it to each rail.
    // The rails share one sampling clock, so either would steer it alone; the
    // sum doubles the detector gain Kd (TIMING_KD below) and averages the two
    // rails' data-pattern noise.
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

    if (++rx->symbols_processed == GARDNER_ONLY_SYMBOLS) {
        rx->costas_enabled = true;   // timing has had time to settle; bring carrier recovery online
    }
    return dibit;
}

// Sample-rate stage: called once per incoming loopback sample (from radio_get_rx_sample()
// above), at 2 samples/symbol. tx_dibit is only consulted on the second sample of each pair,
// once a full symbol's worth of matched-filter history is ready.
void qpsk_loopback_process_sample(qpsk_loopback_rx_t *rx, int32_t raw_sample, uint8_t tx_dibit) {
    // Apply the current gain first, then let the AGC measure what actually
    // came out: its own output power, which closes the loop. The squared
    // sample is a crude instantaneous power estimate; agc_update's own
    // attack/release shifts do the actual smoothing, and because attack is
    // faster than release the gain settles where the downward and upward
    // steps balance, below the level that puts the mean power exactly on
    // AGC_TARGET_POWER. Clip to 16 bits before squaring so the square fits an
    // int32_t.
    int32_t agc_sample = agc_apply(raw_sample, rx->agc.gain_q15);
    if (agc_sample > 32767) agc_sample = 32767;
    if (agc_sample < -32768) agc_sample = -32768;
    agc_update(&rx->agc, agc_sample * agc_sample);

    int32_t i_q30, q_q30;
    quadrature_mix(&rx->nco, (int16_t)agc_sample, &i_q30, &q_q30);
    int16_t i = mixer_to_q15(i_q30), q = mixer_to_q15(q_q30);   // Q30 mixer product -> Q15
    int32_t mf_i = fir_process_q15(&rx->rrc_i, i);
    int32_t mf_q = fir_process_q15(&rx->rrc_q, q);

    rx->hist_i[rx->sample_parity] = mf_i;
    rx->hist_q[rx->sample_parity] = mf_q;

    if (rx->sample_parity == 1) {
        qpsk_loopback_process_symbol(rx, tx_dibit);
    }
    rx->sample_parity ^= 1;
}

/* Host build: fills the NCO tables of this translation unit (see hb_loopback.h). */
void qpsk_loopback_init_tables(void) { nco_table_init(); }
