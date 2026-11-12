/* HB[47], HB[48]: NCO tables (nco_table_init), nco_t, quadrature_mix, mixer_to_q15, nco_phase_inc_for_freq (include/hb_nco.h, src/hb_nco.c).
 * References: double-precision cos/sin; the closed form of mixing a real tone; the table rule shared by both listings
 *   cos_table[i] = round(32768 * cos(2 pi i/256)), clamped to the Q15 maximum 32767
 *   phase_inc = fc/fs * 2^32. */
#include <stdint.h>
#include "check.h"
#include "hb_nco.h"

static int q15_ref_32768(double v) { double s = floor(v * 32768.0 + 0.5); if (s > 32767) s = 32767; if (s < -32768) s = -32768; return (int)s; }

int main(void) {
    nco_table_init();
    /* ---- tables against double math: every entry is the correctly rounded round(32768*x), clamped to 32767 ---- */
    int mism = 0;
    for (int i = 0; i < NCO_TABLE_SIZE; i++) {
        double a = 2.0 * M_PI * i / NCO_TABLE_SIZE;
        double ec = cos(a) * 32768.0, es = sin(a) * 32768.0;
        if (ec > 32767.0) { ec = 32767.0; }
        if (es > 32767.0) { es = 32767.0; }
        CHECK(fabs(cos_table[i] - ec) <= 0.5 + 1e-6, "cos_table[%d] = %d, exact %.4f", i, cos_table[i], ec);
        CHECK(fabs(sin_table[i] - es) <= 0.5 + 1e-6, "sin_table[%d] = %d, exact %.4f", i, sin_table[i], es);
        if (cos_table[i] != q15_ref_32768(cos(a)) || sin_table[i] != q15_ref_32768(sin(a))) mism++;
    }
    CHECK(mism == 0, "%d table entries differ from the correctly rounded double value", mism);
    CHECK(cos_table[183] == -7180, "cos_table[183]: exact -7179.509 rounds to -7180, got %d", cos_table[183]);
    /* landmarks */
    CHECK(cos_table[0] == 32767 && sin_table[0] == 0, "index 0: (32767, 0)  [1.0 clamped from 32768]");
    CHECK(sin_table[64] == 32767 && cos_table[64] == 0, "index 64 (90 deg)");
    CHECK(cos_table[128] == -32768 && sin_table[128] == 0, "index 128 (180 deg): cos = -32768");
    CHECK(sin_table[192] == -32768 && cos_table[192] == 0, "index 192 (270 deg): sin = -32768");
    CHECK(cos_table[32] == 23170 && sin_table[32] == 23170, "index 32 (45 deg) = 23170, the same 0.7071 constant as the QPSK table");
    /* unit circle: |(c,s)| ~ 32768 */
    for (int i = 0; i < NCO_TABLE_SIZE; i++) CHECK_NEAR(hypot(cos_table[i], sin_table[i]), 32768.0, 1.5, "table radius");

    /* ---- nco_phase_inc_for_freq (HB[48]) ---- */
    CHECK(nco_phase_inc_for_freq(1000.0f, 8192.0f) == 524288000u, "1000/8192 * 2^32 = 524288000 (exact in float)");
    CHECK(nco_phase_inc_for_freq(2048.0f, 8192.0f) == 1073741824u, "fs/4 -> 2^30");
    CHECK(nco_phase_inc_for_freq(4096.0f, 8192.0f) == 2147483648u, "fs/2 -> 2^31");
    CHECK(nco_phase_inc_for_freq(0.0f, 8192.0f) == 0u, "0 Hz -> 0");
    {   /* general case against double: float evaluation of fc/fs costs up to 2^-24 relative = 256 counts at full scale */
        static const float fcs[] = { 1.0f, 440.0f, 1000.0f, 10700.0f, 455000.0f, 12345.678f };
        static const float fss[] = { 8000.0f, 48000.0f, 44100.0f, 1.0e6f, 2.4e6f, 96000.0f };
        for (unsigned a = 0; a < sizeof fcs / sizeof fcs[0]; a++) for (unsigned b = 0; b < sizeof fss / sizeof fss[0]; b++) {
            double want = (double)fcs[a] / (double)fss[b] * 4294967296.0;
            if (want >= 4294967296.0) continue;
            double got = (double)nco_phase_inc_for_freq(fcs[a], fss[b]);
            CHECK_NEAR(got, want, 1.0 + want * 6.2e-8, "phase_inc");   /* 1 count truncation + one float ulp (2^-24) of the quotient */
        }
    }
    /* frequency actually produced: after fs samples the phase must have advanced by fc full turns (mod 2^32), to within the float error */
    { uint32_t inc = nco_phase_inc_for_freq(1000.0f, 48000.0f); double hz = (double)inc / 4294967296.0 * 48000.0;
      CHECK_NEAR(hz, 1000.0, 1e-3, "synthesised frequency (Hz) for 1 kHz @ 48 kHz"); }

    /* ---- quadrature_mix: index = top 8 bits, product is Q30, phase advances and wraps ---- */
    { nco_t n = { 0 }; n.phase_inc = n.base_inc = 0x01000000u;   /* exactly one table step per sample */
      int32_t iv, qv;
      for (int k = 0; k < 300; k++) {
          int idx = k % 256;
          uint32_t before = n.phase_acc;
          quadrature_mix(&n, 1000, &iv, &qv);
          CHECK(iv == 1000 * cos_table[idx] && qv == -1000 * sin_table[idx], "mix step %d uses table[%d]", k, idx);
          CHECK(n.phase_acc == before + 0x01000000u, "phase advances by phase_inc, step %d", k);
      }
      CHECK(n.phase_acc == (uint32_t)(300u * 0x01000000u), "phase accumulator wraps modulo 2^32 (300 steps)"); }
    /* Q30 range: full-scale sample times table landmarks */
    { nco_t n = { 0 }; int32_t iv, qv;
      quadrature_mix(&n, -32768, &iv, &qv);   /* idx 0: cos=32767, sin=0 */
      CHECK(iv == -32768 * 32767 && qv == 0, "full-scale negative sample, idx 0"); }

    /* ---- mixer_to_q15: rounding + saturation ---- */
    CHECK(mixer_to_q15(0) == 0, "0");
    CHECK(mixer_to_q15(1 << 14) == 1, "half rounds up (+0.5 -> 1)");
    CHECK(mixer_to_q15((1 << 14) - 1) == 0, "just below half rounds down");
    CHECK(mixer_to_q15(-(1 << 14)) == 0, "-0.5 -> 0 (add-half-then-shift rounds toward +inf)");
    CHECK(mixer_to_q15(-(1 << 14) - 1) == -1, "just below -0.5 -> -1");
    CHECK(mixer_to_q15(32767 * 32767) == 32766, "32767 * 32767 = 1073676289 -> 32766 (1073676289/32768 = 32766.0 + 0.5-ish)");
    CHECK(mixer_to_q15(1 << 30) == 32767, "+2^30 (= -32768 * -32768) saturates to 32767, not 32768 wrapped to -32768");
    /* Domain note: a Q30 product of two int16 values is <= 2^30 in magnitude, so +2^14 cannot overflow.  INT32_MAX is OUT of domain (the add wraps). */
    CHECK(mixer_to_q15(-(1 << 30)) == -32768, "-2^30 -> -32768");
    for (int32_t v = -(1 << 30); v < (1 << 30); v += 99991) {   /* against a double-precision round-and-clamp */
        double r = floor((double)v / 32768.0 + 0.5); if (r > 32767) r = 32767; if (r < -32768) r = -32768;
        CHECK(mixer_to_q15(v) == (int16_t)r, "mixer_to_q15(%d)", v);
    }
    /* "a bare cast keeps only the low 16 bits": demonstrate the prose claim with the Q30 value of a half-scale sample * unity */
    { int32_t q30 = 20000 * 32767; int16_t bare = (int16_t)q30;
      CHECK(bare != 20000 - 1 && bare != 20000, "bare cast of Q30 (%d) gives %d, noise, not the sample", q30, bare);
      CHECK(mixer_to_q15(q30) == 19999 || mixer_to_q15(q30) == 20000, "mixer_to_q15 of the same product is the sample (%d)", mixer_to_q15(q30)); }

    /* ---- mixing a real tone at the NCO frequency down to DC ----
     * x = A cos(theta + phi)  =>  I + jQ = (A/2) e^{j phi} + (term at 2*fc, removed by averaging whole periods).
     * fc = fs/8 puts the NCO exactly on table points (index step 32), so the only error left is Q15 rounding. */
    { const double A = 20000.0; const int N = 8 * 64;   /* 64 whole periods */
      for (int deg = -150; deg <= 150; deg += 30) {
          double phi = deg * M_PI / 180.0;
          nco_t n = { 0 }; n.phase_inc = n.base_inc = nco_phase_inc_for_freq(1000.0f, 8000.0f);
          double si = 0, sq = 0;
          for (int k = 0; k < N; k++) {
              int16_t x = (int16_t)lrint(A * cos(2.0 * M_PI * k / 8.0 + phi));
              int32_t iv, qv; quadrature_mix(&n, x, &iv, &qv);
              si += mixer_to_q15(iv); sq += mixer_to_q15(qv);
          }
          CHECK_NEAR(si / N, A / 2.0 * cos(phi), 3.0, "mixed I mean = A/2 cos(phi)");
          CHECK_NEAR(sq / N, A / 2.0 * sin(phi), 3.0, "mixed Q mean = A/2 sin(phi) (sign convention: Q = -x sin)");
      } }
    /* general (off-grid) frequency: the table index truncates the phase, so the phase seen by the mixer lags by err in [0, 2pi/256).
     * That bounds the baseband error:  |Q mean| <= A/2 sin(2pi/256) = 245  and  I mean >= A/2 cos(2pi/256) = 9994. */
    { const double A = 20000.0; const int N = 8192;
      nco_t n = { 0 }; n.phase_inc = n.base_inc = nco_phase_inc_for_freq(1000.0f, 8192.0f);
      double si = 0, sq = 0;
      for (int k = 0; k < N; k++) {
          int16_t x = (int16_t)lrint(A * cos(2.0 * M_PI * 1000.0 * k / 8192.0));
          int32_t iv, qv; quadrature_mix(&n, x, &iv, &qv); si += mixer_to_q15(iv); sq += mixer_to_q15(qv);
      }
      double bound_q = A / 2.0 * sin(2.0 * M_PI / 256.0) + 3.0, bound_i = A / 2.0 * cos(2.0 * M_PI / 256.0) - 3.0;
      printf("  [info] off-grid mix (1 kHz @ 8192 Hz): I mean %.1f (ideal 10000), Q mean %.1f (ideal 0; truncating the phase index biases it)\n", si / N, sq / N);
      CHECK(si / N >= bound_i && si / N <= A / 2.0 + 3.0, "off-grid I mean %g within [%g, %g]", si / N, bound_i, A / 2.0 + 3.0);
      CHECK(fabs(sq / N) <= bound_q, "off-grid |Q mean| %g <= %g", fabs(sq / N), bound_q);
      /* and exactly which bias the truncation produces: the index is floor(256*frac) with frac stepping 125/1024 per sample
       * -> error fractions {0, .25, .5, .75}/256 of a turn, mean 0.375 table steps => Q mean = A/2 * mean(sin(err)). */
      double mq = 0; for (int f4 = 0; f4 < 4; f4++) mq += sin(2.0 * M_PI / 256.0 * (f4 * 0.25)) / 4.0;
      CHECK_NEAR(sq / N, A / 2.0 * mq, 6.0, "off-grid Q mean matches the table-phase-truncation bias A/2*mean(sin(err))"); }
    TEST_DONE("test_B_nco");
}
