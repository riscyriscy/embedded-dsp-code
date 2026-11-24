/* Source: Embedded DSP Handbook, Chapter 5 ("Closing the loop: exporting coefficients").
 * Q15 FIR, FIR_TAPS = 19 from tools/design_filter.py -> include/generated/coeffs.h. */
#include "hb_fir_q15.h"

void fir_init_q15(fir_q15_t *f, const int16_t *coeffs) {
    for (int i = 0; i < FIR_TAPS; i++) {
        f->coeffs[i] = coeffs[i];
        f->history[i] = 0;
    }
    f->idx = 0;
}

int16_t fir_process_q15(fir_q15_t *f, int16_t input) {
    f->history[f->idx] = input;

    int64_t acc = 0;   // Chapter 1's wide-accumulator rule: accumulate wide, shift back once
    int idx = f->idx;
    for (int i = 0; i < FIR_TAPS; i++) {
        acc += (int64_t)f->coeffs[i] * f->history[idx];
        idx = (idx == 0) ? FIR_TAPS - 1 : idx - 1;
    }

    f->idx = (f->idx + 1) % FIR_TAPS;

    // Saturate, don't wrap (Step 8 above): a bare narrowing cast would let a sum that
    // overflows int16_t silently wrap instead of clipping, exactly the failure that step
    // exists to prevent.
    // Round to nearest (add half an LSB before the shift): a bare >> floors,
    // which would bias every output by -1/2 LSB, the truncation bias Step 7
    // warns about.
    int64_t shifted = (acc + (1 << (Q15_SHIFT - 1))) >> Q15_SHIFT;
    if (shifted > 32767) return 32767;
    if (shifted < -32768) return -32768;
    return (int16_t)shifted;
}
