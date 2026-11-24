/*
 * Embedded DSP Handbook, Chapter 16: Bluetooth and BLE: What the Module Is Doing / CRC and whitening: two packet-processing steps easy to conflate
 * Listing 97 of 119 (index 96, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_ble.c include/hb_ble.h | Checked by: tests/test_D_ble.c: CRC vs catalogue check values 29B1/31C3, binascii.crc_hqx vectors, bit-by-bit division, residue 0; whitening vs the spec register written two ways (channel 37: 8D D2 57 A1), self-inverse
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
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
