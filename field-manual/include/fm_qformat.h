/* Q-format macros and saturating helpers.
 * Source: Embedded DSP Field Manual, Chapter 1 ("Q-format and the macros you'll reuse everywhere").
 * MIT License. */
#ifndef FM_QFORMAT_H
#define FM_QFORMAT_H


#include <stdint.h>

#include <stdbool.h>   // bool/true/false - used from Chapter 4's CIC decimator onward

#include <math.h>      // lrintf, fmaxf, fminf

typedef int32_t q15_wide_t;  // wide accumulator container - see the Q-format cheat table below

#define Q15_SHIFT 15

#define FLOAT_TO_Q15(f) ((int32_t)lrintf(fmaxf(-32768.0f, fminf((f) * (1 << Q15_SHIFT), 32767.0f))))

#define Q15_TO_FLOAT(q) ((float)(q) / (1 << Q15_SHIFT))

// Truncating multiply: fine for a one-off calculation, a biased trap inside an IIR loop
// (see this book's limit-cycle box, Chapter 2).
static inline int32_t q15_mul_trunc(int32_t a, int32_t b) {
    int64_t result = (int64_t)a * (int64_t)b;
    return (int32_t)(result >> Q15_SHIFT);
}

// Saturates the WIDE container against a genuine int32_t wrap - for chaining several
// q15_wide_t adds before the eventual shift back to a real 16-bit Q15 value. Deliberately
// does NOT clamp to +-32767: the wide container is allowed to run past that mid-accumulation.
static inline int32_t q15_wide_add_sat(int32_t a, int32_t b) {
    int64_t sum = (int64_t)a + (int64_t)b;
    if (sum > INT32_MAX) return INT32_MAX;
    if (sum < INT32_MIN) return INT32_MIN;
    return (int32_t)sum;
}

// Clamps to Q15's own +-32767 range - for two values that are already meant to be final,
// narrow Q15 numbers, not wide accumulator state. Reaching for the wrong one of these two
// either clips legitimate headroom mid-accumulation, or lets a "final" value silently
// exceed its own range.
static inline int32_t q15_add_sat(int32_t a, int32_t b) {
    // assert(a >= -32768 && a <= 32767 && b >= -32768 && b <= 32767);  // <assert.h> - catches
    // a caller passing wide accumulator state into the narrow-Q15 saturate by mistake, in a
    // debug build; the two saturates above look interchangeable until this one is called on
    // something that was never a final, narrow value in the first place.
    int32_t sum = a + b;
    if (sum > 32767) return 32767;
    if (sum < -32768) return -32768;
    return sum;
}

#endif /* FM_QFORMAT_H */
