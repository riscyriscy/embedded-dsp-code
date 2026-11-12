/* Source: Embedded DSP Handbook, Chapter 9 (Differential PSK: sidestepping the ambiguity instead of detecting it).
 * dqpsk_decoder_t, dqpsk_decode_symbol().  MIT License.
 * Host build: -std=c11 -Wpedantic has no binary literals, so the 2-bit codes are written in hex (binary in the comment). */
#ifndef HB_DQPSK_H
#define HB_DQPSK_H

#include <stdint.h>

// Differential QPSK decode: phase difference between consecutive symbols carries the bits,
// not each symbol's absolute phase. Reuses the same atan2/CORDIC phase extraction from Chapter 6.
typedef struct { int32_t prev_phase_q31; } dqpsk_decoder_t;

/* ---- function prototypes / shared state ---- */
uint8_t dqpsk_decode_symbol(dqpsk_decoder_t *d, int32_t curr_phase_q31);

#endif /* HB_DQPSK_H */
