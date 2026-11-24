/*
 * Embedded DSP Field Manual, Chapter 2: Filters You Ship / FIR: convolution, sized and generated
 * Listing 10 of 61 (index 9, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_fir.h src/fm_fir.c | Checked by: tests/test_filters.c, tests/test_vectors.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Q15 runtime type - native int16_t, matching what design_filter.py exports.
typedef struct {
    int16_t coeffs[FIR_TAPS];
    int16_t history[FIR_TAPS];
    int idx;
} fir_q15_t;

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
