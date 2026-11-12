/*
 * Embedded DSP Handbook, Chapter 10: OFDM and Multicarrier Modulation / Windowing: shaping the OFDM symbol's edges for spectral containment
 * Listing 90 of 119 (index 89, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_ofdm.c include/hb_ofdm.h | Checked by: tests/test_D_ofdm.c: window == 0.5(1-cos(pi i/W)) head and mirrored tail, middle untouched; N-D3
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Raised-cosine windowing applied to the edges of an OFDM symbol (post-IFFT, post-CP),
// tapering the first and last window_len samples smoothly instead of leaving hard edges.
// The tapered region typically overlaps into the cyclic prefix of the adjacent symbol
// rather than adding extra samples, so throughput isn't reduced to apply this.
void ofdm_apply_window(complex_t *symbol_with_cp, int total_len, int window_len) {
    for (int i = 0; i < window_len; i++) {
        float taper = 0.5f * (1.0f - cosf(3.14159265f * i / window_len));  // 0 -> 1 raised-cosine ramp
        symbol_with_cp[i].re *= taper;
        symbol_with_cp[i].im *= taper;
        int tail = total_len - 1 - i;
        symbol_with_cp[tail].re *= taper;
        symbol_with_cp[tail].im *= taper;
    }
}
