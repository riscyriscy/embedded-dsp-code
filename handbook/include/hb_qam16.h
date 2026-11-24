/* Source: Embedded DSP Handbook, Chapter 11 (4. Higher-order constellations).
 * qam16_axis_decision(), qam16_demodulate_symbol().  MIT License.
 * Host build: -std=c11 -Wpedantic has no binary literals, so the 2-bit codes are written in hex (binary in the comment). */
#ifndef HB_QAM16_H
#define HB_QAM16_H

#include <stdint.h>

/* ---- function prototypes / shared state ---- */
uint8_t qam16_axis_decision(int32_t val, int32_t level_spacing);
uint8_t qam16_demodulate_symbol(int32_t i, int32_t q, int32_t level_spacing);

#endif /* HB_QAM16_H */
