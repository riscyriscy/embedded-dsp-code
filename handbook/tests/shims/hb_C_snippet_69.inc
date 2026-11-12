/* TEST-ONLY. HB[69]: the matched-filter call pair, verbatim (needs rrc_filter_i/q, i_sample, q_sample in scope),
 * #included inside a test function. */
// RRC coefficients are precomputed offline (same FIR machinery as Chapter 1's fir_q15_t/
// fir_process_q15) and applied as a normal Q15 FIR to each of I and Q independently -
// i_sample/q_sample already narrowed to Q15 by mixer_to_q15() if they came from the NCO
// mixer above, rather than the raw Q30 mixer product.
int16_t matched_filter_i = fir_process_q15(&rrc_filter_i, i_sample);
int16_t matched_filter_q = fir_process_q15(&rrc_filter_q, q_sample);
