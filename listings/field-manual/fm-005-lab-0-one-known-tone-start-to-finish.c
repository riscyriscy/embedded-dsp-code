/*
 * Embedded DSP Field Manual, Chapter 1: Sampling, Q-Format, and DMA / Lab 0: one known tone, start to finish
 * Listing 6 of 57 (index 5, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: examples/lab0.c | Checked by: tests/test_lab0.c (peak bin 32 at 1 kHz, against a naive-DFT arm_math.h shim)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#define LAB0_N 256

static dc_blocker_t   lab0_dcb = { .r_q15 = 32604 };       // R = 0.995, Chapter 2's DC blocker
static one_pole_lpf_t lab0_lpf = { .alpha_q15 = 25956 };   // fc ~ 2 kHz at fs = 8 kHz
static float lab0_window[LAB0_N];                          // Hann, computed once at init
static float lab0_buf[LAB0_N];
static float lab0_fft_out[LAB0_N];                          // CMSIS's packed real-FFT layout
static arm_rfft_fast_instance_f32 lab0_rfft;

void lab0_init(void) {
    arm_rfft_fast_init_f32(&lab0_rfft, LAB0_N);
    for (int n = 0; n < LAB0_N; n++)
        lab0_window[n] = 0.5f - 0.5f * cosf(2.0f * 3.14159265f * n / (LAB0_N - 1));
}

void process_block(const int16_t *block, int len) {
    // block[] already holds Q15 samples here - adc_to_q15() ran in the ISR/DMA handoff above,
    // the first of the two call sites that section names, not as a line inside this function.
    for (int n = 0; n < len; n++) {
        int32_t y = dc_blocker_process(&lab0_dcb, block[n]);
        y = one_pole_lpf_process(&lab0_lpf, y);
        lab0_buf[n] = (float)y * lab0_window[n];   // no Q15_TO_FLOAT - see below
    }
    arm_rfft_fast_f32(&lab0_rfft, lab0_buf, lab0_fft_out, 0);

    int peak_bin = 1;
    float peak_mag2 = 0.0f;
    for (int k = 1; k < LAB0_N / 2; k++) {
        float re = lab0_fft_out[2 * k], im = lab0_fft_out[2 * k + 1];
        float mag2 = re * re + im * im;
        if (mag2 > peak_mag2) { peak_mag2 = mag2; peak_bin = k; }
    }
    printf("peak bin = %d\n", peak_bin);   // once per block, from the main loop, not the ISR
}
