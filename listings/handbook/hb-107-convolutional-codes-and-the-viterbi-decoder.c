/*
 * Embedded DSP Handbook, Chapter 18: Forward Error Correction (FEC) / Convolutional codes and the Viterbi decoder
 * Listing 108 of 119 (index 107, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_fec.c include/hb_fec.h | Checked by: tests/test_D_fec.c: branch metrics vs independent formula, erasure neutrality, traceback via the streaming harness, 24-bit burst with/without interleaver, soft-decision gain about 2 dB at BER 1e-3 (Monte-Carlo)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Branch metric construction: for a hypothetical transition into `state`, from one of its two
// predecessors, replicate what the encoder's two output bits WOULD have been for that exact
// transition, then score how well the received soft LLRs match that hypothesis. Lower score
// is a better match, consistent with viterbi_acs_step's own "lower is better" convention.
// The bit for any transition into `state` is fixed, state's own low bit (only one input bit
// is ever consistent with a given target state); predecessor0 vs predecessor1 differ in the
// other, older bits still feeding the generator polynomials, which is what actually changes
// the expected outputs between the two hypotheses.
void expected_encoder_outputs(int predecessor_state, int input_bit, uint8_t *out0, uint8_t *out1) {
    uint32_t shift_reg = ((uint32_t)predecessor_state << 1) | (input_bit & 1);
    *out0 = __builtin_parity(shift_reg & 0x4F);  // masks as in conv_encode_bit()
    *out1 = __builtin_parity(shift_reg & 0x6D);
}

// Building branch_metrics[state][0..1] for one received symbol: for every state, score both
// of its predecessors against the input bit that specific state's own low bit fixes, using
// this book's LLR sign convention (established earlier in this chapter: positive LLR =
// confident 0, negative = confident 1). A good match contributes a negative (low) cost;
// a bad match contributes positive (high) cost, scaled by how confident the LLR was.
void build_branch_metrics(int32_t branch_metrics[NUM_STATES][2], int32_t llr0, int32_t llr1) {
    for (int state = 0; state < NUM_STATES; state++) {
        int bit = state & 1;
        uint8_t exp0_a, exp1_a, exp0_b, exp1_b;
        expected_encoder_outputs(predecessor0(state), bit, &exp0_a, &exp1_a);
        expected_encoder_outputs(predecessor1(state), bit, &exp0_b, &exp1_b);
        branch_metrics[state][0] = (exp0_a ? llr0 : -llr0) + (exp1_a ? llr1 : -llr1);
        branch_metrics[state][1] = (exp0_b ? llr0 : -llr0) + (exp1_b ? llr1 : -llr1);
    }
}

// Traceback: find whichever state has the best (lowest) cumulative path metric, then walk
// backward following the recorded winner bits. The decoded bit at each step is simply that
// step's state's own low bit (the same fact that fixes which input bit any transition into a
// state must have used); the winner just says which predecessor to continue from for the step
// before that. Callable after every ACS step once the buffer has filled once, not just
// immediately after a wrap, which is exactly why it has to track v->depth explicitly below
// rather than assuming a fixed starting column.
int viterbi_traceback(viterbi_decoder_t *v, uint8_t *decoded_bits_out) {
    int best_state = 0;
    for (int state = 1; state < NUM_STATES; state++) {
        if (v->path_metric[state] < v->path_metric[best_state]) best_state = state;
    }
    int state = best_state;
    // v->depth (viterbi_acs_step's own comment) points one PAST the newest written column, so
    // the newest column itself is depth-1, wrapping. col has to walk backward in lockstep with
    // state, one column per predecessor step; assuming the newest column is always
    // MAX_TRACEBACK_DEPTH-1 (true only at the exact instant depth wraps to 0) silently decodes
    // the wrong columns at every other call, which for continuous streaming is almost every call.
    int col = (v->depth + MAX_TRACEBACK_DEPTH - 1) % MAX_TRACEBACK_DEPTH;
    for (int n = MAX_TRACEBACK_DEPTH - 1; n >= 0; n--) {
        decoded_bits_out[n] = state & 1;
        uint8_t winner = v->traceback[state][col];
        state = winner ? predecessor1(state) : predecessor0(state);
        col = (col + MAX_TRACEBACK_DEPTH - 1) % MAX_TRACEBACK_DEPTH;
    }
    return MAX_TRACEBACK_DEPTH;  // number of bits written to decoded_bits_out
}
