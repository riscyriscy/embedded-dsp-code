/*
 * Embedded DSP Handbook, Chapter 13: OFDM and Multicarrier Modulation / Per-subcarrier equalization and pilot subcarriers
 * Listing 87 of 119 (index 86, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_ofdm.c include/hb_ofdm.h | Checked by: tests/test_D_ofdm.c: equalize_subcarrier vs double complex division, H=0 non-finite note
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// One-tap equalizer per subcarrier: divide received symbol by estimated channel gain H_k.
complex_t equalize_subcarrier(complex_t received, complex_t channel_estimate_h) {
    float mag_sq = channel_estimate_h.re * channel_estimate_h.re
                 + channel_estimate_h.im * channel_estimate_h.im;
    complex_t h_conj = { channel_estimate_h.re, -channel_estimate_h.im };
    complex_t numerator = cmul(received, h_conj);     // cmul from Chapter 7
    complex_t result = { numerator.re / mag_sq, numerator.im / mag_sq };
    return result;
}
