/*
 * Embedded DSP Handbook, Chapter 10: OFDM and Multicarrier Modulation / The cyclic prefix
 * Listing 85 of 119 (index 84, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_ofdm.c include/hb_ofdm.h | Checked by: tests/test_D_ofdm.c: CP layout; 4-tap multipath with CP gives Y=HX, one-tap equalizer recovers all 64 QPSK subcarriers; no-CP negative control fails
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Add cyclic prefix: copy the last cp_len samples to the front of the block.
void add_cyclic_prefix(complex_t *ofdm_symbol, int n, int cp_len, complex_t *out_with_cp) {
    for (int i = 0; i < cp_len; i++) {
        out_with_cp[i] = ofdm_symbol[n - cp_len + i];   // copy the tail to the front
    }
    for (int i = 0; i < n; i++) {
        out_with_cp[cp_len + i] = ofdm_symbol[i];
    }
}

// RX: discard the CP, then FFT the remainder.
void remove_cp_and_demod(complex_t *rx_with_cp, int n, int cp_len, complex_t *freq_domain_out) {
    fft(&rx_with_cp[cp_len], freq_domain_out, n);   // fft(): forward FFT, same butterfly core as Ch. 4
}
