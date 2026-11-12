/* Source: Embedded DSP Handbook, Chapter 13 (Bluetooth and BLE), HB[95] GFSK bit decision (uses fm_demod_process from hb_fm_demod), HB[96] CRC-16-CCITT and whitening.
 * MIT License.
 * Host build: <stdint.h> and hb_fm_demod.h are included. */
#ifndef HB_BLE_H
#define HB_BLE_H

#include <stdint.h>
#include "hb_fm_demod.h"

/* ---- function prototypes / shared state ---- */
uint8_t gfsk_decide_bit(fm_demod_t *d, int32_t i, int32_t q);
uint16_t crc16_ccitt(const uint8_t *data, int len, uint16_t crc);
void whiten_payload(uint8_t *data, int len, uint8_t channel);

#endif /* HB_BLE_H */
