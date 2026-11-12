/*
 * Embedded DSP Handbook, Chapter 1: DSP Refresher for Embedded Systems / Lab 0: one known tone, start to finish
 * Listing 17 of 119 (index 16, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: examples/hb_lab0.c tests/shims/hbA_arm_math.h | Checked by: tests/test_A_lab0.c (21 checks): Hann window, 1000 Hz -> bin 32, 1050 Hz -> bin 34, bin-centred tones, scale and persistence claims
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#define LAB0_N 256   // one block, one FFT's worth; the DMA pattern above used 128 for its example

static dc_blocker_t   lab0_dcb = { .r_q15 = 32604 };       // R = 0.995, this chapter's DC blocker
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
