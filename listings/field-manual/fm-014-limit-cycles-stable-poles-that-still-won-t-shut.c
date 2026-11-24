/*
 * Embedded DSP Field Manual, Chapter 2: Filters You Ship / Limit cycles: stable poles that still won't shut up
 * Listing 15 of 61 (index 14, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_simple_filters.h | Checked by: tests/test_filters.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
int32_t q15_mul_round(int32_t a, int32_t b) {
    int64_t product = (int64_t)a * b;
    return (int32_t)((product + (1 << 14)) >> 15);
}
