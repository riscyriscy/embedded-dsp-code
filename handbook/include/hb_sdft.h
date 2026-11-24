/* Source: Embedded DSP Handbook, Chapter 3 (The Goertzel algorithm and the sliding DFT).
 * Sliding-DFT bin update.  MIT License. */
#ifndef HB_SDFT_H
#define HB_SDFT_H


// Sliding DFT: update DFT bin k by one sample at a time, given the bin value
// for the PREVIOUS window and the sample entering/leaving the window.
// X_new = (X_old - x[oldest_sample_leaving_window] + x[newest_sample]) * W
// where W = exp(+j * 2*pi*k / N) is a fixed unit-magnitude rotation, precomputed once.
typedef struct {
    float w_re, w_im;   // W = cos(2*pi*k/N) + j*sin(2*pi*k/N)
    float x_re, x_im;   // current bin value X[k] for the current window
} sdft_bin_t;

/* ---- function prototypes / shared state ---- */
void sdft_update(sdft_bin_t *b, float x_leaving, float x_entering);

#endif /* HB_SDFT_H */
