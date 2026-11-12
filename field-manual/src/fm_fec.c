/* Soft QPSK LLRs, K=7 convolutional encoder, Viterbi ACS step.
 * Source: Field Manual, Chapter 9. */
#include "fm_fec.h"

// Bit order matches qpsk_demodulate_symbol (Chapter 5) exactly: I is bit1, Q is bit0. A decoder
// fed llr_bit1/llr_bit0 in the other order silently swaps every symbol's two bits.
void qpsk_soft_llr(int32_t i, int32_t q, int32_t *llr_bit1, int32_t *llr_bit0) {
    *llr_bit1 = (i * LLR_SCALE_Q) >> 15;   // confidence for the I-axis bit
    *llr_bit0 = (q * LLR_SCALE_Q) >> 15;   // confidence for the Q-axis bit
}

uint32_t shift_reg = 0;

// The newest bit enters at bit 0, so the masks are the usual 171/133 octal
// generators (leftmost tap = the current input) written bit-reversed:
// 1111001 -> 1001111 = 0x4F, and 1011011 -> 1101101 = 0x6D.
void conv_encode_bit(uint8_t input_bit, uint8_t *out0, uint8_t *out1) {
    shift_reg = ((shift_reg << 1) | (input_bit & 1)) & ((1u << K) - 1);
    *out0 = __builtin_parity(shift_reg & 0x4F);   // 0171 octal generator
    *out1 = __builtin_parity(shift_reg & 0x6D);   // 0133 octal generator
}

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
