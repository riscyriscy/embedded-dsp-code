/*
 * Embedded DSP Handbook, Chapter 2: FIR and IIR Filters / Limit cycles: stable poles that still oscillate
 * Listing 7 of 119 (index 6, counting from 0 in book order), as printed in the 1.0 edition of the book.
 * Status: tested | Built as: include/hb_filters.h src/hb_filters.c | Checked by: tests/test_A_filters.c: q15_mul_round == floor(x+1/2), worked 0.6*-0.5 example (-9831 truncated / -9830 rounded), mean error 0 vs -0.5 LSB
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 * No longer printed in the book (November 2026): this listing is now a paragraph that points to Field Manual listing fm-014 (q15_mul_round). The text below is the former Handbook listing, kept so the compiled module still matches it.
 */
// q15_mul_trunc, defined back in Chapter 1's Q-format basics, is exactly the truncating
// shift with the bias problem described above. Rounding removes that bias with
// one extra add before the same shift, giving zero-mean error. It does not by
// itself stop limit cycles (see the discussion above):
int32_t q15_mul_round(int32_t a, int32_t b) {
    int64_t product = (int64_t)a * b;
    return (int32_t)((product + (1 << 14)) >> 15);
}
