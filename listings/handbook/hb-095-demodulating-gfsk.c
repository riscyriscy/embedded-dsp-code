/*
 * Embedded DSP Handbook, Chapter 16: Bluetooth and BLE: What the Module Is Doing / Demodulating GFSK
 * Listing 96 of 119 (index 95, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_ble.c include/hb_ble.h (links src/hb_fm_demod.c) | Checked by: tests/test_D_ble.c: ideal FSK h=0.5 and Gaussian BT=0.5 FSK, 0 wrong decisions; sign convention; zero deviation -> 0; dead zone below ~580 counts (N-D7)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// GFSK bit decision: reuse fm_demod_process() from Chapter 9, then threshold per bit period.
uint8_t gfsk_decide_bit(fm_demod_t *d, int32_t i, int32_t q) {
    int32_t freq_estimate = fm_demod_process(d, i, q);   // same function as Chapter 9's FM demod
    return (freq_estimate > 0) ? 1 : 0;   // positive frequency deviation = 1, negative = 0 (or vice versa)
}
