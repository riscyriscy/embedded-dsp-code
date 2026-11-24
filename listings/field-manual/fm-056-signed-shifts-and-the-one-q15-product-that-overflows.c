/*
 * Embedded DSP Field Manual, Chapter 12: Bugs and Golden Vectors / Signed shifts and the one Q15 product that overflows
 * Listing 57 of 61 (index 56, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: bench/shift_check.c | Checked by: compiles with host GCC 13.3.0 and arm-none-eabi-gcc 13.2.1 (the assertion holds); `make -C field-manual/bench shift`
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
_Static_assert((-1 >> 1) == -1, "this code needs an arithmetic right shift of negative values");
