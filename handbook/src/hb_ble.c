/* Source: Embedded DSP Handbook, Chapter 16 (Bluetooth and BLE), HB[95] GFSK bit decision (uses fm_demod_process from hb_fm_demod), HB[96] CRC-16-CCITT and whitening.
 * MIT License.
 * Host build: <stdint.h> and hb_fm_demod.h are included. */
#include "hb_ble.h"

// GFSK bit decision: reuse fm_demod_process() from Chapter 9, then threshold per bit period.
uint8_t gfsk_decide_bit(fm_demod_t *d, int32_t i, int32_t q) {
    int32_t freq_estimate = fm_demod_process(d, i, q);   // same function as Chapter 9's FM demod
    return (freq_estimate > 0) ? 1 : 0;   // positive frequency deviation = 1, negative = 0 (or vice versa)
}

// CRC-16-CCITT: a representative CRC, structurally the same idea as BLE's own CRC (a wider,
// 24-bit variant with a different polynomial). Run once over the packet at the transmitter,
// appended; recomputed identically at the receiver and compared against the appended value.
uint16_t crc16_ccitt(const uint8_t *data, int len, uint16_t crc) {
    for (int i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return crc;
}

// Whitening: XOR the payload against an LFSR-generated pseudo-random sequence. BLE uses
// the 7-bit LFSR x^7 + x^4 + 1 (Core Spec Vol 6, Part B, 3.2), initialised per channel:
// register position 0 = 1, positions 1..6 = the channel index, MSB in position 1.
// Position p lives in bit (6 - p) of a byte, so the seed is 0x40 | channel, the output
// (position 6) is bit 0, and each step shifts right and, if the output bit was 1, XORs
// in 0x44 (feedback into positions 0 and 4). Payload bits are whitened LSB first, the
// order BLE transmits them. Applying this same function again on receive exactly
// reverses it, the same XOR-is-its-own-inverse property as a one-time pad, just for
// spectral shaping here rather than any security purpose.
void whiten_payload(uint8_t *data, int len, uint8_t channel) {
    uint8_t lfsr = 0x40 | (channel & 0x3F);   // channel: BLE channel index 0..39
    for (int i = 0; i < len; i++) {
        uint8_t whitening_byte = 0;
        for (int b = 0; b < 8; b++) {
            uint8_t out_bit = lfsr & 1;
            lfsr >>= 1;
            if (out_bit) lfsr ^= 0x44;
            whitening_byte |= out_bit << b;
        }
        data[i] ^= whitening_byte;
    }
}
