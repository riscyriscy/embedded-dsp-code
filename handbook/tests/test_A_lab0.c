/* GROUP A: Lab 0 (HB[16]).  The book's listing (examples/hb_lab0.c) is #included into this translation unit and built against
 * tests/shims/hbA_arm_math.h (a naive double-precision DFT with CMSIS's packed rfft layout) instead of CMSIS-DSP, with the DC blocker /
 * one-pole LPF of HB[8] (src/hb_filters.c).  Claims from the prose: 1000 Hz -> bin 32; 1050 Hz -> bin 34 ("0.4 bins off ... still reads 34");
 * the peak search is scale-invariant but lab0_fft_out is inflated ~32768x; a cold filter makes the FFT see a transient; the text it prints. */
#define HB_HOST_TEST
#include "../examples/hb_lab0.c"
#include <unistd.h>
#include <string.h>
#include "check.h"

static void reset_filters(void) { lab0_dcb = (dc_blocker_t){ 0, 0, 32604 }; lab0_lpf = (one_pole_lpf_t){ 0, 25956 }; }
static void make_tone(int16_t *blk, int len, double hz, double amp, double dc, int start) {
    for (int n = 0; n < len; n++) blk[n] = (int16_t)lrint(dc + amp * sin(2.0 * M_PI * hz * (start + n) / 8000.0));
}
/* Run process_block, capturing its stdout text; returns the integer it wrote. */
static int run_capture(const int16_t *blk, int len) {
    fflush(stdout); int saved = dup(1); FILE *tmp = tmpfile(); dup2(fileno(tmp), 1);
    process_block(blk, len);
    fflush(stdout); dup2(saved, 1); close(saved);
    rewind(tmp); char line[128] = { 0 }; if (!fgets(line, sizeof line, tmp)) line[0] = 0; fclose(tmp);
    int v = -1; if (sscanf(line, "peak bin = %d", &v) != 1) v = -1; return v;
}
static int run_capture_len(const int16_t *blk, int len) { return run_capture(blk, len); }
static int argmax_fft_out(void) {
    int peak = 1; float pm = 0;
    for (int k = 1; k < LAB0_N / 2; k++) { float re = lab0_fft_out[2 * k], im = lab0_fft_out[2 * k + 1], m = re * re + im * im; if (m > pm) { pm = m; peak = k; } }
    return peak;
}

