/*
 * Embedded DSP Handbook, Chapter 2: Fixed-Point Design Workflow / Closing the loop: exporting coefficients from the design tool to C
 * Listing 24 of 119 (index 23, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: reference/hb_fir_q15_usage_snippet.inc | Checked by: usage snippet: fir_init_q15(...) is a statement at file scope in the book (not valid C there); verbatim .inc run inside a harness in tests/test_A_fir_q15.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// main.c or wherever the filter is instantiated - coeffs.h is generated, never hand-typed
#include "coeffs.h"

fir_q15_t my_filter;
fir_init_q15(&my_filter, fir_coeffs_q15);
