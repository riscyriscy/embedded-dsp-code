/*
 * Embedded DSP Handbook, Chapter 19: Test and Validation Methodology / Error Vector Magnitude (EVM)
 * Listing 116 of 119 (index 115, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_testkit.c include/hb_testkit.h | Checked by: tests/test_D_testkit.c: EVM 10.00% for 10% error vectors, AWGN closed form 5.0%, double-precision RMS reference on 16-QAM
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// EVM computation: RMS error vs. nearest ideal constellation point, normalized by average symbol power.
float compute_evm_percent(complex_t *received, complex_t *ideal_nearest, int num_symbols) {
    float error_sum_sq = 0.0f, ref_sum_sq = 0.0f;
    for (int n = 0; n < num_symbols; n++) {
        float err_i = received[n].re - ideal_nearest[n].re;
        float err_q = received[n].im - ideal_nearest[n].im;
        error_sum_sq += err_i * err_i + err_q * err_q;
        ref_sum_sq += ideal_nearest[n].re * ideal_nearest[n].re
                    + ideal_nearest[n].im * ideal_nearest[n].im;
    }
    return 100.0f * sqrtf(error_sum_sq / ref_sum_sq);
}
