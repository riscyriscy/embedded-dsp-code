/*
 * Embedded DSP Handbook, Chapter 5: Fixed-Point Design Workflow / Closing the loop: exporting coefficients from the design tool to C
 * Listing 23 of 119 (index 22, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_fir_q15.h src/hb_fir_q15.c | Checked by: tests/test_A_fir_q15.c (30 checks): bit-exact vs int64 convolution with round-to-nearest, saturating; Step-7 peak error 2.8 LSB
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// The Q15 counterpart to Chapter 2's fir_filter_t: coefficients and history are native
// int16_t, matching exactly what design_filter.py exports, with the wider accumulator
// appearing only inside fir_process_q15 itself, never in the stored state.
typedef struct {
    int16_t coeffs[FIR_TAPS];
    int16_t history[FIR_TAPS];
    int idx;
} fir_q15_t;

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
