/* Chapter 9: coarse frequency estimate from the preamble (HB[80]).
 * References: complex tones of known frequency; a double-precision independent measurement of the rotation rate of the real
 * mixer's baseband output (tests the SIGN in the repo's own quadrature_mix() convention, not just the magnitude).
 * The estimate is the amount by which the received carrier sits ABOVE the NCO (add it to the NCO frequency to correct).
 * It is valid for a preamble that is constant or repeats with a period that divides `lag`; for a general known preamble
 * the samples must first be multiplied by the conjugate of the known symbols. */
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "hb_coarse_freq.h"
#include "hb_nco.h"

static void tone(complex_t *x, double f_hz, double fs, double phase0) {
    for (int n = 0; n < PREAMBLE_LEN; n++) { double a = 2 * M_PI * f_hz * n / fs + phase0; x[n].re = (float)(1000.0 * cos(a)); x[n].im = (float)(1000.0 * sin(a)); }
}

int main(void) {
    const double fs = 38400.0;
    /* ---- tone sweep: magnitude and sign over the unambiguous range |f| < fs / (2 lag) ----------------------------------- */
    const int lags[] = { 1, 2, 4, 8 };
    double worst = 0;
    for (unsigned li = 0; li < 4; li++) {
        const int lag = lags[li]; const double fmax = fs / (2.0 * lag);                  /* unambiguous range |f| < fs / (2 lag) */
        for (int k = -9; k <= 9; k++) {
            double f = fmax * k / 10.0; complex_t x[PREAMBLE_LEN]; tone(x, f, fs, 0.4);
            double est = coarse_freq_estimate_hz(x, lag, (float)fs);
            double e = fabs(est - f);
            if (e > worst) worst = e;
        }
    }
    printf("  [info] tone sweep: estimate vs true worst error %.4f Hz (range +-%.0f Hz at lag 1)\n", worst, fs / 2 * 0.9);
    CHECK(worst < 0.05, "estimate error %.4f Hz", worst);
    /* beyond the unambiguous range it aliases (as the maths says): 1.2 * fs/(2 lag) wraps to -0.8 * fs/(2 lag) */
    { complex_t x[PREAMBLE_LEN]; tone(x, 1.2 * fs / 8.0, fs, 0.0); CHECK_NEAR(coarse_freq_estimate_hz(x, 4, (float)fs), -0.8 * fs / 8.0, 0.1, "aliasing beyond fs/(2 lag)"); }
    /* noise: 10 dB SNR per sample, 400 trials: the estimator is unbiased and its spread shrinks with the preamble length -- just check unbiased */
    { double acc = 0; int N = 400; lcg_state = 11u;
      for (int t = 0; t < N; t++) { complex_t x[PREAMBLE_LEN]; tone(x, 1500.0, fs, 0.0);
          for (int n = 0; n < PREAMBLE_LEN; n++) { x[n].re += (float)((int)(lcg_next() % 801) - 400) * 0.55f; x[n].im += (float)((int)(lcg_next() % 801) - 400) * 0.55f; }
          acc += coarse_freq_estimate_hz(x, 4, (float)fs); }
      CHECK_NEAR(acc / N, 1500.0, 60.0, "estimator mean over noisy trials"); }

    /* ---- only for constant / periodic preambles ---------------------------------------------------------------- */
    {   /* QPSK-modulated random known preamble, true offset +1200 Hz, lag 4 */
        lcg_state = 91u; complex_t x[PREAMBLE_LEN]; const double f = 1200.0; double worst_err = 0;
        for (int trial = 0; trial < 30; trial++) {
            for (int n = 0; n < PREAMBLE_LEN; n++) { double a = 2 * M_PI * f * n / fs; double si = ((lcg_next() >> 4) & 1) ? 1 : -1, sq = ((lcg_next() >> 4) & 1) ? 1 : -1;
                x[n].re = (float)(1000.0 * (si * cos(a) - sq * sin(a))); x[n].im = (float)(1000.0 * (si * sin(a) + sq * cos(a))); }
            double e = fabs(coarse_freq_estimate_hz(x, 4, (float)fs) - f); if (e > worst_err) worst_err = e;
        }
        printf("  random QPSK preamble, true offset +1200 Hz, lag 4: worst estimate error over 30 preambles %.0f Hz\n", worst_err);
        CHECK(worst_err > 2000.0, "a random modulated preamble should not give a usable estimate without removing the modulation (worst error %.0f Hz)", worst_err);
        /* periodic preamble (period 4: +1 +1 -1 -1 style), lag = 4: data product is constant = |s|^2 and the estimate is right */
        const int pat_i[4] = { 1, 1, -1, -1 }, pat_q[4] = { 1, -1, -1, 1 };
        for (int n = 0; n < PREAMBLE_LEN; n++) { double a = 2 * M_PI * f * n / fs; double si = pat_i[n & 3], sq = pat_q[n & 3];
            x[n].re = (float)(1000.0 * (si * cos(a) - sq * sin(a))); x[n].im = (float)(1000.0 * (si * sin(a) + sq * cos(a))); }
        CHECK_NEAR(coarse_freq_estimate_hz(x, 4, (float)fs), f, 0.5, "period-4 preamble, lag 4: exact");
        /* and with a lag that is NOT a multiple of the period (lag 3) it fails again */
        CHECK(fabs(coarse_freq_estimate_hz(x, 3, (float)fs) - f) > 1000.0, "lag 3 on a period-4 preamble must fail (got %.0f)", coarse_freq_estimate_hz(x, 3, (float)fs));
    }

    /* ---- the sign in the repo's own convention: real passband -> quadrature_mix() -> baseband -> estimate -> correct the NCO ---- */
    {
        nco_table_init();
        const double f_nco = 4800.0, offset = 500.0, f_sig = f_nco + offset;                 /* fs = 19200: carrier at fs/4 + 500 Hz */
        const double fs2 = 19200.0;
        for (int variant = 0; variant < 2; variant++) {                                       /* 0: no correction, 1: estimate applied */
            nco_t nco; nco.phase_acc = 0; nco.base_inc = nco.phase_inc = nco_phase_inc_for_freq((float)f_nco, (float)fs2);
            complex_t z[PREAMBLE_LEN]; double meas_hz = 0.0;
            for (int pass = 0; pass < 2; pass++) {                                              /* pass 0 estimates, pass 1 re-measures after the correction */
                nco.phase_acc = 0;
                complex_t zz[PREAMBLE_LEN + 4]; static int16_t bi[PREAMBLE_LEN + 4], bq[PREAMBLE_LEN + 4];
                for (int n = 0; n < PREAMBLE_LEN + 4; n++) {
                    int16_t x = (int16_t)lrint(12000.0 * cos(2 * M_PI * f_sig * n / fs2 + 0.3));
                    int32_t i30, q30; quadrature_mix(&nco, x, &i30, &q30);
                    bi[n] = mixer_to_q15(i30); bq[n] = mixer_to_q15(q30);
                }
                /* lowpass: binomial (1 3 3 1)/8 -- triple zero at fs/2 removes the mixer's 2*fc image (fc = fs/4) to < 0.1 % */
                for (int n = 3; n < PREAMBLE_LEN + 4; n++) {
                    zz[n].re = (bi[n] + 3.0f * bi[n - 1] + 3.0f * bi[n - 2] + bi[n - 3]) / 8.0f;
                    zz[n].im = (bq[n] + 3.0f * bq[n - 1] + 3.0f * bq[n - 2] + bq[n - 3]) / 8.0f;
                }
                memcpy(z, zz + 4, sizeof(complex_t) * PREAMBLE_LEN);
                /* independent measurement (double): phase of sum z[n+1] conj(z[n]) -> rotation per sample */
                double sr = 0, si = 0;
                for (int n = 0; n + 1 < PREAMBLE_LEN; n++) { sr += (double)z[n + 1].re * z[n].re + (double)z[n + 1].im * z[n].im; si += (double)z[n + 1].im * z[n].re - (double)z[n + 1].re * z[n].im; }
                meas_hz = atan2(si, sr) * fs2 / (2 * M_PI);
                if (pass == 0) {
                    CHECK_NEAR(meas_hz, +offset, 3.0, "baseband from the repo's mixer rotates at +offset when the carrier is above the NCO (independent measurement)");
                    double est = (variant == 1) ? coarse_freq_estimate_hz(z, 4, (float)fs2) : 0.0;
                    if (variant == 1) CHECK_NEAR(est, +offset, 3.0, "estimate is +offset on the repo's own mixer output");
                    nco.phase_inc = nco.base_inc + (uint32_t)(int32_t)lrint(est / fs2 * 4294967296.0);    /* 'a starting correction' to phase_inc */
                }
            }
            printf("  [%s] carrier %+.0f Hz from the NCO; residual after applying the estimate to phase_inc: %+.1f Hz\n", variant == 0 ? "no correction" : "estimate     ", offset, meas_hz);
            if (variant == 0) CHECK_NEAR(meas_hz, offset, 3.0, "no correction: residual = offset");
            if (variant == 1) CHECK(fabs(meas_hz) < 6.0, "estimate leaves a residual of %.1f Hz", meas_hz);
        }
    }
    TEST_DONE("test_C_ch9_coarse");
}
