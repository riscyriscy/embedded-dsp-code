/* Source: Embedded DSP Handbook, Chapter 3 (The Goertzel algorithm and the sliding DFT).
 * MIT License. */
#include "hb_goertzel.h"

void goertzel_init(goertzel_t *g, int k, int N) {
    float w = 2.0f * 3.14159265f * (float)k / (float)N;
    g->coeff = 2.0f * cosf(w);
    g->sin_w = sinf(w);
    g->cos_w = cosf(w);
    g->s1 = 0.0f;
    g->s2 = 0.0f;
}

// After N calls to goertzel_process_sample, read the complex bin value:
void goertzel_result(const goertzel_t *g, float *real, float *imag) {
    *real = g->s1 * g->cos_w - g->s2;
    *imag = g->s1 * g->sin_w;
}
