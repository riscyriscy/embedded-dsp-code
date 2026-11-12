/* GROUP A: formulas and worked numbers that the Handbook gives in code blocks (HB[0], [3], [11], [12], [25], [27]) checked against
 * independent calculations (integer arithmetic, closed forms, a naive double-precision DFT).  HB[12] is a call-site snippet and is
 * #included verbatim inside a harness function. */
#include <stdint.h>
#include "check.h"

/* ---- HB[11]: the DFT formula, its inverse and Parseval, by brute force ---------------------------------------------------------------- */
#define NN 32
static void dft(const double *xr, const double *xi, double *Xr, double *Xi, int n, int inverse) {
    for (int k = 0; k < n; k++) { double sr = 0, si = 0;
        for (int m = 0; m < n; m++) { double a = (inverse ? 2.0 : -2.0) * M_PI * (double)((long)k * m % n) / n; double c = cos(a), s = sin(a); sr += xr[m] * c - xi[m] * s; si += xr[m] * s + xi[m] * c; }
        Xr[k] = sr; Xi[k] = si; }
}
static void test_dft_formula(void) {
    double xr[NN], xi[NN], Xr[NN], Xi[NN], yr[NN], yi[NN]; lcg_state = 71;
    for (int i = 0; i < NN; i++) { xr[i] = ((int)(lcg_next() & 0xFFFF) - 32768) / 32768.0; xi[i] = 0.0; }
    dft(xr, xi, Xr, Xi, NN, 0);
    /* book: no 1/N in the forward transform; inverse carries (1/N): round trip returns x unscaled */
    dft(Xr, Xi, yr, yi, NN, 1); double worst = 0, worst_unscaled = 0;
    for (int i = 0; i < NN; i++) { worst = fmax(worst, fabs(yr[i] / NN - xr[i])); worst_unscaled = fmax(worst_unscaled, fabs(yr[i] - xr[i])); }
    CHECK(worst < 1e-12, "forward (no 1/N) then inverse with 1/N returns x: worst %g", worst);
    CHECK(worst_unscaled > 0.5, "...and WITHOUT the 1/N on the inverse the result is N times too big (worst deviation %g)", worst_unscaled);
    /* Parseval, book's form: sum |x|^2 = (1/N) sum |X|^2; and the 1/sqrt(N) split gives sum|x|^2 = sum|X|^2 */
    double ex = 0, eX = 0; for (int i = 0; i < NN; i++) { ex += xr[i] * xr[i]; eX += Xr[i] * Xr[i] + Xi[i] * Xi[i]; }
    CHECK_NEAR(ex, eX / NN, 1e-12, "Parseval: sum|x|^2 == (1/N) sum|X|^2");
    CHECK_NEAR(ex, eX / NN, 1e-9 * ex, "Parseval relative");
    { double s = 1.0 / sqrt((double)NN), e2 = 0; for (int i = 0; i < NN; i++) e2 += (s * Xr[i]) * (s * Xr[i]) + (s * Xi[i]) * (s * Xi[i]); CHECK_NEAR(e2, ex, 1e-12, "1/sqrt(N) convention: sum|x|^2 == sum|X|^2"); }
    /* real input: bins N-k are conjugate mirrors of bin k (book) */
    double mir = 0; for (int k = 1; k < NN / 2; k++) mir = fmax(mir, fmax(fabs(Xr[NN - k] - Xr[k]), fabs(Xi[NN - k] + Xi[k])));
    CHECK(mir < 1e-12, "bins N/2+1..N-1 are conjugate mirrors of 1..N/2-1: worst %g", mir);
    /* the k-th bin sits at k * fs / N: a tone at 5 bins peaks at k = 5 */
    for (int i = 0; i < NN; i++) xr[i] = cos(2 * M_PI * 5 * i / NN);
    dft(xr, xi, Xr, Xi, NN, 0); int pk = 1; for (int k = 1; k < NN / 2; k++) if (hypot(Xr[k], Xi[k]) > hypot(Xr[pk], Xi[pk])) pk = k;
    CHECK(pk == 5 && fabs(Xr[5] - NN / 2.0) < 1e-12, "cos at bin 5: peak bin %d with X = N/2 = %g", pk, Xr[5]);
}

