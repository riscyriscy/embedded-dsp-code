/* HB[55], HB[56]: Gray QPSK table, qpsk_modulate_symbol(), quadrature_upconvert(), qpsk_demodulate_symbol()
 * (src/hb_qpsk.c, include/hb_qpsk.h; NCO tables from hb_nco.h).  References: atan2 geometry (Gray ring), closed-form energy, the
 * closed-form BER of Gray QPSK in AWGN, and double-precision upconversion Re{(I+jQ) e^{j theta}}. */
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "hb_qpsk.h"

static int popcount2(int v) { return (v & 1) + ((v >> 1) & 1); }
static double gauss(void) {   /* Box-Muller from the LCG */
    double u1 = (lcg_next() + 1.0) / 16777217.0, u2 = (lcg_next() + 1.0) / 16777217.0;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}
static double qfunc(double x) { return 0.5 * erfc(x / sqrt(2.0)); }

int main(void) {
    nco_table_init();
    /* ---- mapping exactly as the comments in the listing say ---- */
    static const int16_t wi[4] = { +1, +1, -1, -1 }, wq[4] = { +1, -1, +1, -1 };   /* 00:++  01:+-  10:-+  11:-- */
    for (uint8_t d = 0; d < 4; d++) {
        iq_sample_t s = qpsk_modulate_symbol(d);
        CHECK(s.i == wi[d] * 23170 && s.q == wq[d] * 23170, "dibit %u -> (%d,%d)", d, s.i, s.q);
    }
    CHECK(qpsk_modulate_symbol(7).i == qpsk_modulate_symbol(3).i && qpsk_modulate_symbol(7).q == qpsk_modulate_symbol(3).q && qpsk_modulate_symbol(0xFC).i == qpsk_modulate_symbol(0).i, "dibit & 3 masking");
    /* ---- constellation energy: 23170 = round(32768/sqrt 2); |point|^2 ~ 1.0 in Q15 ---- */
    CHECK((int)lrint(32768.0 / sqrt(2.0)) == 23170, "23170 = round(32768/sqrt(2)) = %.3f", 32768.0 / sqrt(2.0));
    for (uint8_t d = 0; d < 4; d++) {
        iq_sample_t s = qpsk_modulate_symbol(d); double e = ((double)s.i * s.i + (double)s.q * s.q) / (32768.0 * 32768.0);
        CHECK_NEAR(e, 1.0, 1e-4, "constellation point energy = 1.0 (Q15)");
        CHECK_NEAR(hypot(s.i, s.q), 32768.0, 1.0, "|point| ~ 32768 (the unit circle in Q15)");
    }
    { double e = 0; for (uint8_t d = 0; d < 4; d++) { iq_sample_t s = qpsk_modulate_symbol(d); e += ((double)s.i * s.i + (double)s.q * s.q) / 4.0; }
      CHECK_NEAR(e / (32768.0 * 32768.0), 1.0, 1e-4, "average symbol energy Es = 1");
      printf("  [info] energy per point = %.6f of full-scale^2 (|23170|^2 * 2 / 2^30), i.e. %.5f dB\n", e / (32768.0 * 32768.0), 10 * log10(e / (32768.0 * 32768.0))); }
    /* ---- Gray property by geometry: order the points by angle; neighbours must differ in exactly one bit, including the wrap-around ---- */
    { int order[4], used[4] = { 0 }; double ang[4];
      for (int d = 0; d < 4; d++) { iq_sample_t s = qpsk_modulate_symbol((uint8_t)d); ang[d] = atan2((double)s.q, (double)s.i); }
      for (int k = 0; k < 4; k++) { int best = -1; for (int d = 0; d < 4; d++) if (!used[d] && (best < 0 || ang[d] < ang[best])) best = d; used[best] = 1; order[k] = best; }
      for (int k = 0; k < 4; k++) CHECK(popcount2(order[k] ^ order[(k + 1) % 4]) == 1, "angularly adjacent points %d and %d differ in %d bits", order[k], order[(k + 1) % 4], popcount2(order[k] ^ order[(k + 1) % 4]));
      /* prose: counter-clockwise ring 00 -> 10 -> 11 -> 01 -> 00: check by angle (CCW = increasing angle from quadrant I) */
      CHECK(ang[0] > 0 && ang[0] < M_PI / 2 && ang[2] > M_PI / 2 && ang[2] < M_PI && ang[3] < -M_PI / 2 && ang[1] < 0 && ang[1] > -M_PI / 2, "00 in quadrant I, 10 in II, 11 in III, 01 in IV");
      /* a single-axis error (nearest neighbour) costs exactly one bit; a diagonal error costs two */
      for (int d = 0; d < 4; d++) { iq_sample_t s = qpsk_modulate_symbol((uint8_t)d);
          int e1 = qpsk_demodulate_symbol(-s.i, s.q), e2 = qpsk_demodulate_symbol(s.i, -s.q), e3 = qpsk_demodulate_symbol(-s.i, -s.q);
          CHECK(popcount2(e1 ^ d) == 1 && popcount2(e2 ^ d) == 1 && popcount2(e3 ^ d) == 2, "dibit %d: single-axis flips cost 1 bit, diagonal 2", d); } }

    /* ---- demodulator ---- */
    for (uint8_t d = 0; d < 4; d++) { iq_sample_t s = qpsk_modulate_symbol(d);
        CHECK(qpsk_demodulate_symbol(s.i, s.q) == d, "round trip %u", d);
        CHECK(qpsk_demodulate_symbol(s.i / 100, s.q / 100) == d, "attenuated 100x still decodes %u (sign-only decision)", d);
        CHECK(qpsk_demodulate_symbol(s.i * 50000, s.q * 50000) == d, "huge amplitudes (32-bit) decode %u", d); }
    CHECK(qpsk_demodulate_symbol(0, 0) == 0, "i=q=0 decides as positive (00)");
    CHECK(qpsk_demodulate_symbol(-1, 1) == 2 && qpsk_demodulate_symbol(1, -1) == 1 && qpsk_demodulate_symbol(INT32_MIN, INT32_MIN) == 3 && qpsk_demodulate_symbol(INT32_MAX, INT32_MAX) == 0, "sign boundaries / extremes");
    { int ok = 1; for (int t = 0; t < 10000; t++) { int32_t i = (int32_t)lcg_next() - (1 << 23), q = (int32_t)lcg_next() - (1 << 23); uint8_t r = qpsk_demodulate_symbol(i, q); if (r > 3 || r != (uint8_t)(((i < 0) << 1) | (q < 0))) ok = 0; }
      CHECK(ok, "always returns 0..3 and equals the sign bits"); }
    /* ---- noisy channel: sign decisions are the ML decision for Gray QPSK, so BER = Q(A/sigma) ---- */
    { const double A = 23170.0, sigma = A / 2.0; long nerr = 0, nbit = 0; const int NS = 200000;
      for (int t = 0; t < NS; t++) { uint8_t d = (uint8_t)(lcg_next() & 3); iq_sample_t s = qpsk_modulate_symbol(d);
          int32_t i = (int32_t)lrint(s.i + sigma * gauss()), q = (int32_t)lrint(s.q + sigma * gauss());
          uint8_t r = qpsk_demodulate_symbol(i, q); nerr += popcount2(r ^ d); nbit += 2; }
      double ber = (double)nerr / nbit, want = qfunc(A / sigma), se = sqrt(want * (1 - want) / nbit);
      printf("  [info] QPSK BER at A/sigma = 2: measured %.5f, Q(2) = %.5f (+-%.5f one sigma)\n", ber, want, se);
      CHECK(fabs(ber - want) < 4.0 * se, "BER %.5f within 4 sigma of Q(A/sigma) = %.5f", ber, want); }

    /* ---- quadrature_upconvert: sign convention and agreement with Re{(I+jQ) e^{j theta}} ---- */
    for (uint8_t d = 0; d < 4; d++) {
        iq_sample_t s = qpsk_modulate_symbol(d); nco_t n = { 0 }; n.phase_inc = n.base_inc = 0x01000000u * 5;   /* 5 table steps per sample */
        double worst = 0;
        for (int k = 0; k < 256; k++) {
            int idx = (k * 5) % 256; double th = 2.0 * M_PI * idx / 256.0; int16_t rf; quadrature_upconvert(&n, s.i, s.q, &rf);
            double ref = s.i * cos(th) - s.q * sin(th);
            if (fabs(rf - ref) > worst) worst = fabs(rf - ref);
        }
        CHECK(worst <= 3.0, "dibit %d: upconverted samples within 3 LSB of I cos - Q sin (worst %g)", d, worst);
        CHECK(n.phase_acc == (uint32_t)(256u * 0x01000000u * 5u), "phase accumulator advanced 256 times");
    }
    /* loopback: upconvert a held symbol, mix it back down with quadrature_mix, average whole periods: I/2 and Q/2 (the 2*fc term averages out) */
    for (uint8_t d = 0; d < 4; d++) {
        iq_sample_t s = qpsk_modulate_symbol(d); nco_t tx = { 0 }, rx = { 0 };
        tx.phase_inc = tx.base_inc = rx.phase_inc = rx.base_inc = nco_phase_inc_for_freq(1000.0f, 8000.0f);   /* fs/8: exactly on table points */
        double si = 0, sq = 0; const int N = 8 * 32;
        for (int k = 0; k < N; k++) { int16_t rf; quadrature_upconvert(&tx, s.i, s.q, &rf);
            int32_t iv, qv; quadrature_mix(&rx, rf, &iv, &qv); si += mixer_to_q15(iv); sq += mixer_to_q15(qv); }
        CHECK_NEAR(si / N, s.i / 2.0, 3.0, "loopback I mean = I/2 (sign: upconvert is the inverse of the mixer)");
        CHECK_NEAR(sq / N, s.q / 2.0, 3.0, "loopback Q mean = Q/2");
        CHECK(qpsk_demodulate_symbol((int32_t)si, (int32_t)sq) == d, "loopback decodes dibit %u", d);
    }
    /* ---- quadrature_upconvert saturates instead of wrapping (pulse-shaped I/Q can leave the unit circle) ---- */
    { nco_t n = { 0 }; n.phase_acc = 96u << 24; n.phase_inc = 0;       /* theta = 135 deg: cos = -23170, sin = +23170 */
      int16_t rf = 0; quadrature_upconvert(&n, 32767, 32767, &rf);
      int64_t exact = ((int64_t)32767 * cos_table[96] - (int64_t)32767 * sin_table[96]) >> 15;   /* about -46339 */
      CHECK(exact < -32768, "the exact value %lld is outside int16 range", (long long)exact);
      CHECK(rf == -32768, "I=Q=32767 at 135 deg saturates to -32768 (exact %lld), got %d", (long long)exact, rf);
      n.phase_acc = 224u << 24; quadrature_upconvert(&n, 32767, 32767, &rf);   /* theta = 315 deg: cos = +23170, sin = -23170 */
      CHECK(rf == 32767, "I=Q=32767 at 315 deg saturates to +32767, got %d", rf);
      /* over random i, q (range chosen so that some results overflow): output == clamp(exact) */
      int ok = 1, overflowed = 0;
      for (int t = 0; t < 20000; t++) {
          nco_t a = { 0, 0, 0 }; a.phase_acc = (uint32_t)lcg_next() << 8;
          int32_t i = (int32_t)(lcg_next() % 80001) - 40000, q = (int32_t)(lcg_next() % 80001) - 40000;
          int idx = a.phase_acc >> 24; int64_t exact = ((int64_t)i * cos_table[idx] - (int64_t)q * sin_table[idx]) >> 15;
          int64_t clamped = exact > 32767 ? 32767 : exact < -32768 ? -32768 : exact; if (clamped != exact) overflowed++;
          int16_t ra; quadrature_upconvert(&a, i, q, &ra);
          if (ra != clamped) ok = 0;
      }
      CHECK(ok && overflowed > 1000, "output == clamp(exact) for random i, q (%d of 20000 cases exceed int16)", overflowed); }
    TEST_DONE("test_B_qpsk");
}
