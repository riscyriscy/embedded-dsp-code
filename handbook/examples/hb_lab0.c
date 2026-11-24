/* Source: Embedded DSP Handbook, Chapter 4 (Lab 0: one known tone, start to finish).
 * MIT License.
 * dc_blocker_t / one_pole_lpf_t come from hb_filters.h (HB[8]); arm_math.h is CMSIS-DSP on a target and
 * tests/shims/hbA_arm_math.h (naive DFT with the CMSIS packed layout) in the host build (-DHB_HOST_TEST).
 * The host test is tests/test_A_lab0.c, which includes this file. */
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#ifdef HB_HOST_TEST   /* host build: naive-DFT stand-in for CMSIS-DSP */
#include "hbA_arm_math.h"
#else
#include "arm_math.h"
#endif
#include "hb_filters.h"   /* dc_blocker_t, one_pole_lpf_t and their process functions (HB[8]) */

#define LAB0_N 256   // one block, one FFT's worth; the DMA pattern above used 128 for its example

static dc_blocker_t   lab0_dcb = { .r_q15 = 32604 };       // R = 0.995, Chapter 2's DC blocker
static one_pole_lpf_t lab0_lpf = { .alpha_q15 = 25956 };   // -3 dB at ~2.7 kHz (fs = 8 kHz)
static float lab0_window[LAB0_N];                          // Hann, computed once at init
static float lab0_buf[LAB0_N];
static float lab0_fft_out[LAB0_N];                          // CMSIS's packed real-FFT layout
static arm_rfft_fast_instance_f32 lab0_rfft;

void lab0_init(void) {
    arm_rfft_fast_init_f32(&lab0_rfft, LAB0_N);
    for (int n = 0; n < LAB0_N; n++) {
        lab0_window[n] = 0.5f - 0.5f * cosf(2.0f * 3.14159265f * n / (LAB0_N - 1));
    }
}

// process_block(), as called by main_loop_process_audio() in the DMA pattern above - this lab's
// entire pipeline, one ready block at a time.
void process_block(const int16_t *block, int len) {
    for (int n = 0; n < len; n++) {
        int32_t y = dc_blocker_process(&lab0_dcb, block[n]);
        y = one_pole_lpf_process(&lab0_lpf, y);
        lab0_buf[n] = (float)y * lab0_window[n];
    }

    arm_rfft_fast_f32(&lab0_rfft, lab0_buf, lab0_fft_out, 0);

    // output[0] = DC bin, output[1] = Nyquist bin, then (re, im) pairs for bins 1..N/2-1
    int peak_bin = 1;
    float peak_mag2 = 0.0f;
    for (int k = 1; k < LAB0_N / 2; k++) {
        float re = lab0_fft_out[2 * k];
        float im = lab0_fft_out[2 * k + 1];
        float mag2 = re * re + im * im;
        if (mag2 > peak_mag2) { peak_mag2 = mag2; peak_bin = k; }
    }

    printf("peak bin = %d\n", peak_bin);   // once per 32 ms block, not per sample - see above
}