int main(void) {
    static int16_t blk[LAB0_N];
    lab0_init();
    /* --- Hann window as the listing computes it ------------------------------------------------------------------------------------ */
    { double worst = 0; for (int n = 0; n < LAB0_N; n++) { double w = 0.5 - 0.5 * cos(2.0 * M_PI * n / (LAB0_N - 1)); if (fabs(lab0_window[n] - w) > worst) worst = fabs(lab0_window[n] - w); }
      CHECK(worst < 1e-6, "Hann window vs double formula: worst %g", worst);
      int asym = 0; for (int n = 0; n < LAB0_N; n++) if (fabsf(lab0_window[n] - lab0_window[LAB0_N - 1 - n]) > 1e-6f) asym++;
      CHECK(asym == 0, "window is symmetric (N-1 denominator): %d asymmetric points", asym);
      CHECK_NEAR(lab0_window[0], 0.0, 1e-7, "w[0] = 0"); CHECK_NEAR(lab0_window[LAB0_N - 1], 0.0, 1e-6, "w[N-1] = 0"); CHECK(lab0_window[127] > 0.9999f, "w peaks at ~1"); }
    CHECK(LAB0_N == 256 && 8000.0 / LAB0_N == 31.25, "bin spacing fs/N = 31.25 Hz");
    CHECK((double)32604 / 32768 > 0.9949 && (double)32604 / 32768 < 0.9951, "R_q15 = 32604 is R = 0.995 (book)");
    { double a = 25956.0 / 32768, w = 2 * M_PI * 2700.0 / 8000.0;   /* the comment on lab0_lpf: -3 dB at ~2.7 kHz */
      CHECK_NEAR(20 * log10(a / hypot(1 - (1 - a) * cos(w), (1 - a) * sin(w))), -3.0, 0.15, "alpha 25956: gain at 2.7 kHz is -3 dB (closed form)"); }

    /* --- the book's two golden cases ------------------------------------------------------------------------------------------------ */
    reset_filters(); make_tone(blk, LAB0_N, 1000.0, 12000.0, 0.0, 0);
    int shown = run_capture(blk, LAB0_N);
    CHECK(shown == 32, "1000 Hz -> output 'peak bin = %d' (book: 32)", shown);
    CHECK(argmax_fft_out() == 32, "1000 Hz -> argmax of lab0_fft_out = %d", argmax_fft_out());
    reset_filters(); make_tone(blk, LAB0_N, 1050.0, 12000.0, 0.0, 0);
    shown = run_capture(blk, LAB0_N);
    CHECK(1050.0 / 31.25 > 33.59 && 1050.0 / 31.25 < 33.61, "1050 Hz is 33.6 bins, i.e. 0.4 bins from bin 34");
    CHECK(shown == 34, "1050 Hz -> output 'peak bin = %d' (book: should still read 34)", shown);

    /* --- every bin-centred tone reads its own bin; off-bin tones stay within one bin of the true position -------------------------------- */
    { int wrong = 0, first_wrong = -1;
      for (int k = 2; k <= 126; k++) { reset_filters(); make_tone(blk, LAB0_N, k * 31.25, 12000.0, 0.0, 0); run_capture_len(blk, LAB0_N) ; if (argmax_fft_out() != k) { wrong++; if (first_wrong < 0) first_wrong = k; } }
      CHECK(wrong == 0, "bin-centred tones k = 2..126: %d read the wrong bin (first: %d)", wrong, first_wrong);
      int far = 0;
      for (int k = 3; k <= 120; k++) { reset_filters(); double bins = k + 0.4; make_tone(blk, LAB0_N, bins * 31.25, 12000.0, 0.0, 0); run_capture_len(blk, LAB0_N);
          int p = argmax_fft_out(); if (fabs(p - bins) > 1.0) far++; }
      CHECK(far == 0, "tones 0.4 bins above bin k: %d peaks more than one bin from the true position", far); }
    /* bin-exact answer for an off-bin tone would mean something is wrong: peak magnitudes at 33 vs 34 for 1050 Hz differ */
    { reset_filters(); make_tone(blk, LAB0_N, 1050.0, 12000.0, 0.0, 0); run_capture_len(blk, LAB0_N);
      float m33 = hypotf(lab0_fft_out[66], lab0_fft_out[67]), m34 = hypotf(lab0_fft_out[68], lab0_fft_out[69]);
      CHECK(m34 > m33 && m33 > 0.5f * m34, "1050 Hz: |X[34]| = %g > |X[33]| = %g (both strong: tone sits between them)", m34, m33); }

    /* --- scale claim: magnitudes inflated by ~32768 relative to Q15_TO_FLOAT ------------------------------------------------------------- */
    { reset_filters(); make_tone(blk, LAB0_N, 1000.0, 12000.0, 0.0, 0); run_capture_len(blk, LAB0_N);
      /* expected |X[32]| = A * (sum of window / 2) * |H_dcb| * |H_lpf| at 1 kHz (steady state; the first block still contains the filters' transients, so allow 10%). */
      double w = 2 * M_PI * 1000.0 / 8000.0, a = 25956.0 / 32768, R = 32604.0 / 32768;
      double hl = a / hypot(1 - (1 - a) * cos(w), (1 - a) * sin(w)), hd = hypot(1 - cos(w), sin(w)) / hypot(1 - R * cos(w), R * sin(w));
      double wsum = 0; for (int n = 0; n < LAB0_N; n++) wsum += 0.5 - 0.5 * cos(2.0 * M_PI * n / (LAB0_N - 1));
      double expect = 12000.0 * wsum / 2.0 * hl * hd;
      double got = hypot(lab0_fft_out[64], lab0_fft_out[65]);
      CHECK(fabs(got - expect) < 0.1 * expect, "|X[32]| = %g vs closed form %g (A * sum(w)/2 * |H_dcb| * |H_lpf|)", got, expect);
      CHECK(got > 100000.0, "magnitude %g is in raw Q15-count units (~32768x a full-scale-normalised value of %g)", got, got / 32768.0); }

    /* --- persistence: 'lab0_dcb and lab0_lpf need to persist across calls'; a cold filter lets the DC blocker's transient swamp a weak tone ----- */
    { /* ADC-style input: weak 1 kHz tone (amplitude 200) on a 2000-count offset.  Warm = filters settled by earlier blocks. */
      static int16_t b[4 * LAB0_N];
      make_tone(b, 4 * LAB0_N, 1000.0, 200.0, 2000.0, 0);
      reset_filters(); for (int blkn = 0; blkn < 3; blkn++) run_capture_len(b + blkn * LAB0_N, LAB0_N);
      int warm = (run_capture_len(b + 3 * LAB0_N, LAB0_N), argmax_fft_out());
      /* state persisted: prev_x is the last sample of the previous block */
      CHECK(lab0_dcb.prev_x == b[4 * LAB0_N - 1], "dc blocker state persists across process_block calls (prev_x = %d)", (int)lab0_dcb.prev_x);
      reset_filters(); int cold = (run_capture_len(b + 3 * LAB0_N, LAB0_N), argmax_fft_out());
      CHECK(warm == 32, "settled filters: tone + offset reads bin %d (32)", warm);
      CHECK(cold != 32 && cold <= 3, "cold filters every block: the same block now reads bin %d (the DC blocker's 2000-count transient swamps the tone)", cold); }

    /* --- block-size mismatch: a short block leaves stale data in the upper half of lab0_buf, and a stronger stale tone can win ---------------- */
    { static int16_t half[LAB0_N / 2]; make_tone(half, LAB0_N / 2, 1000.0, 12000.0, 0.0, 0);
      memset(lab0_buf, 0, sizeof lab0_buf); reset_filters(); run_capture_len(half, LAB0_N / 2);
      int p = argmax_fft_out();
      CHECK(p == 32, "128-sample block into a cleared 256-point Lab 0 still peaks at bin %d (the bin axis comes from LAB0_N, not from len)", p);
      /* the hazard: the second half of lab0_buf keeps the PREVIOUS block's windowed data */
      reset_filters(); make_tone(blk, LAB0_N, 2000.0, 12000.0, 0.0, 0); run_capture_len(blk, LAB0_N);   /* leaves 2 kHz data in lab0_buf */
      make_tone(half, LAB0_N / 2, 1000.0, 1000.0, 0.0, 0);                                              /* weak 1 kHz, 128 samples */
      reset_filters(); run_capture_len(half, LAB0_N / 2);
      int q = argmax_fft_out();
      CHECK(q == 64, "weak 128-sample 1 kHz block after a strong 2 kHz block: the stale upper half wins, peak bin %d (2 kHz = bin 64)", q); }
    TEST_DONE("test_A_lab0");
}
