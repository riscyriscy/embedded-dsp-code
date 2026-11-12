/* Lab 0 golden vector: a 1 kHz tone sampled at 8 kHz, N = 256, must peak in FFT bin 32.
 * Compiles the book's listing (examples/lab0.c) verbatim into this translation unit, against
 * tests/shims/arm_math.h (naive DFT, CMSIS packed layout) instead of real CMSIS-DSP. */
#include "../examples/lab0.c"
#include "check.h"

static int run_tone(double hz) {
    int16_t blk[LAB0_N];
    for (int n = 0; n < LAB0_N; n++) blk[n] = (int16_t)lrint(12000.0 * sin(2.0 * M_PI * hz * n / 8000.0));
    /* Peak-bin search replicated from process_block's output through the FFT buffer it leaves behind. */
    process_block(blk, LAB0_N);
    int peak = 1; float pm = 0;
    for (int k = 1; k < LAB0_N / 2; k++) {
        float re = lab0_fft_out[2 * k], im = lab0_fft_out[2 * k + 1], m = re * re + im * im;
        if (m > pm) { pm = m; peak = k; }
    }
    return peak;
}

int main(void) {
    lab0_init();
    CHECK_NEAR(lab0_window[0], 0.0, 1e-6, "Hann window starts at 0");
    CHECK_NEAR(lab0_window[LAB0_N - 1], 0.0, 1e-5, "Hann window ends at 0");
    CHECK_NEAR(lab0_window[(LAB0_N - 1) / 2], 1.0, 1e-3, "Hann window peaks at ~1");
    int p = run_tone(1000.0);
    CHECK(p == 32, "1 kHz tone: peak bin = %d, expected 32 (1000 Hz / (8000/256) = 32.0)", p);
    /* Reset filter state between blocks so earlier tones don't leak into the next block. */
    lab0_dcb = (dc_blocker_t){ .r_q15 = 32604 }; lab0_lpf = (one_pole_lpf_t){ .alpha_q15 = 25956 };
    p = run_tone(500.0);
    CHECK(p == 16, "500 Hz tone: peak bin = %d, expected 16", p);
    TEST_DONE("test_lab0");
}
