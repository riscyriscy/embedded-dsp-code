/*
 * Embedded DSP Handbook, Chapter 1: DSP Refresher for Embedded Systems / The Goertzel algorithm and the sliding DFT: when you only need one bin
 * Listing 15 of 119 (index 14, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_sdft.h src/hb_sdft.c | Checked by: tests/test_A_sdft.c (12 checks): exact vs direct DFT of the window, +j sign required, float drift over 2e6 samples (1e-5 -> 1e-3), hard reset cures it, damped variant (harness) bounded
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Sliding DFT: update DFT bin k by one sample at a time, given the bin value
// for the PREVIOUS window and the sample entering/leaving the window.
// X_new = (X_old - x[oldest_sample_leaving_window] + x[newest_sample]) * W
// where W = exp(+j * 2*pi*k / N) is a fixed unit-magnitude rotation, precomputed once.
typedef struct {
    float w_re, w_im;   // W = cos(2*pi*k/N) + j*sin(2*pi*k/N)
    float x_re, x_im;   // current bin value X[k] for the current window
} sdft_bin_t;

void sdft_update(sdft_bin_t *b, float x_leaving, float x_entering) {
    float diff = x_entering - x_leaving;   // real-valued input difference
    // (b->x + diff) * W, done as real/imag complex multiply
    float re = b->x_re + diff;
    float im = b->x_im;
    b->x_re = re * b->w_re - im * b->w_im;
    b->x_im = re * b->w_im + im * b->w_re;
}
