/* HB[54]: quadrature FM discriminator fm_demod_process() (src/hb_fm_demod.c).
 * Reference: a complex tone z[n] = A e^{j phi[n]} gives cross = I[n-1] Q[n] - Q[n-1] I[n] = A^2 sin(phi[n]-phi[n-1])
 * EXACTLY (trig identity), and the code returns cross >> 16.  The prose claim "approximates sin(delta_phase), good for
 * small delta_phase" is checked by comparing against sin(dphi) (exact) and against dphi (the small-angle approximation). */
#include <stdint.h>
#include "check.h"
#include "hb_fm_demod.h"

#define A 16384.0                 /* A^2 = 2^28  =>  output = 4096 * sin(dphi) */
#define GAIN (A * A / 65536.0)    /* 4096 */

int main(void) {
    /* ---- constant frequency: output = GAIN * sin(dphi), sign = sign of the frequency ---- */
    static const double dphis[] = { 0.0, 0.01, 0.05, 0.1, 0.3, 0.5, 1.0, M_PI / 2, 2.0, 3.0, -0.01, -0.1, -0.5, -1.0, -M_PI / 2, -3.0 };
    for (unsigned t = 0; t < sizeof dphis / sizeof dphis[0]; t++) {
        double d = dphis[t]; fm_demod_t st = { 0, 0 }; int32_t out = 0;
        for (int n = 0; n < 8; n++) {
            double ph = 0.7 + d * n;
            out = fm_demod_process(&st, (int32_t)lrint(A * cos(ph)), (int32_t)lrint(A * sin(ph)));
        }
        /* integer I/Q rounding (+-0.5 each) perturbs the cross product by <= ~2*A*0.5*... = 2*16384 -> 0.5 LSB after >>16; +1 for the floor */
        CHECK_NEAR(out, GAIN * sin(d), 2.0, "fm_demod output vs A^2 sin(dphi)/2^16");
        if (d > 0.0) CHECK(out > 0, "positive frequency (counter-clockwise rotation) must give a positive output, dphi=%g out=%d", d, out);
        if (d < 0.0) CHECK(out < 0, "negative frequency must give a negative output, dphi=%g out=%d", d, out);
    }
    /* first call has no history: i_prev = q_prev = 0 -> output 0 */
    { fm_demod_t st = { 0, 0 }; CHECK(fm_demod_process(&st, 12345, -6789) == 0, "first sample: no history, output 0");
      CHECK(st.i_prev == 12345 && st.q_prev == -6789, "state holds the previous sample"); }
    /* amplitude dependence: output scales with A^2 (the discriminator is NOT amplitude-invariant) */
    { fm_demod_t s1 = { 0, 0 }, s2 = { 0, 0 }; int32_t o1 = 0, o2 = 0; double d = 0.2;
      for (int n = 0; n < 4; n++) {
          o1 = fm_demod_process(&s1, (int32_t)lrint(8192.0 * cos(d * n)), (int32_t)lrint(8192.0 * sin(d * n)));
          o2 = fm_demod_process(&s2, (int32_t)lrint(16384.0 * cos(d * n)), (int32_t)lrint(16384.0 * sin(d * n)));
      }
      CHECK_NEAR((double)o2 / o1, 4.0, 0.02, "doubling A quadruples the output"); }

    /* ---- the small-angle claim, quantified ----
     * output/GAIN = sin(dphi);  a receiver that treats it as proportional to dphi is off by (1 - sin(x)/x) ~ x^2/6. */
    printf("  [info] linearity error of sin(dphi) vs dphi:\n");
    static const double ds[] = { 0.02, 0.05, 0.1, 0.2, 0.3, 0.5, 0.8, 1.0, 1.4 };
    for (unsigned t = 0; t < sizeof ds / sizeof ds[0]; t++) {
        double d = ds[t]; fm_demod_t st = { 0, 0 }; int32_t out = 0;
        for (int n = 0; n < 3; n++) out = fm_demod_process(&st, (int32_t)lrint(A * cos(d * n)), (int32_t)lrint(A * sin(d * n)));
        double lin_err = 1.0 - (out / GAIN) / d;                 /* measured departure from proportionality */
        double pred = 1.0 - sin(d) / d;                          /* closed form */
        printf("    dphi=%.2f rad  measured %.4f  predicted %.4f\n", d, lin_err, pred);
        CHECK_NEAR(lin_err, pred, 3.0 / (GAIN * d) + 1e-6, "linearisation error follows 1 - sin(x)/x");
        /* (the closed-form error is what the prose's claim is about; the MEASURED figure also carries the output's own 1-LSB floor
         * quantisation, 1/(GAIN*dphi) = 1.2%% at 0.02 rad, which is why the check above uses that tolerance) */
        if (d <= 0.1) CHECK(pred < 0.002, "small angle (<= 0.1 rad, i.e. >= 63 samples per cycle): < 0.2%% error");
        if (d >= 0.5) CHECK(lin_err > 0.04, "0.5 rad per sample: >4%% non-linearity (small-angle trick no longer valid)");
    }
    /* beyond +-pi/2 the discriminator turns over (output falls although frequency rises) and beyond pi it flips sign: aliasing of dphi */
    { double d1 = 1.2, d2 = 2.4, d3 = 3.6; int32_t o[3]; double dd[3] = { d1, d2, d3 };
      for (int t = 0; t < 3; t++) { fm_demod_t st = { 0, 0 }; for (int n = 0; n < 3; n++) o[t] = fm_demod_process(&st, (int32_t)lrint(A * cos(dd[t] * n)), (int32_t)lrint(A * sin(dd[t] * n))); }
      CHECK(o[1] < o[0], "dphi 2.4 gives LESS output than 1.2 (past pi/2 the S-curve turns over)");
      CHECK(o[2] < 0, "dphi 3.6 rad > pi reads as a NEGATIVE frequency (3.6 - 2pi = -2.68)"); }

    /* ---- an actual FM signal: phi[n] = phi[n-1] + 2 pi dev/fs m[n], m = 0.8 sin(2 pi fm n/fs) ---- */
    { const double fs = 48000.0, dev = 3000.0, fm = 500.0; fm_demod_t st = { 0, 0 };
      double ph = 0; double sxy = 0, sxx = 0; int maxn = 4800;
      static double m_[4800], y_[4800];
      for (int n = 0; n < maxn; n++) {
          double m = 0.8 * sin(2.0 * M_PI * fm * n / fs);
          ph += 2.0 * M_PI * dev / fs * m;
          int32_t o = fm_demod_process(&st, (int32_t)lrint(A * cos(ph)), (int32_t)lrint(A * sin(ph)));
          m_[n] = m; y_[n] = o;
      }
      for (int n = 10; n < maxn; n++) { sxy += m_[n] * y_[n]; sxx += m_[n] * m_[n]; }
      double slope = sxy / sxx;
      double want = GAIN * 2.0 * M_PI * dev / fs;   /* output per unit message */
      printf("  [info] FM tone: demod slope %.2f per unit message, small-angle prediction %.2f (max dphi = %.3f rad)\n", slope, want, 2.0 * M_PI * dev / fs * 0.8);
      /* dphi_max = 0.314 rad: sin(x)/x = 0.984 => slope is ~1% low, never high */
      CHECK(slope < want && slope > want * 0.97, "slope %g within (0.97, 1.0) of the small-angle value %g", slope, want);
      double r2 = 0; for (int n = 10; n < maxn; n++) { double d = y_[n] - slope * m_[n]; r2 += d * d; }
      CHECK(sqrt(r2 / (maxn - 10)) < 0.02 * GAIN * 2.0 * M_PI * dev / fs * 0.8, "residual (non-linearity + rounding) < 2%% of the peak output");
      /* proportional to deviation: halve the deviation -> half the output */
      fm_demod_t st2 = { 0, 0 }; ph = 0; double sxy2 = 0;
      for (int n = 0; n < maxn; n++) {
          double m = 0.8 * sin(2.0 * M_PI * fm * n / fs); ph += 2.0 * M_PI * (dev / 2.0) / fs * m;
          int32_t o = fm_demod_process(&st2, (int32_t)lrint(A * cos(ph)), (int32_t)lrint(A * sin(ph)));
          if (n >= 10) sxy2 += m * o;
      }
      CHECK_NEAR((sxy2 / sxx) / slope, 0.5, 0.01, "output is proportional to the frequency deviation"); }

    /* ---- Q15 full scale: the int64 cross product and the >>16 never overflow the int32 return ---- */
    { fm_demod_t st = { 32767, 32767 };
      CHECK(fm_demod_process(&st, -32768, 32767) == (int32_t)(((int64_t)32767 * 32767 - (int64_t)32767 * -32768) >> 16), "full-scale corners");
      st.i_prev = -32768; st.q_prev = -32768;
      int32_t o = fm_demod_process(&st, 32767, -32768);
      CHECK(o == (int32_t)(((int64_t)-32768 * -32768 - (int64_t)-32768 * 32767) >> 16) && o > 0, "extreme Q15 inputs: result %d, no wrap", o); }
    TEST_DONE("test_B_fm_demod");
}
