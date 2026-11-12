/* Source: Embedded DSP Handbook, Chapter 9 (Differential PSK: sidestepping the ambiguity instead of detecting it).
 * dqpsk_decoder_t, dqpsk_decode_symbol().  MIT License.
 * Host build: -std=c11 -Wpedantic has no binary literals, so the 2-bit codes are written in hex (binary in the comment). */
#include "hb_dqpsk.h"

uint8_t dqpsk_decode_symbol(dqpsk_decoder_t *d, int32_t curr_phase_q31) {
    // Subtract as uint32_t first: unsigned overflow wraps by a well-defined rule in C, unlike
    // signed int32_t subtraction, which only happens to wrap the same way in practice on most
    // compilers but is technically unspecified. Reinterpreting the wrapped unsigned result as
    // int32_t recovers the correct signed angle difference in [-pi, +pi) either way.
    int32_t phase_diff = (int32_t)((uint32_t)curr_phase_q31 - (uint32_t)d->prev_phase_q31);
    d->prev_phase_q31 = curr_phase_q31;

    // Quantize the wrapped phase difference (Q31, representing angle/pi from -1.0 to +1.0)
    // to the nearest of the four 90-degree steps, using the same counterclockwise Gray ring
    // from Chapter 6's QPSK table: 0 -> +90 -> 180 -> -90 -> back to 0, each step one bit flip.
    const int32_t QUARTER = 0x40000000;  // +90 degrees in Q31 (angle/pi = 0.5)
    const int32_t EIGHTH  = QUARTER / 2; // +45 degrees, the decision boundary between bins
    if (phase_diff > -EIGHTH && phase_diff <= EIGHTH) {
        return 0x0; /* 0b00 */  // ~0 degrees: no phase change
    } else if (phase_diff > EIGHTH && phase_diff <= QUARTER + EIGHTH) {
        return 0x2; /* 0b10 */  // ~+90 degrees
    } else if (phase_diff <= -EIGHTH && phase_diff > -(QUARTER + EIGHTH)) {
        return 0x1; /* 0b01 */  // ~-90 degrees
    } else {
        return 0x3; /* 0b11 */  // ~180 degrees (wraps either direction)
    }
}
