/* Source: Embedded DSP Handbook, Chapter 7 (The production version: arm_rfft_q15 and arm_cfft_q15, called directly).
 * Code from the Embedded DSP Handbook; MIT License.
 * REFERENCE ONLY: needs CMSIS-DSP (arm_math.h: q15_t, arm_rfft_instance_q15, arm_cfft_instance_q15, arm_rfft_init_q15,
 * arm_rfft_q15, arm_cfft_q15) and a LAB0_N definition supplied by Lab 0 (Chapter 4). Not part of any library build.
 * tests/test_B_cmsis_spy.c includes this file against a test-only stub (tests/shims/hb_B_arm_math_stub.h) to check that it
 * compiles and that the ifftFlag/bitReverseFlag arguments the comments describe are what is actually passed. */

// Q15 counterpart to Lab 0's arm_rfft_fast_f32() call (Chapter 4): same LAB0_N, Q15 samples in,
// what an actual FPU-less target runs in place of Lab 0's float path. The output buffer is NOT
// the same size or layout as lab0_fft_out above - see the comment on lab0_fft_out_q15 below.
static q15_t lab0_buf_q15[LAB0_N];       // windowed Q15 samples - filled the same way Lab 0's
                                          // float lab0_buf is, just carrying Q15 instead of float
static q15_t lab0_fft_out_q15[2 * LAB0_N];   // NOT LAB0_N: unlike arm_rfft_fast_f32() above,
                                          // arm_rfft_q15() does not fold DC and Nyquist into one
                                          // shared complex slot - CMSIS-DSP's own documentation
                                          // says so explicitly for the fixed-point RFFT - so this
                                          // buffer carries the fuller conjugate-symmetric output
                                          // instead of the compact packed layout. The exact size
                                          // and where DC/Nyquist land in it have both changed
                                          // across CMSIS-DSP releases and differ between MVE
                                          // (Helium) and non-MVE cores; 2 * LAB0_N is safely large
                                          // on every version checked for this book, but confirm
                                          // the precise layout against arm_rfft_instance_q15's own
                                          // current documentation for your exact library version
                                          // before indexing into it - not a number worth freezing
                                          // into this book across every future CMSIS-DSP release.
static arm_rfft_instance_q15 lab0_rfft_q15;

void lab0_q15_init(void) {
    // ifftFlagR = 0: forward transform. bitReverseFlag = 1: normal-order output - CMSIS-DSP's
    // own convention for this family is 1 = normal order, 0 = bit-reversed; easy to get backward,
    // and arm_rfft_fast_init_f32() above doesn't expose this argument at all, so it's easy to
    // forget it needs setting correctly here.
    arm_rfft_init_q15(&lab0_rfft_q15, LAB0_N, 0, 1);
}

void lab0_transform_q15(void) {
    arm_rfft_q15(&lab0_rfft_q15, lab0_buf_q15, lab0_fft_out_q15);
    // Every stage inside arm_rfft_q15() already shifted right by 1 bit internally - the same
    // block-floating-point convention this chapter derived by hand in butterfly_q15() above,
    // applied here by the library instead. lab0_fft_out_q15 is the true spectrum divided down by
    // a fixed, known power of two, not the transform itself; CMSIS-DSP's own documentation gives
    // the exact bit count for LAB0_N. Left-shift back by that count before comparing a bin's
    // magnitude against a threshold in physical units - skip it and this is Appendix D's "FFT
    // block-floating-point scale forgotten" bug, reproduced from a library call instead of
    // hand-rolled code.
}

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
