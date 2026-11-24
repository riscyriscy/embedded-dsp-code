/*
 * Embedded DSP Field Manual, Chapter 5: I/Q and the Demod Chain / Exporting the RRC taps: design_rrc.py
 * Listing 31 of 61 (index 30, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/generated/rrc_coeffs.h | Checked by: generated; values asserted in tests/test_vectors.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
static const int16_t rrc_coeffs_q15[FIR_TAPS] = {
    -269, 47, 222, -590, 594, 1324, -3132, -1962, 14084,
    25388, 14084, -1962, -3132, 1324, 594, -590, 222, 47, -269
};
