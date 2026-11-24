/* Source: Embedded DSP Handbook, Chapter 21 (Forward Error Correction): HB[104] QPSK soft LLR, HB[105] rate-1/2 K=7 convolutional encoder, HB[106] Viterbi ACS step,
 * HB[107] branch-metric construction + traceback, HB[108] rate-3/4 puncturing/de-puncturing, HB[109] block interleaver,
 * HB[110] GF(2^8) tables + multiply.  MIT License.
 * Host build: stdint.h is included; LLR_SCALE_Q and MAX_TRACEBACK_DEPTH are tuning constants with defaults, overridable with -D.
 * (HB[103] is a statement-level snippet, not a function: syntax-only, exercised in tests/test_D_fec.c.) */
#include "hb_fec.h"

// QPSK soft decision: I and Q each independently produce one bit's LLR, exactly the same
// BPSK-style computation run twice, once per axis, not a fundamentally different calculation.
void qpsk_soft_llr(int32_t i, int32_t q, int32_t *llr_bit0, int32_t *llr_bit1) {
    *llr_bit0 = (i * LLR_SCALE_Q) >> 15;   // confidence for the I-axis bit
    *llr_bit1 = (q * LLR_SCALE_Q) >> 15;   // confidence for the Q-axis bit
}

// The generators 171 and 133 (octal) are 1111001 and 1011011 in binary, with the newest
// input bit at the LEFT end. This register shifts the newest bit in at bit 0, so each
// mask is that pattern bit-reversed: 0x4F = 1001111 (117 octal) and 0x6D = 1101101
// (155 octal) are the taps of 171 and 133 as this register sees them.

uint32_t shift_reg = 0;

void conv_encode_bit(uint8_t input_bit, uint8_t *out0, uint8_t *out1) {
    shift_reg = ((shift_reg << 1) | (input_bit & 1)) & ((1u << K) - 1);  // keep only K bits
    // Generator polynomials 171 and 133 (octal) - standard rate-1/2 K=7 code
    *out0 = __builtin_parity(shift_reg & 0x4F);  // 171 octal, bit-reversed
    *out1 = __builtin_parity(shift_reg & 0x6D);  // 133 octal, bit-reversed
}

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

// Apply puncturing to a rate-1/2 encoder's bit stream, writing only the kept bits out.
int puncture_bits(const uint8_t *encoded_bits, int num_bits, uint8_t *punctured_out) {
    int out_idx = 0;
    for (int i = 0; i < num_bits; i++) {
        if (puncture_pattern_3_4[i % 6]) {
            punctured_out[out_idx++] = encoded_bits[i];
        }
    }
    return out_idx;  // number of bits actually transmitted
}

// At the receiver, before Viterbi decoding: re-insert erasures (a neutral, zero-confidence
// LLR) at every position the transmitter punctured, restoring the original rate-1/2 shape
// the Viterbi decoder above expects. The decoder still works correctly on an erasure, an
// LLR of exactly zero simply contributes no information to that position's branch metric.
int depuncture_llrs(const int32_t *received_llrs, int num_received, int32_t *depunctured_out) {
    int in_idx = 0, out_idx = 0;
    while (in_idx < num_received) {
        for (int i = 0; i < 6; i++) {
            if (puncture_pattern_3_4[i]) {
                // Guard against a final, partial puncture period: num_received isn't always
                // an exact multiple of the pattern's kept-bit count, and reading past the end
                // of received_llrs here is exactly the kind of one-off buffer overrun that's
                // easy to miss in testing but real in production once a packet length doesn't
                // land on a period boundary.
                depunctured_out[out_idx++] = (in_idx < num_received) ? received_llrs[in_idx++] : 0;
            } else {
                depunctured_out[out_idx++] = 0;
            }
        }
    }
    return out_idx;
}

// Block interleaver: write encoded bits into a matrix row by row, transmit column by column.
// A burst error hitting several consecutive TRANSMITTED bits (one column) lands on bits that
// were far apart in the ORIGINAL bit stream (spread across different rows), which is exactly
// the property that turns a burst error into scattered, individually correctable errors.
void interleave_block(const uint8_t *bits_in, uint8_t *bits_out, int rows, int cols) {
    for (int c = 0; c < cols; c++) {
        for (int r = 0; r < rows; r++) {
            bits_out[c * rows + r] = bits_in[r * cols + c];   // read column-major, write sequentially
        }
    }
}

// De-interleave at the receiver: the exact inverse permutation, restoring original bit order
// before depuncturing and Viterbi decoding.
void deinterleave_block(const uint8_t *bits_in, uint8_t *bits_out, int rows, int cols) {
    for (int c = 0; c < cols; c++) {
        for (int r = 0; r < rows; r++) {
            bits_out[r * cols + c] = bits_in[c * rows + r];
        }
    }
}

// GF(2^8) multiply via precomputed log/antilog tables - the primitive every RS encode/decode
// step above reduces to. gf_log[]/gf_exp[] are computed once, below, from the field's
// primitive polynomial (0x11d, x^8+x^4+x^3+x^2+1, the choice QR codes' Reed-Solomon
// uses), a fixed lookup table, not something recomputed per call. gf_exp is stored
// twice over (512 entries) because gf_mul() indexes it with a sum of two logs, at
// most 254 + 254 = 508.
void gf_tables_init(uint8_t gf_log[256], uint8_t gf_exp[512]) {
    unsigned x = 1;
    for (int i = 0; i < 255; i++) {
        gf_exp[i] = (uint8_t)x;
        gf_log[x] = (uint8_t)i;
        x <<= 1;                       // multiply by alpha = 2
        if (x & 0x100) x ^= 0x11d;     // reduce modulo the field polynomial
    }
    for (int i = 255; i < 512; i++) gf_exp[i] = gf_exp[i - 255];
    gf_log[0] = 0;   // log(0) is undefined; gf_mul() tests for a zero operand
}

// Multiply two field elements; zero has no logarithm, so it is handled first.
uint8_t gf_mul(uint8_t a, uint8_t b, const uint8_t *gf_log, const uint8_t *gf_exp) {
    if (a == 0 || b == 0) return 0;
    return gf_exp[gf_log[a] + gf_log[b]];   // multiply = add the logs, then look up the antilog
}
