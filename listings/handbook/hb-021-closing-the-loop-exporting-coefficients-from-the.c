/*
 * Embedded DSP Handbook, Chapter 5: Fixed-Point Design Workflow / Closing the loop: exporting coefficients from the design tool to C
 * Listing 22 of 119 (index 21, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/generated/coeffs.h | Checked by: tests/test_A_fir_q15.c: equals the listed values, symmetric, equals round(32768*remez taps), 40 dB stopband / +-0.054 dB passband of the quantised set
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#define FIR_TAPS 19
static const int16_t fir_coeffs_q15[FIR_TAPS] = {
    -212, 5, 560, 678, -474, -1977, -1180, 3418, 9498,
    12339, 9498, 3418, -1180, -1977, -474, 678, 560, 5, -212
};
