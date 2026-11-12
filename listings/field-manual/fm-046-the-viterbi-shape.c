/*
 * Embedded DSP Field Manual, Chapter 9: FEC and the CPU Budget / The Viterbi shape
 * Listing 47 of 57 (index 46, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_fec.h src/fm_fec.c | Checked by: tests/test_fec.c (wrapped in a minimal decoder harness: encode, inject errors, decode)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#define NUM_STATES (1 << (K - 1))

static inline int predecessor0(int state) { return state >> 1; }
static inline int predecessor1(int state) { return (state >> 1) | (1 << (K - 2)); }

void viterbi_acs_step(int32_t path_metric[NUM_STATES], int32_t branch_metrics[NUM_STATES][2],
                       uint8_t winner_out[NUM_STATES]) {
    int32_t new_metric[NUM_STATES];
    for (int state = 0; state < NUM_STATES; state++) {
        int32_t metric0 = path_metric[predecessor0(state)] + branch_metrics[state][0];
        int32_t metric1 = path_metric[predecessor1(state)] + branch_metrics[state][1];
        winner_out[state] = (metric0 <= metric1) ? 0 : 1;
        new_metric[state] = winner_out[state] ? metric1 : metric0;
    }
    for (int state = 0; state < NUM_STATES; state++) path_metric[state] = new_metric[state];
}
