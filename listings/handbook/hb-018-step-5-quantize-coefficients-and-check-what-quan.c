/*
 * Embedded DSP Handbook, Chapter 5: Fixed-Point Design Workflow / Step 5: Quantize coefficients, and check what quantization did
 * Listing 19 of 119 (index 18, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: (snippet) | Checked by: one-line call-site statement; FLOAT_TO_Q15(0.0731f) == 2395 asserted in tests/test_A_qformat.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
int16_t coeff_f = FLOAT_TO_Q15(0.0731f);  // from your float design
