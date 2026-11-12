/*
 * Embedded DSP Handbook, Chapter 4: FFT Butterfly: What It Is / From butterflies to the full FFT
 * Listing 33 of 119 (index 32, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_fft.c include/hb_fft.h | Checked by: tests/test_B_fft.c: bit reversal vs bit-by-bit definition, fft vs naive double DFT N=8..1024, impulse, tone, Parseval, golden N=8 cosine
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Bit-reversal permutation: reorders the input so the iterative stage loop below can combine
// the right pairs at each step. Each index's bit pattern gets reversed (for N=8, index 1 = 001
// swaps with index 4 = 100). This is the one-time reordering the prose above names.
void bit_reverse_permute(complex_t *x, int n) {
    int bits = 0;
    while ((1 << bits) < n) bits++;
    for (int i = 0; i < n; i++) {
        int rev = 0;
        for (int b = 0; b < bits; b++) {
            if (i & (1 << b)) rev |= 1 << (bits - 1 - b);
        }
        if (rev > i) {
            complex_t tmp = x[i];
            x[i] = x[rev];
            x[rev] = tmp;
        }
    }
}

// The complete iterative radix-2 FFT: bit-reverse, then log2(N) stages, each stage running
// N/2 butterflies (the exact butterfly() from above) at that stage's twiddle factors. This is
// the real, assembled driver every other chapter's fft() call actually is; computing the
// twiddle angle with cosf/sinf per butterfly here is for clarity, a real target precomputes
// the whole twiddle table once (this chapter's own fixed-point FFT section, below) rather
// than calling cosf/sinf in the inner loop.
void fft(complex_t *input, complex_t *output, int n) {
    for (int i = 0; i < n; i++) output[i] = input[i];
    bit_reverse_permute(output, n);

    for (int stage_size = 2; stage_size <= n; stage_size *= 2) {
        int half = stage_size / 2;
        for (int group_start = 0; group_start < n; group_start += stage_size) {
            for (int k = 0; k < half; k++) {
                float angle = -2.0f * 3.14159265f * k / stage_size;
                complex_t w = { cosf(angle), sinf(angle) };
                butterfly(&output[group_start + k], &output[group_start + k + half], w);
            }
        }
    }
}
