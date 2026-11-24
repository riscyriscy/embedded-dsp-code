/*
 * Embedded DSP Handbook, Chapter 14: Spread Spectrum: DSSS and FHSS / DSSS: spreading with a pseudorandom chip sequence
 * Listing 92 of 119 (index 91, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_dsss.c include/hb_dsss.h | Checked by: tests/test_D_dsss.c: recurrence o[t+4]=o[t]^o[t+3], period 15 for all seeds, 8 ones, autocorrelation 15/-1, seed-0 lock-up, spread/despread +-ratio, shifted phase -1, tone jammer 3x suppressed
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Maximal-length PN sequence generator via a linear feedback shift register (LFSR).
// This specific tap configuration (bits 0 and 3, for a 4-bit register) is illustrative -
// real designs pick taps from published primitive-polynomial tables for the desired length.
typedef struct { uint8_t state; } lfsr_t;   // 4-bit example; real PN sequences use longer registers

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
