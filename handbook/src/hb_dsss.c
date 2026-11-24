/* Source: Embedded DSP Handbook, Chapter 14 (Spread Spectrum), HB[91] 4-bit LFSR PN generator + spread/despread, HB[92] early-late DLL error.
 * MIT License.
 * Host build: <stdint.h> is included. */
#include "hb_dsss.h"

uint8_t lfsr_next_chip(lfsr_t *l) {
    uint8_t feedback = ((l->state >> 0) ^ (l->state >> 3)) & 1;   // XOR the tapped bits
    uint8_t out_bit = l->state & 1;
    l->state = (l->state >> 1) | (feedback << 3);
    return out_bit;
}

// TX: spread one data bit into chip_ratio chips
void dsss_spread_bit(lfsr_t *pn, uint8_t data_bit, int chip_ratio, int8_t *chips_out) {
    for (int i = 0; i < chip_ratio; i++) {
        uint8_t pn_chip = lfsr_next_chip(pn);
        chips_out[i] = (data_bit ^ pn_chip) ? -1 : 1;   // BPSK-style +/-1 chip
    }
}

// RX: despread by correlating received chips against the same (synchronized) PN sequence
int32_t dsss_despread_bit(lfsr_t *pn, int8_t *received_chips, int chip_ratio) {
    int32_t correlation = 0;
    for (int i = 0; i < chip_ratio; i++) {
        uint8_t pn_chip = lfsr_next_chip(pn);
        int8_t expected = pn_chip ? -1 : 1;
        correlation += received_chips[i] * expected;
    }
    return correlation;  // strong positive/negative = confident 0/1; near zero = not synchronized or noisy
}

// Early-late DLL: the DSSS equivalent of Gardner's timing error detector, comparing
// correlation strength half a chip early and half a chip late against the current alignment.
// A positive error means the local code needs to shift earlier; negative means later.
int32_t dll_chip_error(const int8_t *early_corr_chips, const int8_t *late_corr_chips,
                         const int8_t *pn_sequence, int len) {
    int32_t early_energy = 0, late_energy = 0;
    for (int i = 0; i < len; i++) {
        early_energy += early_corr_chips[i] * pn_sequence[i];
        late_energy  += late_corr_chips[i]  * pn_sequence[i];
    }
    return early_energy - late_energy;  // feeds the same style of loop filter as Chapter 12
}
