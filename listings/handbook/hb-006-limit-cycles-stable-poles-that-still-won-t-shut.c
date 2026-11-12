/*
 * Embedded DSP Handbook, Chapter 1: DSP Refresher for Embedded Systems / Limit cycles: stable poles that still won't shut up
 * Listing 7 of 119 (index 6, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_filters.h src/hb_filters.c | Checked by: tests/test_A_filters.c: q15_mul_round == floor(x+1/2), worked 0.6*-0.5 example (-9831 truncated / -9830 rounded), mean error 0 vs -0.5 LSB
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// q15_mul_trunc, defined back in Chapter 1's Q-format basics, is exactly the truncating
// shift with the bias problem described above. Rounding removes that bias with
// one extra add before the same shift, giving zero-mean error. It does not by
// itself stop limit cycles (see the discussion above):
int32_t q15_mul_round(int32_t a, int32_t b) {
    int64_t product = (int64_t)a * b;
    return (int32_t)((product + (1 << 14)) >> 15);
}
