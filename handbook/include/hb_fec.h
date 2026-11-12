/* Source: Embedded DSP Handbook, Chapter 18 (Forward Error Correction): HB[104] QPSK soft LLR, HB[105] rate-1/2 K=7 convolutional encoder, HB[106] Viterbi ACS step,
 * HB[107] branch-metric construction + traceback, HB[108] rate-3/4 puncturing/de-puncturing, HB[109] block interleaver,
 * HB[110] GF(2^8) tables + multiply.  MIT License.
 * Host build: stdint.h is included; LLR_SCALE_Q and MAX_TRACEBACK_DEPTH are tuning constants with defaults, overridable with -D.
 * (HB[103] is a statement-level snippet, not a function: syntax-only, exercised in tests/test_D_fec.c.) */
#ifndef HB_FEC_H
#define HB_FEC_H

#include <stdint.h>

/* Tuning constants, overridable with -D.
 * LLR_SCALE_Q: the LLR gain in Q15 used by (i * LLR_SCALE_Q) >> 15; 32768 means a gain of exactly 1.0 (the LLR is the raw
 *   I/Q value).  The product i * LLR_SCALE_Q is computed in int32_t, so |i| * LLR_SCALE_Q must stay below 2^31.
 * MAX_TRACEBACK_DEPTH: the traceback window, 5x the constraint length (35 for this chapter's K=7 code, the Chapter 17
 *   memory-budget rule of thumb). */
#ifndef LLR_SCALE_Q
#define LLR_SCALE_Q 32768
#endif

#ifndef MAX_TRACEBACK_DEPTH
#define MAX_TRACEBACK_DEPTH 35
#endif

// Rate-1/2, constraint-length-7 convolutional encoder (the classic "NASA standard" code,
// used historically in satellite/deep-space and Wi-Fi systems)
#define K 7  // constraint length

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

// Rate-3/4 puncturing of a rate-1/2 encoder's output: a repeating 6-bit pattern that keeps
// 4 of every 6 encoded bits (3 input bits -> 4 transmitted bits = rate 3/4), a standard
// WiFi (802.11) puncture pattern for this rate. 1 = keep, 0 = puncture (drop).
static const uint8_t puncture_pattern_3_4[6] = { 1, 1, 1, 0, 0, 1 };

/* ---- function prototypes / shared state ---- */
void qpsk_soft_llr(int32_t i, int32_t q, int32_t *llr_bit0, int32_t *llr_bit1);
void conv_encode_bit(uint8_t input_bit, uint8_t *out0, uint8_t *out1);
void viterbi_acs_step(viterbi_decoder_t *v, int32_t branch_metrics[NUM_STATES][2]);
void expected_encoder_outputs(int predecessor_state, int input_bit, uint8_t *out0, uint8_t *out1);
void build_branch_metrics(int32_t branch_metrics[NUM_STATES][2], int32_t llr0, int32_t llr1);
int viterbi_traceback(viterbi_decoder_t *v, uint8_t *decoded_bits_out);
int puncture_bits(const uint8_t *encoded_bits, int num_bits, uint8_t *punctured_out);
int depuncture_llrs(const int32_t *received_llrs, int num_received, int32_t *depunctured_out);
void interleave_block(const uint8_t *bits_in, uint8_t *bits_out, int rows, int cols);
void deinterleave_block(const uint8_t *bits_in, uint8_t *bits_out, int rows, int cols);
uint8_t gf_mul(uint8_t a, uint8_t b, const uint8_t *gf_log, const uint8_t *gf_exp);
void gf_tables_init(uint8_t gf_log[256], uint8_t gf_exp[512]);
extern uint32_t shift_reg;

#endif /* HB_FEC_H */
