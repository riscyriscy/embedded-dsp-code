/*
 * Embedded DSP Handbook, Chapter 8: Digital Demodulation In Depth / 1. Matched filtering: purpose and mechanism
 * Listing 70 of 119 (index 69, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: tests/shims/hb_C_snippet_69.inc (+ foundation hb_fir_q15) | Checked by: tests/test_C_ch8_matched.c (calls run verbatim on a 19-tap RRC; Q15 convolution and ISI-free cascade vs analytic RC)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// RRC coefficients are precomputed offline (same FIR machinery as Chapter 1's fir_q15_t/
// fir_process_q15) and applied as a normal Q15 FIR to each of I and Q independently -
// i_sample/q_sample already narrowed to Q15 by mixer_to_q15() if they came from the NCO
// mixer above, rather than the raw Q30 mixer product.
int16_t matched_filter_i = fir_process_q15(&rrc_filter_i, i_sample);
int16_t matched_filter_q = fir_process_q15(&rrc_filter_q, q_sample);
