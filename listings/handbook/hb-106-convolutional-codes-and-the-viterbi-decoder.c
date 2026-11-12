/*
 * Embedded DSP Handbook, Chapter 18: Forward Error Correction (FEC) / Convolutional codes and the Viterbi decoder
 * Listing 107 of 119 (index 106, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_fec.c include/hb_fec.h | Checked by: tests/test_D_fec.c: ACS driven by a streaming harness (clean, 9 isolated errors, 3 in one constraint length, soft vs hard constructed case), traceback window oldest-first after wraps, 2240-byte traceback; N-D6 path-metric growth
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Simplified add-compare-select (ACS) core of a Viterbi decoder - the operation
// repeated for every state, every received symbol.

#define NUM_STATES (1 << (K - 1))  // K from the encoder above; one state per possible register content

typedef struct {
    int32_t path_metric[NUM_STATES];
    uint8_t traceback[NUM_STATES][MAX_TRACEBACK_DEPTH];
    int depth;  // which traceback column this step writes to, wraps at MAX_TRACEBACK_DEPTH
} viterbi_decoder_t;

// The two states that could transition into a given state, corresponding to the two possible
// values of the bit that shifted OUT of the predecessor state (the encoder's shift-in
// convention is `shift_reg = ((shift_reg << 1) | input_bit) & mask`, so state, the K-1 bit
// history the trellis tracks, gains a bit at the LOW end and loses its TOP bit each step;
// state's own top bit is exactly the bit a predecessor lost, unrecoverable from state alone,
// which is why there are two candidates rather than one). Right-shifting state by one
// recovers the K-2 bits every predecessor shares; the two predecessors differ only in
// whichever bit their own shift discarded, reintroduced at the top:
static inline int predecessor0(int state) { return state >> 1; }
static inline int predecessor1(int state) { return (state >> 1) | (1 << (K - 2)); }

void viterbi_acs_step(viterbi_decoder_t *v, int32_t branch_metrics[NUM_STATES][2]) {
    int32_t new_metric[NUM_STATES];
    for (int state = 0; state < NUM_STATES; state++) {
        // Two possible predecessor states/branches converge on each state;
        // pick whichever gives the lower (better) cumulative metric.
        int32_t metric0 = v->path_metric[predecessor0(state)] + branch_metrics[state][0];
        int32_t metric1 = v->path_metric[predecessor1(state)] + branch_metrics[state][1];
        uint8_t winner = (metric0 <= metric1) ? 0 : 1;  // which branch won, needed for traceback
        new_metric[state] = winner ? metric1 : metric0;
        v->traceback[state][v->depth] = winner;
    }
    for (int state = 0; state < NUM_STATES; state++) {
        v->path_metric[state] = new_metric[state];
    }
    v->depth = (v->depth + 1) % MAX_TRACEBACK_DEPTH;
}
