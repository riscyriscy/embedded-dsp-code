/* Source: Embedded DSP Handbook, Chapter 1 (The Goertzel algorithm and the sliding DFT).
 * MIT License. */
#ifndef HB_GOERTZEL_H
#define HB_GOERTZEL_H

#include <math.h>   /* cosf/sinf */

// Goertzel algorithm: compute DFT bin k (of an N-point DFT) from N real samples,
// in O(N) work with no per-sample complex multiply. w = 2*pi*k/N is fixed for a
// given target frequency and block length, so cos(w)/sin(w) are precomputed once,
// not recomputed per sample.
typedef struct {
    float coeff;      // 2*cos(w), precomputed
    float sin_w, cos_w;
    float s1, s2;      // resonator state: s1 = s[n-1], s2 = s[n-2]
} goertzel_t;

// Feed one sample; call this N times for one N-sample block.
static inline void goertzel_process_sample(goertzel_t *g, float x) {
    float s = x + g->coeff * g->s1 - g->s2;
    g->s2 = g->s1;
    g->s1 = s;
}

// When only tone PRESENCE/power matters (DTMF detection, energy at a known
// frequency), skip the final sin/cos multiply entirely and get power directly
// from the resonator state - this is the form most embedded tone detectors
// actually use, since it avoids two multiplies per detection, not just per sample:
static inline float goertzel_power(const goertzel_t *g) {
    return g->s1*g->s1 + g->s2*g->s2 - g->s1*g->s2*g->coeff;
}

/* ---- function prototypes / shared state ---- */
void goertzel_init(goertzel_t *g, int k, int N);
void goertzel_result(const goertzel_t *g, float *real, float *imag);

#endif /* HB_GOERTZEL_H */
