/* Q15 helpers: FLOAT_TO_Q15, q15_mul_trunc, saturating adds.
 * Source: Embedded DSP Handbook, Chapter 1 (section 2, Quantization & fixed-point).
 * MIT License. */
#ifndef HB_QFORMAT_H
#define HB_QFORMAT_H


#include <stdint.h>

#include <math.h>   // lrintf, fmaxf, fminf below all come from here

// Named q15_wide_t, not q15_t, to avoid colliding with CMSIS-DSP's own q15_t
// (which is int16_t); this container is deliberately 32-bit to give multiply-
// accumulate headroom before you shift back down to a real 16-bit Q15 value.
typedef int32_t q15_wide_t;  // Q1.15 stored in low 16 bits, or full Q17.15 in 32-bit container

#define Q15_SHIFT 15

// Clamped to the actual int16_t range: a plain (f * 32768) would produce exactly 32768 for
// f=1.0, which overflows if the result is ever stored in a packed int16_t Q15 value (as
// fir_coeffs_q15 and FMAC's Q1.15 registers both are elsewhere in this book) even though it
// happens to fit fine in this wider q15_wide_t container. Clamping here means the same macro
// is safe to use regardless of which container the result ultimately lands in.
#define FLOAT_TO_Q15(f) ((int32_t)lrintf(fmaxf(-32768.0f, fminf((f) * (1 << Q15_SHIFT), 32767.0f))))

#define Q15_TO_FLOAT(q) ((float)(q) / (1 << Q15_SHIFT))

// Fixed-point multiply: two Q15 values multiplied give Q30, shift back to Q15. Named
// _trunc explicitly: this plain shift truncates (floors) rather than rounds.
// That is fine for a quick calculation, but it carries a one-directional bias
// of -1/2 LSB per product; in an IIR section's feedback path the bias becomes
// a DC error and can leave a stuck non-zero residue (Chapter 2's limit-cycle
// discussion). q15_mul_round below has zero mean error, the better default for
// that use.
static inline int32_t q15_mul_trunc(int32_t a, int32_t b) {
    int64_t result = (int64_t)a * (int64_t)b;
    return (int32_t)(result >> Q15_SHIFT);
}

// Saturates the WIDE accumulator container itself against a genuine int32_t wraparound -
// useful when chaining several q15_wide_t adds before the eventual shift back down to a real
// 16-bit Q15 value. Deliberately does NOT clamp to Q15's own [-32768, 32767]
// range: the wide container is allowed, on purpose, to run wider than that
// mid-accumulation.
static inline int32_t q15_wide_add_sat(int32_t a, int32_t b) {
    int64_t sum = (int64_t)a + (int64_t)b;
    if (sum > INT32_MAX) return INT32_MAX;
    if (sum < INT32_MIN) return INT32_MIN;
    return (int32_t)sum;
}

// The other saturating add most people actually mean by "Q15 saturating add": clamps to
// Q15's own [-32768, 32767] range, for two values already meant to be final,
// narrow Q15 numbers rather than wide accumulator state. Distinct from the
// wide version above on purpose - picking the wrong one either clips
// legitimate accumulator headroom before you're done accumulating, or lets a
// value meant to be a final Q15 result silently exceed its own representable
// range.
static inline int32_t q15_add_sat(int32_t a, int32_t b) {
    int32_t sum = a + b;
    if (sum > 32767) return 32767;
    if (sum < -32768) return -32768;
    return sum;
}

#endif /* HB_QFORMAT_H */