/* ---- HB[12]: peak interpolation snippet, verbatim, inside a harness ----------------------------------------------------------------- */
static void windowed_mags(double hz, int N, double fs, int symmetric, double *mag) {
    double xr[256] = { 0 }, xi[256] = { 0 }, Xr[256] = { 0 }, Xi[256] = { 0 };
    for (int n = 0; n < N; n++) { double w = 0.5 - 0.5 * cos(2 * M_PI * n / (symmetric ? N - 1 : N)); xr[n] = sin(2 * M_PI * hz * n / fs) * w; xi[n] = 0; }
    dft(xr, xi, Xr, Xi, N, 0);
    for (int k = 0; k < N / 2; k++) mag[k] = hypot(Xr[k], Xi[k]);
}
static float refine(const double *mag, int N, double fs, int *kp, float *delta_out) {
    int k_peak = 1; for (int k = 1; k < N / 2 - 1; k++) if (mag[k] > mag[k_peak]) k_peak = k;
    float alpha = (float)mag[k_peak - 1], beta = (float)mag[k_peak], gamma = (float)mag[k_peak + 1];
#include "../reference/hb_peak_interp_snippet.inc"
    *kp = k_peak; *delta_out = delta; return refined_hz;
}
#undef NN
static void test_peak_interp(void) {
    double mag[256]; int kp; float delta; const int N = 64; const double fs = 8000.0;
    windowed_mags(1050.0, N, fs, 1, mag);
    float hz = refine(mag, N, fs, &kp, &delta);
    CHECK(kp == 8 && 1000.0 == kp * fs / N, "raw peak bin is %d = %g Hz, 50 Hz below the tone (book)", kp, kp * fs / N);
    CHECK_NEAR(1050.0 / (fs / N), 8.4, 1e-12, "1050 Hz is 0.4 bins above bin 8");
    CHECK_NEAR(delta, 0.359, 0.001, "delta (book: ~0.359; symmetric Hann, N-1 denominator)");
    CHECK_NEAR(hz, 1044.8, 0.1, "refined estimate (book: about 1044.8 Hz)");
    CHECK_NEAR(fabs(hz - 1050.0), 5.2, 0.1, "error after refinement (book: about 5.2 Hz)");
    CHECK(50.0 / fabs(hz - 1050.0) > 9.0 && 50.0 / fabs(hz - 1050.0) < 10.5, "improvement x%.1f (book: 'roughly 10x')", 50.0 / fabs(hz - 1050.0));
    /* with the periodic Hann the number is 0.357 / 1044.65 Hz: the claim does not depend on the window variant at the stated precision of 'about 5.2 Hz' within 0.2 Hz */
    windowed_mags(1050.0, N, fs, 0, mag); float hz2 = refine(mag, N, fs, &kp, &delta);
    CHECK_NEAR(hz2, 1044.8, 0.25, "periodic Hann (1044.65 Hz) is still 'about 1044.8'");
    /* delta stays in [-0.5, 0.5] and the estimate is within 0.1 bin across a sweep of tone positions between bin 8 and 9 */
    double worst = 0; int out_of_range = 0;
    for (int i = 0; i <= 20; i++) { double bins = 8.0 + 0.5 + (i - 10) * 0.045; windowed_mags(bins * fs / N, N, fs, 1, mag); float h = refine(mag, N, fs, &kp, &delta);
        if (delta < -0.5f || delta > 0.5f) out_of_range++;
        worst = fmax(worst, fabs(h / (fs / N) - bins)); }
    CHECK(out_of_range == 0, "delta stays within [-0.5, 0.5] (%d outside)", out_of_range);
    CHECK(worst < 0.1, "parabolic refinement error across 8.05..8.95 bins: worst %.3f bins", worst);
    /* sign: a tone BELOW the peak bin center gives negative delta */
    windowed_mags(7.7 * fs / N, N, fs, 1, mag); refine(mag, N, fs, &kp, &delta);
    CHECK(kp == 8 && delta < 0, "tone at 7.7 bins: peak %d, delta %.3f < 0", kp, delta);
}

