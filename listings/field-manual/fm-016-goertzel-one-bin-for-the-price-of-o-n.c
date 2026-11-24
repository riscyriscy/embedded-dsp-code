/*
 * Embedded DSP Field Manual, Chapter 3: FFT and Goertzel / Goertzel: one bin for the price of O(N)
 * Listing 17 of 61 (index 16, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_goertzel.h src/fm_goertzel.c | Checked by: tests/test_filters.c, tests/test_vectors.c (N=8 vector vs DFT)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
typedef struct {
    float coeff;       // 2*cos(w), precomputed
    float sin_w, cos_w;
    float s1, s2;       // resonator state: s1 = s[n-1], s2 = s[n-2]
} goertzel_t;

void goertzel_init(goertzel_t *g, int k, int N) {
    float w = 2.0f * 3.14159265f * (float)k / (float)N;
    g->coeff = 2.0f * cosf(w);
    g->sin_w = sinf(w);  g->cos_w = cosf(w);
    g->s1 = g->s2 = 0.0f;
}

// Call N times for one N-sample block.
static inline void goertzel_process_sample(goertzel_t *g, float x) {
    float s = x + g->coeff * g->s1 - g->s2;
    g->s2 = g->s1;
    g->s1 = s;
}

void goertzel_result(const goertzel_t *g, float *real, float *imag) {
    *real = g->s1 * g->cos_w - g->s2;
    *imag = g->s1 * g->sin_w;
}

// Presence/power only (most tone detectors): skips the sin/cos multiply entirely.
static inline float goertzel_power(const goertzel_t *g) {
    return g->s1*g->s1 + g->s2*g->s2 - g->s1*g->s2*g->coeff;
}
