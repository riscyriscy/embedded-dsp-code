/* Source: Embedded DSP Handbook, Chapter 2 (Closing the loop: exporting coefficients).
 * MIT License.
 * HB[23], usage snippet: the declaration is fine at file scope but fir_init_q15(...) is a statement, which is not valid at file scope.
 * Included verbatim inside a harness function by tests/test_A_fir_q15.c. */
// main.c or wherever the filter is instantiated - coeffs.h is generated, never hand-typed

fir_q15_t my_filter;
fir_init_q15(&my_filter, fir_coeffs_q15);
