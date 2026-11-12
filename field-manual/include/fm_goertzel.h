/* Goertzel single-bin detector.
 * Source: Field Manual, Chapter 3. */
#ifndef FM_GOERTZEL_H
#define FM_GOERTZEL_H

#include <math.h>

typedef struct {
    float coeff;       // 2*cos(w), precomputed
    float sin_w, cos_w;
    float s1, s2;       // resonator state: s1 = s[n-1], s2 = s[n-2]
} goertzel_t;

// Call N times for one N-sample block.
static inline void goertzel_process_sample(goertzel_t *g, float x) {
    float s = x + g->coeff * g->s1 - g->s2;
    g->s2 = g->s1;
    g->s1 = s;
}

// Presence/power only (most tone detectors): skips the sin/cos multiply entirely.
static inline float goertzel_power(const goertzel_t *g) {
    return g->s1*g->s1 + g->s2*g->s2 - g->s1*g->s2*g->coeff;
}

/* ---- function prototypes / shared state ---- */
void goertzel_init(goertzel_t *g, int k, int N);
void goertzel_result(const goertzel_t *g, float *real, float *imag);

#endif /* FM_GOERTZEL_H */
