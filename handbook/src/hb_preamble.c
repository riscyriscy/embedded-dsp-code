/* Source: Embedded DSP Handbook, Chapter 8 (Preamble acquisition: finding the packet before locking onto anything).
 * preamble_corr_t, preamble_correlate().  MIT License. */
#include "hb_preamble.h"

int32_t preamble_correlate(preamble_corr_t *c, const int32_t *known_preamble, int32_t new_sample) {
    c->history[c->idx] = new_sample;
    int32_t idx = c->idx;
    int64_t corr = 0;
    // idx walks backwards from the newest sample, so the newest sample meets
    // the LAST preamble sample and the oldest meets known_preamble[0].
    // known_preamble[] is in transmission order, which makes this a true
    // correlation (indexed the other way it would be a FIR with those taps,
    // not a matcher).
    for (int i = 0; i < PREAMBLE_LEN; i++) {
        corr += (int64_t)c->history[idx] * known_preamble[PREAMBLE_LEN-1-i];
        idx = (idx == 0) ? PREAMBLE_LEN - 1 : idx - 1;
    }
    c->idx = (c->idx + 1) % PREAMBLE_LEN;
    return (int32_t)(corr >> 16);  // scale to your Q-format; compare against a detection threshold
}
