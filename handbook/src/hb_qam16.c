/* Source: Embedded DSP Handbook, Chapter 11 (4. Higher-order constellations).
 * qam16_axis_decision(), qam16_demodulate_symbol().  MIT License.
 * Host build: -std=c11 -Wpedantic has no binary literals, so the 2-bit codes are written in hex (binary in the comment). */
#include "hb_qam16.h"

// 16-QAM: each axis independently maps to one of {-3,-1,+1,+3} (normalized), 2 bits per axis.
// Gray-coded so adjacent amplitude levels differ by exactly one bit (matching the QPSK
// table's convention two chapters earlier): the most likely error, noise pushing a sample
// across the nearest decision boundary, then costs one bit, not two.
// level_spacing is the distance between adjacent levels (2 in this -3/-1/+1/+3 scaling),
// so the decision boundaries below sit at 0 and at +/-level_spacing.
uint8_t qam16_axis_decision(int32_t val, int32_t level_spacing) {
    if (val >  level_spacing)      return 0x2; /* 0b10 */ // +3 level
    else if (val >  0)             return 0x3; /* 0b11 */ // +1 level
    else if (val > -level_spacing) return 0x1; /* 0b01 */ // -1 level
    else                            return 0x0; /* 0b00 */ // -3 level
}

uint8_t qam16_demodulate_symbol(int32_t i, int32_t q, int32_t level_spacing) {
    uint8_t i_bits = qam16_axis_decision(i, level_spacing);
    uint8_t q_bits = qam16_axis_decision(q, level_spacing);
    return (i_bits << 2) | q_bits;
}
