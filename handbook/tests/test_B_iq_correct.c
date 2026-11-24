/* HB[59]: iq_dc_correct() (per-rail Chapter 2 DC blocker) and iq_imbalance_correct() (src/hb_iq_correct.c; links src/hb_filters.c).
 * References: closed-form DC-blocker gain |1-e^{-jw}|/|1-r e^{-jw}|, and an explicit impairment model that the 2x2 matrix undoes:
 *   I' = I,   Q' = g (Q cos(phi) + I sin(phi))     (g = gain of the Q path, phi = phase error away from 90 degrees).
 * This model is inverted EXACTLY by q = (q' - beta i')/alpha with alpha = g cos(phi), beta = g sin(phi).
 * iq_dc_correct() only applies Chapter 2's dc_blocker_process() to each rail; the checks on how completely the blocker removes a
 * constant (marked "blocker behaviour") exercise that Chapter 2 code through the I/Q wrapper, the rest is independent of it. */
#include <stdint.h>
#include <complex.h>
#include <string.h>
#include "check.h"
#include "hb_iq_correct.h"

#define R_Q15 32604   /* 0.995 in Q15 = round(0.995 * 32768) */

static void init_corrector(iq_dc_corrector_t *c) { memset(c, 0, sizeof *c); c->i_blocker.r_q15 = c->q_blocker.r_q15 = R_Q15; }

