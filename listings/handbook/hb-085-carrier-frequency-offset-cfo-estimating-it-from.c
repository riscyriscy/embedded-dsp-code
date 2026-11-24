/*
 * Embedded DSP Handbook, Chapter 13: OFDM and Multicarrier Modulation / Carrier frequency offset (CFO): estimating it from the CP itself
 * Listing 86 of 119 (index 85, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_ofdm.c include/hb_ofdm.h | Checked by: tests/test_D_ofdm.c: CP-correlation phase = -2 pi eps for 7 offsets, cfo_correct restores subcarriers, +-0.5 ambiguity (0.7 reads -0.3), integer offset invisible; N-D1
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// CFO estimate from the cyclic prefix's self-similarity: correlate the CP against the tail
// of the same OFDM symbol it was copied from, n samples later. The phase of that correlation
// is directly proportional to the carrier frequency offset over one OFDM symbol period.
complex_t cfo_estimate_from_cp(const complex_t *rx_with_cp, int n, int cp_len) {
    complex_t corr = { 0.0f, 0.0f };
    for (int i = 0; i < cp_len; i++) {
        complex_t a = rx_with_cp[i];                  // the CP itself
        complex_t b = rx_with_cp[n + i];               // the tail sample it was copied from
        complex_t b_conj = { b.re, -b.im };
        complex_t prod = cmul(a, b_conj);              // cmul from Chapter 7
        corr.re += prod.re;
        corr.im += prod.im;
    }
    // atan2 of this value (Chapter 9) is the CFO phase over n samples, already signed as a
    // correction: atan2(corr.im, corr.re) / n is the phase_per_sample for cfo_correct().
    return corr;
}

// Correct CFO by rotating every sample in the block by the estimated per-sample phase
// increment, the same NCO-driven rotation as Chapter 9's quadrature mixer, just derived
// from this measurement instead of a fixed known frequency.
void cfo_correct(complex_t *block, int n, float phase_per_sample) {
    float phase = 0.0f;
    for (int i = 0; i < n; i++) {
        float c = cosf(phase), s = sinf(phase);
        float re = block[i].re * c - block[i].im * s;
        float im = block[i].re * s + block[i].im * c;
        block[i].re = re;
        block[i].im = im;
        phase += phase_per_sample;
    }
}
