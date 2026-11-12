/* Gray-coded QPSK map and hard-decision slicer.
 * Source: Field Manual, Chapter 5. */
#ifndef FM_QPSK_H
#define FM_QPSK_H

#include "fm_qformat.h"

typedef struct { int16_t i, q; } iq_sample_t;

// Gray-coded QPSK: 2 bits -> one I/Q symbol, +/-0.707 (Q15-scaled unit-energy points)
static const iq_sample_t qpsk_table[4] = {
    { 23170,  23170},  // 00
    { 23170, -23170},  // 01
    {-23170,  23170},  // 10
    {-23170, -23170},  // 11
};

/* ---- function prototypes / shared state ---- */
iq_sample_t qpsk_modulate_symbol(uint8_t dibit );
uint8_t qpsk_demodulate_symbol(int32_t i, int32_t q);

#endif /* FM_QPSK_H */