int main(void) {
    /* ---- DC offset removal: offsets of 3000 / -2000 under a 1000-amplitude tone ---- */
    {
      iq_dc_corrector_t c; init_corrector(&c);
      const double DCI = 3000.0, DCQ = -2000.0, A = 1000.0, f = 1.0 / 64.0;
      const int N = 64 * 60; double sumI = 0, sumQ = 0; int cnt = 0;
      double toneI_re = 0, toneI_im = 0;
      for (int n = 0; n < N; n++) {
          int32_t i = (int32_t)lrint(DCI + A * cos(2.0 * M_PI * f * n)), q = (int32_t)lrint(DCQ + A * sin(2.0 * M_PI * f * n));
          iq_dc_correct(&c, &i, &q);
          if (n >= 64 * 40) { sumI += i; sumQ += q; cnt++; toneI_re += i * cos(2.0 * M_PI * f * n); toneI_im -= i * sin(2.0 * M_PI * f * n); }
      }
      /* after 40 periods (2560 samples) 0.995^2560 = 2.6e-6 of the offset is left: anything bigger is arithmetic bias */
      printf("  [info] DC correction: residual mean I = %.2f (offset 3000), Q = %.2f (offset -2000)\n", sumI / cnt, sumQ / cnt);
      CHECK(fabs(sumI / cnt) < 1.0, "blocker behaviour: I rail residual DC %g after settling", sumI / cnt);
      CHECK(fabs(sumQ / cnt) < 1.0, "blocker behaviour: Q rail residual DC %g after settling", sumQ / cnt);
      /* the tone passes with the closed-form gain of the blocker */
      double w = 2.0 * M_PI * f, r = R_Q15 / 32768.0; double complex hg = (1.0 - cexp(-I * w)) / (1.0 - r * cexp(-I * w));
      double amp = 2.0 * hypot(toneI_re, toneI_im) / cnt;
      CHECK_NEAR(amp, A * cabs(hg), 4.0, "tone amplitude after the DC blocker = A|H(f)|");
    }
    /* a constant offset of either sign decays all the way to 0 */
    { iq_dc_corrector_t c; init_corrector(&c); int32_t yp = 0, yn = 0;
      for (int n = 0; n < 6000; n++) { int32_t i = 10000, q = -10000; iq_dc_correct(&c, &i, &q); yp = i; yn = q; }
      CHECK(yp == 0 && yn == 0, "blocker behaviour: constant +10000 / -10000 decays to 0 (got %d / %d)", yp, yn);
      }
    /* the wrapper is exactly one Chapter 2 blocker per rail: same outputs, independent state */
    { iq_dc_corrector_t c; init_corrector(&c); dc_blocker_t bi, bq; memset(&bi, 0, sizeof bi); memset(&bq, 0, sizeof bq); bi.r_q15 = bq.r_q15 = R_Q15;
      int same = 1;
      for (int n = 0; n < 2000; n++) {
          int32_t i = (int32_t)(lcg_next() % 20001) - 10000, q = (int32_t)(lcg_next() % 20001) - 10000, i0 = i, q0 = q;
          iq_dc_correct(&c, &i, &q);
          if (i != dc_blocker_process(&bi, i0) || q != dc_blocker_process(&bq, q0)) same = 0;
      }
      CHECK(same, "iq_dc_correct == dc_blocker_process on each rail, rails independent"); }
    /* settling: a DC step decays like r^n (Chapter 2: "closer to 1.0 = slower settling"); closed form 10000 r^n */
    { iq_dc_corrector_t c; init_corrector(&c); int n99 = -1;
      for (int n = 0; n < 3000; n++) {
          int32_t i = 10000, q = 0;
          iq_dc_correct(&c, &i, &q);
          if (n99 < 0 && i < 100) { n99 = n; }
          CHECK(q == 0, "Q rail untouched by a DC step on I (independent rails)");
      }
      double r = R_Q15 / 32768.0; int want = (int)ceil(log(100.0 / 10000.0) / log(r));
      printf("  [info] DC step 10000: output < 1%% after %d samples (closed form r^n: %d)\n", n99, want);
      CHECK(abs(n99 - want) <= 2, "blocker behaviour: settling to 1%% takes %d samples, closed form says %d", n99, want);
    }

    /* ---- imbalance correction against the explicit impairment model ---- */
    { int bad = 0;
      for (int t = 0; t < 5000; t++) {
          double g = 0.7 + 0.8 * (lcg_next() & 0xFFFF) / 65536.0, phi = (((int)(lcg_next() & 0xFFFF)) - 32768) / 32768.0 * 0.3;   /* up to +-17 deg */
          double Iv = ((int)(lcg_next() & 0xFFFF) - 32768) / 8192.0, Qv = ((int)(lcg_next() & 0xFFFF) - 32768) / 8192.0;
          float i = (float)Iv, q = (float)(g * (Qv * cos(phi) + Iv * sin(phi)));           /* impaired */
          iq_imbalance_corrector_t c = { (float)(g * cos(phi)), (float)(g * sin(phi)) };
          iq_imbalance_correct(&c, &i, &q);
          if (fabs(i - Iv) > 1e-5 || fabs(q - Qv) > 1e-4) bad++;
      }
      CHECK(bad == 0, "alpha = g cos(phi), beta = g sin(phi) undoes the impairment exactly: %d failures of 5000", bad); }
    { iq_imbalance_corrector_t c = { 1.0f, 0.0f }; float i = 3.5f, q = -2.25f; iq_imbalance_correct(&c, &i, &q); CHECK(i == 3.5f && q == -2.25f, "alpha=1, beta=0 is the identity");
      c.alpha = 2.0f; i = 1.0f; q = 8.0f; iq_imbalance_correct(&c, &i, &q); CHECK(i == 1.0f && q == 4.0f, "alpha divides Q only; I untouched");
      c.alpha = 1.0f; c.beta = 0.5f; i = 4.0f; q = 3.0f; iq_imbalance_correct(&c, &i, &q); CHECK(i == 4.0f && q == 1.0f, "beta subtracts beta*I from Q"); }
    /* ellipse -> circle, and image rejection before / after (closed form from the impairment model) */
    { const double g = 1.10, phi = 5.0 * M_PI / 180.0; enum { P = 128 }; const double f = 9.0 / P, w = 2.0 * M_PI * f;
      iq_imbalance_corrector_t c = { (float)(g * cos(phi)), (float)(g * sin(phi)) };
      double complex pos_b = 0, neg_b = 0, pos_a = 0, neg_a = 0; double rmin_b = 1e9, rmax_b = 0, rmin_a = 1e9, rmax_a = 0;
      for (int n = 0; n < P; n++) {
          float i = (float)cos(w * n), q = (float)(g * sin(w * n + phi)); double rb = hypot(i, q);
          if (rb < rmin_b) { rmin_b = rb; }
          if (rb > rmax_b) { rmax_b = rb; }
          pos_b += (i + I * q) * cexp(-I * w * n); neg_b += (i + I * q) * cexp(I * w * n);
          iq_imbalance_correct(&c, &i, &q); double ra = hypot(i, q);
          if (ra < rmin_a) { rmin_a = ra; }
          if (ra > rmax_a) { rmax_a = ra; }
          pos_a += (i + I * q) * cexp(-I * w * n); neg_a += (i + I * q) * cexp(I * w * n);
      }
      double irr_b = 20 * log10(cabs(pos_b) / cabs(neg_b)), irr_a = 20 * log10(cabs(pos_a) / fmax(cabs(neg_a), 1e-12));
      double pred_b = 20 * log10(cabs(1.0 + g * cexp(I * phi)) / cabs(1.0 - g * cexp(-I * phi)));
      printf("  [info] g=1.10, phi=5 deg: image rejection %.1f dB before (closed form %.1f dB), %.1f dB after; |z| range %.3f..%.3f -> %.6f..%.6f\n", irr_b, pred_b, irr_a, rmin_b, rmax_b, rmin_a, rmax_a);
      CHECK_NEAR(irr_b, pred_b, 0.05, "uncorrected image rejection matches the closed form |1+g e^{j phi}| / |1-g e^{-j phi}|");
      CHECK(rmax_b / rmin_b > 1.09, "uncorrected constellation is an ellipse (axis ratio %.3f)", rmax_b / rmin_b);
      CHECK(rmax_a / rmin_a < 1.00001, "corrected locus is a circle (radius spread %.2e)", rmax_a / rmin_a - 1.0);
      CHECK(irr_a > irr_b + 40.0 && irr_a > 70.0, "correction improves image rejection by > 40 dB (%.1f -> %.1f)", irr_b, irr_a); }
    /* alpha is g*cos(phi), not g: with alpha = g alone the corrected Q is low by the factor cos(phi) (1.5% at 10 deg) */
    { const double g = 1.10, phi = 10.0 * M_PI / 180.0; iq_imbalance_corrector_t c = { (float)g, (float)(g * sin(phi)) };
      float i = 0.0f, q = (float)(g * cos(phi));   /* the point I=0, Q=1 after impairment */
      iq_imbalance_correct(&c, &i, &q);
      CHECK_NEAR(q, cos(phi), 1e-5, "alpha = g leaves Q at cos(phi) instead of 1");
      c.alpha = (float)(g * cos(phi)); i = 0.0f; q = (float)(g * cos(phi)); iq_imbalance_correct(&c, &i, &q);
      CHECK_NEAR(q, 1.0, 1e-5, "alpha = g cos(phi) restores Q = 1"); }
    TEST_DONE("test_B_iq_correct");
}
