/*
 * Embedded DSP Handbook, Chapter 8: Digital Demodulation In Depth / Preamble acquisition: finding the packet before locking onto anything
 * Listing 69 of 119 (index 68, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_preamble.h src/hb_preamble.c | Checked by: tests/test_C_ch8_preamble.c: correlation peak at the preamble end, vs direct correlation, noise rejection
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Sliding correlator: continuously compares the last N samples against a known preamble
// waveform, one multiply-accumulate per candidate alignment. Real implementations often use
// an FFT-based correlation for long preambles, this is the direct-form version for a short one.
#define PREAMBLE_LEN 32   // preamble length in samples (the N above)

typedef struct {
    int32_t history[PREAMBLE_LEN];
    int idx;
} preamble_corr_t;

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
