/*
 * Embedded DSP Handbook, Chapter 12: On-Off Keying (OOK), RFID, and NFC / On-Off Keying: the simplest possible modulation
 * Listing 94 of 119 (index 93, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_ook.c include/hb_ook.h | Checked by: tests/test_D_ook.c: exact cases, +6.80%/-2.79% worst-case error, Manchester OOK with adaptive threshold vs fixed under a 20 dB fade, >>4 quantization/dead-band notes (N-D4)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
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

// Adaptive-threshold OOK bit decision - fixed thresholds don't survive changing
// signal amplitude (distance, fading), so the threshold tracks a running average.
typedef struct { int32_t running_avg; } ook_detector_t;

uint8_t ook_decide_bit(ook_detector_t *d, int32_t envelope) {
    uint8_t bit = (envelope > d->running_avg) ? 1 : 0;
    d->running_avg += (envelope - d->running_avg) >> 4;   // simple exponential average, tracks slow drift
    return bit;
}
