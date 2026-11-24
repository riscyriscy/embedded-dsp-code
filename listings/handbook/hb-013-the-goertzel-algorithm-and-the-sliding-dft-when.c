/*
 * Embedded DSP Handbook, Chapter 3: DFT and FFT / The Goertzel algorithm and the sliding DFT: when you only need one bin
 * Listing 14 of 119 (index 13, counting from 0 in book order), as printed in the 1.0 edition of the book.
 * Status: tested | Built as: include/hb_goertzel.h src/hb_goertzel.c | Checked by: tests/test_A_goertzel.c (29 checks): book N=8 example (4+0j), sign convention via sine, random blocks vs double DFT, DTMF table N=105, scalloping loss -3.46 dB measured vs -3.3 dB claimed
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 * In the book (November 2026), goertzel_process_sample, goertzel_result and goertzel_power are replaced by a comment that points to Field Manual listing fm-016. The text below is the full listing, kept so the compiled module still matches it.
 */
// Goertzel algorithm: compute DFT bin k (of an N-point DFT) from N real samples,
// in O(N) work with no per-sample complex multiply. w = 2*pi*k/N is fixed for a
// given target frequency and block length, so cos(w)/sin(w) are precomputed once,
// not recomputed per sample.
typedef struct {
    float coeff;      // 2*cos(w), precomputed
    float sin_w, cos_w;
    float s1, s2;      // resonator state: s1 = s[n-1], s2 = s[n-2]
} goertzel_t;

void goertzel_init(goertzel_t *g, int k, int N) {
    float w = 2.0f * 3.14159265f * (float)k / (float)N;
    g->coeff = 2.0f * cosf(w);
    g->sin_w = sinf(w);
    g->cos_w = cosf(w);
    g->s1 = 0.0f;
    g->s2 = 0.0f;
}

// Feed one sample; call this N times for one N-sample block.
static inline void goertzel_process_sample(goertzel_t *g, float x) {
    float s = x + g->coeff * g->s1 - g->s2;
    g->s2 = g->s1;
    g->s1 = s;
}

// After N calls to goertzel_process_sample, read the complex bin value:
void goertzel_result(const goertzel_t *g, float *real, float *imag) {
    *real = g->s1 * g->cos_w - g->s2;
    *imag = g->s1 * g->sin_w;
}

// When only tone PRESENCE/power matters (DTMF detection, energy at a known
// frequency), skip the final sin/cos multiply entirely and get power directly
// from the resonator state - this is the form most embedded tone detectors
// actually use, since it avoids two multiplies per detection, not just per sample:
static inline float goertzel_power(const goertzel_t *g) {
    return g->s1*g->s1 + g->s2*g->s2 - g->s1*g->s2*g->coeff;
}
