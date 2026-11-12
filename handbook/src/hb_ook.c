/* Source: Embedded DSP Handbook, Chapter 12 (OOK, RFID and NFC), HB[93]: envelope magnitude (alpha-max-plus-beta-min) and adaptive-threshold OOK decision.
 * MIT License.
 * Host build: <stdint.h> is included.  (HB[94], tag_backscatter_bit(), is a HAL-dependent conceptual listing: reference-only, see reference/ and the full verbatim archive in ../listings/handbook/.) */
#include "hb_ook.h"

// Envelope from an I/Q pair (reuses the magnitude concept from the CORDIC/FM chapters)
int32_t envelope_magnitude(int32_t i, int32_t q) {
    // sqrt(i^2 + q^2) - exact via CORDIC MODULUS on STM32 (Chapter 6), or an
    // integer approximation (e.g. alpha-max-plus-beta-min) when no CORDIC is available.
    int32_t abs_i = (i < 0) ? -i : i;
    int32_t abs_q = (q < 0) ? -q : q;
    int32_t max_v = (abs_i > abs_q) ? abs_i : abs_q;
    int32_t min_v = (abs_i > abs_q) ? abs_q : abs_i;
    return max_v + (min_v * 3 / 8);   // cheap magnitude approximation, ~6.8% worst-case error
}

uint8_t ook_decide_bit(ook_detector_t *d, int32_t envelope) {
    uint8_t bit = (envelope > d->running_avg) ? 1 : 0;
    d->running_avg += (envelope - d->running_avg) >> 4;   // simple exponential average, tracks slow drift
    return bit;
}