/* ---- HB[0], HB[3], HB[27]: formulas with worked numbers --------------------------------------------------------------------------------------- */
static void test_formulas(void) {
    /* HB[0] f_s = f_TIM / (PSC+1) / (ARR+1): 80 MHz -> 8 kHz with PSC = ARR = 99 */
    { uint32_t ftim = 80000000u, psc = 99, arr = 99; uint32_t fs = ftim / (psc + 1) / (arr + 1);
      CHECK(fs == 8000u && 80000000.0 / 100 / 100 == 8000.0, "f_s = %u Hz", (unsigned)fs);
      int pairs = 0; for (uint32_t p = 0; p < 65536; p++) { if (10000u % (p + 1) == 0 && 10000u / (p + 1) - 1 < 65536) pairs++; }
      CHECK((psc + 1) * (arr + 1) == 10000u && pairs > 10, "(PSC+1)(ARR+1) = 10000 has %d (PSC, ARR) pairs with 16-bit registers ('many pairs hit the same f_s')", pairs); }
    /* HB[3] Kaiser: N ~= (A - 8) / (2.285 * d_omega); 1 kHz -> 2 kHz transition at fs = 8 kHz is 0.25 pi rad/sample */
    { double dw = 2 * M_PI * (2000.0 - 1000.0) / 8000.0; double N = (40.0 - 8.0) / (2.285 * dw);
      CHECK_NEAR(dw, 0.25 * M_PI, 1e-12, "delta_omega = 0.25 pi"); CHECK_NEAR(dw, 0.785, 1e-3, "= 0.785");
      CHECK(N > 17.7 && N < 18.0, "Kaiser estimate %.2f taps ('~= 18')", N); }
    /* HB[27] FMAC: max sample rate = Hclk / (2 N) */
    { double hclk = 275e6; CHECK(floor(hclk / (2.0 * 2e6)) == 68.0, "taps sustaining 2 Msps at 275 MHz: %.2f -> at most 68", hclk / (2.0 * 2e6));
      CHECK_NEAR(hclk / (2.0 * 127.0), 1.08e6, 0.005e6, "127 taps cap the rate at ~1.08 MHz (book)"); }
    /* prose arithmetic around Chapter 1: jitter-limited SNR and ENOB */
    CHECK_NEAR(pow(10.0, -74.0 / 20) / (2 * M_PI * 100e3) * 1e12, 318.0, 1.0, "t_j for 74 dB at 100 kHz: ~318 ps");
    CHECK_NEAR(pow(10.0, -74.0 / 20) / (2 * M_PI * 1e6) * 1e12, 32.0, 0.5, "t_j for 74 dB at 1 MHz: ~32 ps");
    CHECK_NEAR((68.0 - 1.76) / 6.02, 11.0, 0.01, "ENOB of a 68 dB ADC: ~11.0");
    CHECK_NEAR(6.02 * 12 + 1.76, 74.0, 0.05, "ideal 12-bit SNR 74.0 dB");
}

/* ---- HB[25]: the noise budget worksheet, every number recomputed ----------------------------------------------------------------------------------- */
static double comb_db(double a, double b) { return -10.0 * log10(pow(10.0, -a / 10.0) + pow(10.0, -b / 10.0)); }
static void test_worksheet(void) {
    double adc = 6.02 * 12 + 1.76;                       CHECK_NEAR(adc, 74.0, 0.05, "stage 1 ideal ADC SNR 74.0 dB");
    double usable = adc - 6.0;                           CHECK_NEAR(usable, 68.0, 0.05, "stage 1 after 6 dB AGC backoff: 68.0 dB");
    double q16 = 6.02 * 16 + 1.76;                       CHECK_NEAR(q16, 98.1, 0.05, "stage 2 16-bit truncation alone: 98.1 dB");
    double s2 = comb_db(usable, q16);                    CHECK_NEAR(s2, 68.0, 0.05, "stage 2 combined 'still 68.0 dB'");
    double pg = 10 * log10(8.0);                         CHECK_NEAR(pg, 9.0, 0.05, "stage 3 processing gain for sps = 8: +9.0 dB");
    double s3 = s2 + pg;                                 CHECK_NEAR(s3, 77.0, 0.1, "stage 3: 77.0 dB");
    double s4 = comb_db(s3, 27.0);                       CHECK_NEAR(s4, 27.0, 0.01, "stage 4 combined with the 27.0 dB equalizer residual: 27.0 dB");
    double target = 9.6 + 10 * log10(2.0);               CHECK_NEAR(target, 12.6, 0.05, "QPSK 1e-5 target in Es/N0: 9.6 + 3.0 = 12.6 dB");
    CHECK_NEAR(27.0 - 12.6, 14.4, 1e-9, "margin 14.4 dB");
    /* 9.6 dB Eb/N0 really is where uncoded QPSK/BPSK reaches 1e-5: BER = 0.5 erfc(sqrt(Eb/N0)) */
    CHECK_NEAR(log10(0.5 * erfc(sqrt(pow(10.0, 0.96)))), -5.0, 0.03, "uncoded QPSK BER at 9.6 dB Eb/N0 is 1e-5 (0.5 erfc(sqrt(Eb/N0)))");
    printf("  [info] worksheet exact values: stage2 %.3f, stage3 %.3f, stage4 %.3f dB\n", s2, s3, s4);
}

int main(void) {
    test_dft_formula(); test_peak_interp(); test_formulas(); test_worksheet();
    TEST_DONE("test_A_claims");
}
