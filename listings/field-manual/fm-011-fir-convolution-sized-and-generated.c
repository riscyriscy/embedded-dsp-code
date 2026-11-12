/*
 * Embedded DSP Field Manual, Chapter 2: Filters You Ship / FIR: convolution, sized and generated
 * Listing 12 of 57 (index 11, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_fir.h src/fm_fir.c | Checked by: tests/test_filters.c: symmetric == direct on 2000 random samples
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
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
