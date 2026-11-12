/* Soft QPSK LLRs, K=7 convolutional encoder, Viterbi ACS step.
 * Source: Field Manual, Chapter 9. */
#ifndef FM_FEC_H
#define FM_FEC_H

#include "fm_qformat.h"

// LLR gain in Q15: 16384 = 0.5, so the LLR is half the Q15 axis value.
// Match it to your decoder's metric range; i * LLR_SCALE_Q must fit an int32_t.
#define LLR_SCALE_Q 16384

#define K 7

#define NUM_STATES (1 << (K - 1))

static inline int predecessor0(int state) { return state >> 1; }

static inline int predecessor1(int state) { return (state >> 1) | (1 << (K - 2)); }

/* ---- function prototypes / shared state ---- */
void qpsk_soft_llr(int32_t i, int32_t q, int32_t *llr_bit1, int32_t *llr_bit0);
void conv_encode_bit(uint8_t input_bit, uint8_t *out0, uint8_t *out1);
void viterbi_acs_step(int32_t path_metric[NUM_STATES], int32_t branch_metrics[NUM_STATES][2], uint8_t winner_out[NUM_STATES]);
extern uint32_t shift_reg;

#endif /* FM_FEC_H */
