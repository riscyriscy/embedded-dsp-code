/*
 * Embedded DSP Handbook, Chapter 4: FFT Butterfly: What It Is / The production version: arm_rfft_q15 and arm_cfft_q15, called directly
 * Listing 37 of 119 (index 36, counting from 0 in book order), exactly as it appears in the book.
 * Status: syntax-only | Built as: src/hb_cmsis_q15_ref.c tests/shims/hb_B_arm_math_stub.h | Checked by: tests/test_B_cmsis_spy.c: arm_cfft_q15(len256, buf, 0, 1) argument check via spy; library maths not testable
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Complex Q15 FFT: I/Q data in, transform in the same buffer out - in-place, unlike
// arm_rfft_q15() above. arm_cfft_sR_q15_len256 is CMSIS-DSP's own precomputed instance for
// N=256; matching LAB0_N here isn't required, just convenient for reusing the same N.
extern const arm_cfft_instance_q15 arm_cfft_sR_q15_len256;

void iq_transform_q15(q15_t *iq_interleaved) {
    // iq_interleaved holds { re0, im0, re1, im1, ... }, LAB0_N complex samples, and comes back
    // holding the transform in the same buffer - no separate output array the way arm_rfft_q15()
    // needs one. Same ifftFlag/bitReverseFlag convention as arm_rfft_q15() above (0/1 here means
    // forward, normal-order output), and the same per-stage right-shift accounting afterward.
    arm_cfft_q15(&arm_cfft_sR_q15_len256, iq_interleaved, 0, 1);
}
