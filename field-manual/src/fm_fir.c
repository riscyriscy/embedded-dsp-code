/* Q15 FIR (direct and symmetric) and zero-stuff interpolator.
 * Source: Field Manual, Chapter 2 (FIR) and Chapter 4 (interpolate_by_L).
 * Coefficients come from tools/design_filter.py via include/generated/coeffs.h. */
#include "fm_fir.h"

void fir_init_q15(fir_q15_t *f, const int16_t *coeffs) {
    for (int i = 0; i < FIR_TAPS; i++) { f->coeffs[i] = coeffs[i]; f->history[i] = 0; }
    f->idx = 0;
}

int16_t fir_process_q15(fir_q15_t *f, int16_t input) {
    f->history[f->idx] = input;

    int64_t acc = 0;   // accumulate wide, shift back once
    int idx = f->idx;
    for (int i = 0; i < FIR_TAPS; i++) {
        acc += (int64_t)f->coeffs[i] * f->history[idx];
        idx = (idx == 0) ? FIR_TAPS - 1 : idx - 1;
    }
    f->idx = (f->idx + 1) % FIR_TAPS;

    // Round to nearest before the shift, not a plain truncating shift - the same
    // one-sided-bias reason Chapter 2's limit-cycle section gives for q15_mul_round:
    // a bare >> here would push every output the same direction, never averaging out.
    int64_t shifted = (acc + (1LL << (Q15_SHIFT - 1))) >> Q15_SHIFT;    // round, then saturate
    if (shifted > 32767) return 32767;
    if (shifted < -32768) return -32768;
    return (int16_t)shifted;
}

// Valid only when coeffs[i] == coeffs[FIR_TAPS - 1 - i] for every i. Same fir_q15_t this
// chapter already ships - the symmetric trick is a cheaper *process* function, not a new type.
int16_t fir_process_symmetric_q15(fir_q15_t *f, int16_t input) {
    f->history[f->idx] = input;
    int64_t acc = 0;
    int i_lo = f->idx, i_hi = (f->idx + 1) % FIR_TAPS;
    for (int i = 0; i < FIR_TAPS / 2; i++) {
        acc += (int64_t)f->coeffs[i] * (f->history[i_lo] + f->history[i_hi]);
        i_lo = (i_lo == 0) ? FIR_TAPS - 1 : i_lo - 1;
        i_hi = (i_hi == FIR_TAPS - 1) ? 0 : i_hi + 1;
    }
    if (FIR_TAPS % 2 == 1) {
        // Odd tap count: the middle tap has no symmetric partner. After the loop above, i_lo
        // has walked down to exactly the center sample's position; add it once, unpaired. Skip
        // this silently and the center tap - 12339 of this chapter's own 19-tap vector - never
        // gets multiplied in at all.
        acc += (int64_t)f->coeffs[FIR_TAPS / 2] * f->history[i_lo];
    }
    f->idx = (f->idx + 1) % FIR_TAPS;
    int32_t shifted = (int32_t)((acc + (1LL << (Q15_SHIFT - 1))) >> Q15_SHIFT);   // round, then saturate
    if (shifted > 32767) return 32767;
    if (shifted < -32768) return -32768;
    return (int16_t)shifted;
}

void interpolate_by_L(fir_q15_t *f, int16_t input, int L, int16_t *outputs) {
    for (int k = 0; k < L; k++) {
        int16_t stuffed = (k == 0) ? input : 0;      // only the first slot carries the real sample
        int32_t scaled = (int32_t)fir_process_q15(f, stuffed) * L;
        if (scaled > INT16_MAX) scaled = INT16_MAX;
        else if (scaled < INT16_MIN) scaled = INT16_MIN;
        outputs[k] = (int16_t)scaled;
    }
}
