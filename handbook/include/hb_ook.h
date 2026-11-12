/* Source: Embedded DSP Handbook, Chapter 12 (OOK, RFID and NFC), HB[93]: envelope magnitude (alpha-max-plus-beta-min) and adaptive-threshold OOK decision.
 * MIT License.
 * Host build: <stdint.h> is included.  (HB[94], tag_backscatter_bit(), is a HAL-dependent conceptual listing: reference-only, see reference/ and the full verbatim archive in ../listings/handbook/.) */
#ifndef HB_OOK_H
#define HB_OOK_H

#include <stdint.h>

// Adaptive-threshold OOK bit decision - fixed thresholds don't survive changing
// signal amplitude (distance, fading), so the threshold tracks a running average.
typedef struct { int32_t running_avg; } ook_detector_t;

/* ---- function prototypes / shared state ---- */
int32_t envelope_magnitude(int32_t i, int32_t q);
uint8_t ook_decide_bit(ook_detector_t *d, int32_t envelope);

#endif /* HB_OOK_H */
