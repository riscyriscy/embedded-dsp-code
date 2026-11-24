/* Source: Embedded DSP Handbook, Chapter 14 (Spread Spectrum), HB[91] 4-bit LFSR PN generator + spread/despread, HB[92] early-late DLL error.
 * MIT License.
 * Host build: <stdint.h> is included. */
#ifndef HB_DSSS_H
#define HB_DSSS_H

#include <stdint.h>

// Maximal-length PN sequence generator via a linear feedback shift register (LFSR).
// This specific tap configuration (bits 0 and 3, for a 4-bit register) is illustrative -
// real designs pick taps from published primitive-polynomial tables for the desired length.
typedef struct { uint8_t state; } lfsr_t;   // 4-bit example; real PN sequences use longer registers

/* ---- function prototypes / shared state ---- */
uint8_t lfsr_next_chip(lfsr_t *l);
void dsss_spread_bit(lfsr_t *pn, uint8_t data_bit, int chip_ratio, int8_t *chips_out);
int32_t dsss_despread_bit(lfsr_t *pn, int8_t *received_chips, int chip_ratio);
int32_t dll_chip_error(const int8_t *early_corr_chips, const int8_t *late_corr_chips, const int8_t *pn_sequence, int len);

#endif /* HB_DSSS_H */
