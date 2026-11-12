/* Source: Embedded DSP Handbook, Chapter 1 (The Goertzel algorithm and the sliding DFT).
 * Sliding-DFT bin update.  MIT License. */
#include "hb_sdft.h"

void sdft_update(sdft_bin_t *b, float x_leaving, float x_entering) {
    float diff = x_entering - x_leaving;   // real-valued input difference
    // (b->x + diff) * W, done as real/imag complex multiply
    float re = b->x_re + diff;
    float im = b->x_im;
    b->x_re = re * b->w_re - im * b->w_im;
    b->x_im = re * b->w_im + im * b->w_re;
}
