/* HB[41], HB[42], HB[43], HB[44]: CIC integrator / comb / order-3 decimator, bit growth, growth-bits rescale, PDM front end
 * (src/hb_cic.c).  This file #includes src/hb_cic.c so it can call the file-static cic_growth_bits().
 * References: brute-force cascaded moving sums in int64 (a CIC of order N and ratio M IS N cascaded length-M boxcars, decimated),
 * closed-form |H(f)| = |sin(pi M f)/(M sin(pi f))|^N, and exact 64-bit arithmetic for the overflow cases. */
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include "check.h"
#include "../src/hb_cic.c"

/* brute force: y = boxcar_M ( boxcar_M ( boxcar_M (x) ) ), zero initial state, kept at n = M-1, 2M-1, ... */
static void brute_cic3(const int32_t *x, int n, int M, int64_t *y) {
    static int64_t a[20000], b[20000];
    for (int i = 0; i < n; i++) { int64_t s = 0; for (int k = 0; k < M && i - k >= 0; k++) s += x[i - k]; a[i] = s; }
    for (int i = 0; i < n; i++) { int64_t s = 0; for (int k = 0; k < M && i - k >= 0; k++) s += a[i - k]; b[i] = s; }
    for (int i = 0; i < n; i++) { int64_t s = 0; for (int k = 0; k < M && i - k >= 0; k++) s += b[i - k]; y[i] = s; }
}
static double H_cic(double f, int M, int N) { return pow(fabs(sin(M_PI * M * f) / (M * sin(M_PI * f))), N); }

int main(void) {
    /* ---- stage primitives ---- */
    { cic_integrator_t s = { 0 }; CHECK(cic_integrator_step(&s, 5) == 5 && cic_integrator_step(&s, -2) == 3 && cic_integrator_step(&s, 10) == 13 && s.accum == 13, "integrator accumulates");
      cic_comb_t c = { 0 }; CHECK(cic_comb_step(&c, 7) == 7 && cic_comb_step(&c, 10) == 3 && (int64_t)cic_comb_step(&c, 4) == -6 && c.delay_reg == 4, "comb differences successive inputs"); }

    /* ---- decimator vs brute-force cascaded moving sums, M = 1..16 ---- */
    for (int M = 1; M <= 16; M++) {
        enum { N = 600 }; static int32_t x[N]; static int64_t ref[N];
        for (int i = 0; i < N; i++) x[i] = (int32_t)(lcg_next() % 201) - 100;
        brute_cic3(x, N, M, ref);
        cic_decimator_t d = { 0 }; int nout = 0, ok = 1;
        for (int i = 0; i < N; i++) {
            int64_t y = 0; bool got = cic_decimator_process(&d, x[i], M, &y);
            bool want = ((i + 1) % M) == 0;
            if (got != want) ok = 0;
            if (got) { nout++; if (y != ref[i]) { ok = 0; printf("  M=%d i=%d got %lld want %lld\n", M, i, (long long)y, (long long)ref[i]); } }
        }
        CHECK(ok, "cic decimator M=%d vs brute-force moving sums", M);
        CHECK(nout == N / M, "M=%d: %d outputs, want %d", M, nout, N / M);
    }
    /* ---- the caller-owns-the-counter contract (prose): passing the SAME counter every call keeps all or none ---- */
    { cic_integrator_t it[3] = { {0}, {0}, {0} }; cic_comb_t cb[3] = { {0}, {0}, {0} }; int kept_mod0 = 0, kept_mod1 = 0; int64_t o;
      for (int i = 0; i < 50; i++) if (cic_decimate_stage(it, cb, 1, 8, 8, &o)) kept_mod0++;     /* counter pinned at a multiple of M */
      for (int i = 0; i < 50; i++) if (cic_decimate_stage(it, cb, 1, 9, 8, &o)) kept_mod1++;     /* pinned at a non-multiple */
      CHECK(kept_mod0 == 50 && kept_mod1 == 0, "constant decim_counter: every sample kept (%d) or none (%d), never 1-in-M", kept_mod0, kept_mod1); }
    /* the counter in the state struct stays within 0..M-1 however long the stream runs, so it cannot overflow */
    { cic_decimator_t d = { 0 }; int64_t o; int in_range = 1;
      for (int i = 0; i < 1000; i++) { cic_decimator_process(&d, 1, 7, &o); if (d.decim_counter < 0 || d.decim_counter >= 7) in_range = 0; }
      CHECK(in_range, "decim_counter stays in [0, M)");
      d.decim_counter = 6; CHECK(cic_decimator_process(&d, 1, 7, &o) && d.decim_counter == 0, "the Mth call keeps a sample and wraps the counter to 0"); }

    /* ---- passband DC gain = M^N (prose), after the N-output transient ---- */
    for (int M = 2; M <= 32; M *= 2) {
        cic_decimator_t d = { 0 }; int64_t y = 0; int64_t gain = (int64_t)M * M * M;
        for (int i = 0; i < M * 8; i++) { int64_t o; if (cic_decimator_process(&d, 3, M, &o)) y = o; }
        CHECK(y == 3 * gain, "DC gain: M=%d, input 3 -> %lld, want 3*M^3 = %lld", M, (long long)y, (long long)(3 * gain));
    }

    /* ---- bit growth: growth_bits = ceil(N log2 M) (HB[42]) and the shift helper (HB[43]) ---- */
    CHECK(cic_growth_bits(3, 64) == 18, "order 3, M=64: 18 bits, got %d", cic_growth_bits(3, 64));
    CHECK(3 * 6 == (int)ceil(3.0 * log2(64.0)), "HB[42] formula gives 18 for N=3, M=64");
    /* the helper takes N as a parameter: equals the formula exactly when M is a power of two */
    for (int N = 1; N <= 5; N++) for (int e = 0; e <= 10; e++) { int M = 1 << e; CHECK(cic_growth_bits(N, M) == (int)lround(ceil(N * log2((double)M) - 1e-9)), "N=%d M=2^%d: helper == ceil(N log2 M)", N, e); }
    /* for other M the helper is N*ceil(log2 M) >= ceil(N log2 M): the shift removes MORE than the gain (prose), by < 2^N */
    { int differ = 0; double worst_ratio = 0;
      for (int M = 2; M <= 1500; M++) {
          int gb = cic_growth_bits(3, M); double gain = (double)M * M * M, shift = ldexp(1.0, gb);
          CHECK(shift >= gain, "M=%d: 2^growth_bits (%g) must be >= M^3 (%g)", M, shift, gain);
          CHECK(shift / gain < 8.0, "M=%d: over-removal ratio %g must be < 2^N = 8", M, shift / gain);
          if (gb != (int)ceil(3.0 * log2((double)M) - 1e-9)) differ++;
          if (shift / gain > worst_ratio) worst_ratio = shift / gain;
      }
      printf("  [info] M=2..1500 (N=3): helper differs from the growth_bits formula for %d values of M; worst over-removal 2^gb/M^3 = %.3f (close to 2 per stage = 8)\n", differ, worst_ratio);
      CHECK(differ > 0 && cic_growth_bits(3, 10) == 12 && (int)ceil(3.0 * log2(10.0)) == 10, "helper N*ceil(log2 M) is larger than ceil(N*log2 M) for non-power-of-2 M (N=3, M=10: %d vs %d)", cic_growth_bits(3, 10), (int)ceil(3.0 * log2(10.0)));
      CHECK(worst_ratio > 7.5, "worst case approaches 2 per stage (7.98 at M=1025): got %.3f", worst_ratio); }
    CHECK(cic_growth_bits(3, 1) == 0 && cic_growth_bits(3, 2) == 3, "M=1 -> 0 bits, M=2 -> 3 bits");

    /* ---- the 34-bit claim: 16-bit input, N=3, M=64 ---- */
    { int64_t worst_pos = (int64_t)32767 * 64 * 64 * 64, worst_neg = (int64_t)-32768 * 64 * 64 * 64;
      /* signed bits needed for [worst_neg, worst_pos]: smallest B with -2^(B-1) <= worst_neg and worst_pos <= 2^(B-1)-1 */
      int B = 1; while (!(worst_neg >= -((int64_t)1 << (B - 1)) && worst_pos <= ((int64_t)1 << (B - 1)) - 1)) B++;
      CHECK(B == 34, "order-3, M=64, 16-bit input: needs %d signed bits (34)", B);
      CHECK(worst_pos > INT32_MAX, "and that exceeds int32 (%lld > %d)", (long long)worst_pos, INT32_MAX);
      CHECK(B - 16 == 18, "= 16 + 18 growth bits"); }

    /* ---- cic_decimate_stage_scaled: round + shift back to the input scale ---- */
    for (int M = 2; M <= 64; M *= 2) {
        cic_integrator_t it[3] = { {0}, {0}, {0} }; cic_comb_t cb[3] = { {0}, {0}, {0} }; int32_t y = -1, o;
        int cnt = 0; for (int i = 0; i < M * 8; i++) { cnt++; if (cic_decimate_stage_scaled(it, cb, 100, cnt, M, &o)) y = o; }
        CHECK(y == 100, "scaled stage, M=%d (power of 2: gain M^3 removed exactly): DC 100 -> %d (want 100)", M, y);
    }
    /* non-power-of-2 M: output = c*M^3/2^gb (rounded), i.e. attenuated by the helper's over-removal */
    { int M = 10; cic_integrator_t it[3] = { {0}, {0}, {0} }; cic_comb_t cb[3] = { {0}, {0}, {0} }; int32_t y = 0, o; int cnt = 0;
      for (int i = 0; i < M * 8; i++) { cnt++; if (cic_decimate_stage_scaled(it, cb, 1000, cnt, M, &o)) y = o; }
      double want = 1000.0 * 1000.0 / 4096.0;   /* gain 1000, shift 12 */
      CHECK_NEAR(y, want, 0.5, "M=10: DC 1000 -> 1000*M^3/2^12 = 244.1 (a 0.244 'gain', as the prose warns)");
      CHECK(y < 300, "result is 4x too small, as the prose says ('a real, fixed, predictable attenuation')"); }
    /* the decimator-state wrapper around the scaled stage */
    { cic_decimator_t d = { 0 }; int32_t y = 0, o; for (int i = 0; i < 64 * 8; i++) { if (cic_decimator_process_scaled(&d, 700, 64, &o)) y = o; }
      CHECK(y == 700, "decimator state + scaled stage: DC 700 -> %d", y); }

    /* ---- 16-bit data at M=64 (the 34-bit case): the scaled stage returns the input level for every DC level, including full scale ---- */
    { static const int32_t levels[] = { 8191, 8192, 20000, 32767, -8192, -8193, -20000, -32768, 1, -1, 0 };
      for (unsigned j = 0; j < sizeof levels / sizeof levels[0]; j++) {
          cic_integrator_t it[3] = { {0}, {0}, {0} }; cic_comb_t cb[3] = { {0}, {0}, {0} }; int32_t o, y = 12345; int cnt = 0;
          for (int i = 0; i < 64 * 8; i++) { cnt++; if (cic_decimate_stage_scaled(it, cb, levels[j], cnt, 64, &o)) y = o; }
          CHECK(y == levels[j], "scaled stage, M=64: DC %d -> %d", levels[j], y);
      }
      /* raw output of the unscaled path: exactly c * M^3, which needs 34 bits */
      cic_decimator_t d = { 0 }; int64_t raw = 0, o;
      for (int i = 0; i < 64 * 8; i++) if (cic_decimator_process(&d, 32767, 64, &o)) raw = o;
      CHECK(raw == (int64_t)32767 * 262144, "unscaled output for DC 32767 is 32767 * 2^18 = %lld, got %lld", (long long)((int64_t)32767 * 262144), (long long)raw);
      /* the wrapper keeps the same contract as the stage */
      cic_decimator_t w = { 0 }; int32_t ys = 0, os; int nk = 0;
      for (int i = 0; i < 64 * 8; i++) if (cic_decimator_process_scaled(&w, 20000, 64, &os)) { ys = os; nk++; }
      CHECK(ys == 20000 && nk == 8, "cic_decimator_process_scaled: DC 20000 -> %d, %d outputs", ys, nk);
      /* against brute force with full-scale random data, raw and scaled (round, then shift by 3*log2(M)) */
      enum { N = 1024 }; static int32_t x[N]; static int64_t ref[N];
      for (int i = 0; i < N; i++) x[i] = (int32_t)((int)(lcg_next() & 0xFFFF) - 32768);
      for (int M = 4; M <= 64; M *= 4) {
          brute_cic3(x, N, M, ref);
          cic_decimator_t h = { 0 }, hs = { 0 }; int ok = 1, oks = 1; int bits = 0; while ((1 << bits) < M) bits++;
          for (int i = 0; i < N; i++) {
              int64_t r; int32_t rs;
              if (cic_decimator_process(&h, x[i], M, &r) && r != ref[i]) ok = 0;
              if (cic_decimator_process_scaled(&hs, x[i], M, &rs)) { int gb = 3 * bits; int64_t w = (ref[i] + ((int64_t)1 << (gb - 1))) >> gb; if (rs != w) oks = 0; }
          }
          CHECK(ok, "raw output matches brute force (M=%d, full-scale random)", M);
          CHECK(oks, "scaled output matches round(brute force / 2^(3 log2 M)) (M=%d, full-scale random)", M);
      }
      /* integrators that start anywhere (here: about to wrap 2^64) change only the first N outputs, because the combs cancel the wrap */
      for (int M = 4; M <= 64; M *= 4) {
          brute_cic3(x, N, M, ref);
          cic_decimator_t h = { 0 }; h.integ[0].accum = UINT64_MAX - 12345u; h.integ[1].accum = UINT64_MAX - 7u; h.integ[2].accum = UINT64_MAX / 2 + 99u;
          int ok = 1, checked = 0;
          for (int i = 0; i < N; i++) { int64_t r; if (cic_decimator_process(&h, x[i], M, &r) && i >= 4 * M) { checked++; if (r != ref[i]) ok = 0; } }
          CHECK(ok && checked > 0, "integrator wrap-around is cancelled by the combs (M=%d, %d outputs checked)", M, checked);
      } }

    /* ---- PDM front end (HB[44]): output is Q15 PCM, +-32768 at full density ---- */
    CHECK(pdm_bit_to_pm1(1) == 1 && pdm_bit_to_pm1(0) == -1, "PDM bit -> +-1");
    { const int M = 64; int32_t o = 0, y = 0; bool ready;
      /* all ones: raw output M^3 = 262144, rescaled to Q15 full scale 32768 */
      cic_decimator_t d = { 0 }; for (int i = 0; i < M * 6; i++) { pdm_cic_feed_bit(&d, 1, M, &ready, &o); if (ready) y = o; }
      CHECK(y == 32768, "all-ones PDM stream -> +32768 (Q15 full scale), got %d", y);
      cic_decimator_t d0 = { 0 }; for (int i = 0; i < M * 6; i++) { pdm_cic_feed_bit(&d0, 0, M, &ready, &o); if (ready) y = o; }
      CHECK(y == -32768, "all-zeros PDM stream -> -32768, got %d", y);
      /* 75 % density, period-4 pattern: mean pulse = +0.5 -> half of full scale exactly (M is a multiple of the period) */
      static const uint8_t pat75[4] = { 1, 1, 1, 0 }; cic_decimator_t d75 = { 0 }; int ready_count = 0;
      for (int i = 0; i < M * 6; i++) { pdm_cic_feed_bit(&d75, pat75[i & 3], M, &ready, &o); if (ready) { y = o; ready_count++; } }
      CHECK(y == 16384 && ready_count == 6, "75%% density -> 16384 (half of full scale), got %d, one sample per M bits (%d)", y, ready_count);
      /* 50 % alternating: zero (silence) */
      cic_decimator_t d50 = { 0 }; for (int i = 0; i < M * 6; i++) { pdm_cic_feed_bit(&d50, (uint8_t)(i & 1), M, &ready, &o); if (ready) y = o; }
      CHECK(y == 0, "50%% density -> 0, got %d", y);
      /* *pcm_out is written only when a sample is ready */
      cic_decimator_t dw = { 0 }; o = 777; pdm_cic_feed_bit(&dw, 1, M, &ready, &o);
      CHECK(!ready && o == 777, "no sample ready: *pcm_out untouched"); }
    /* a real signal: first-order sigma-delta PDM of a sine, OSR 64.  Output must reproduce the sine amplitude (times the CIC droop),
     * measured by least-squares projection onto sin/cos at the tone frequency. */
    { enum { M = 64, NOUT = 256, NB = M * (NOUT + 8) };
      static uint8_t bits[NB]; const double fo = 1.0 / 16.0;   /* tone at 1/16 of the OUTPUT rate: f = fo/M cycles per PDM bit */
      const double amp = 0.5; double integ = 0.0, fb = 0.0;
      for (int n = 0; n < NB; n++) { double x = amp * sin(2.0 * M_PI * fo / M * n); integ += x - fb; bits[n] = integ >= 0.0; fb = bits[n] ? 1.0 : -1.0; }
      cic_decimator_t d = { 0 }; static double yraw[NOUT + 8]; int m = 0; bool ready; int32_t o;
      for (int n = 0; n < NB; n++) { pdm_cic_feed_bit(&d, bits[n], M, &ready, &o); if (ready) yraw[m++] = o / 32768.0; }
      double sc = 0, ss = 0, rr = 0; int cnt = 0;   /* fit y[k] = a sin(2 pi fo k) + b cos(2 pi fo k) over k = 8..NOUT+7 */
      for (int k = 8; k < NOUT + 8; k++) { sc += yraw[k] * cos(2.0 * M_PI * fo * k); ss += yraw[k] * sin(2.0 * M_PI * fo * k); cnt++; }
      double a = 2.0 * ss / cnt, b = 2.0 * sc / cnt, ampout = hypot(a, b);
      for (int k = 8; k < NOUT + 8; k++) { double e = yraw[k] - (a * sin(2.0 * M_PI * fo * k) + b * cos(2.0 * M_PI * fo * k)); rr += e * e; }
      double droop = H_cic(fo / M, M, 3);
      double snr = 10.0 * log10((ampout * ampout / 2.0) / (rr / cnt));
      printf("  [info] PDM sine: output amplitude %.4f (input 0.5 x CIC gain %.4f = %.4f), SNR %.1f dB\n", ampout, droop, 0.5 * droop, snr);
      CHECK_NEAR(ampout, amp * droop, 0.01, "PDM -> CIC output amplitude = input amplitude x closed-form CIC gain");
      CHECK(snr > 25.0, "first-order sigma-delta + order-3 CIC at OSR 64 recovers the tone with SNR > 25 dB, got %.1f", snr);
      /* rescaling a +-1 stream to the +-1 input scale (cic_decimate_stage_scaled) confines every output to {-1, 0, 1}, which is why the
       * PDM front end rescales to Q15 instead */
      cic_decimator_t ds = { 0 }; int outside = 0, nz = 0; int32_t os;
      for (int n = 0; n < NB; n++) { if (cic_decimator_process_scaled(&ds, pdm_bit_to_pm1(bits[n]), M, &os)) { if (os < -1 || os > 1) outside++; if (os != 0) nz++; } }
      CHECK(outside == 0, "scaled-to-input-scale PDM output stays within {-1, 0, 1} (%d outside)", outside);
      printf("  [info] PDM through the input-scale rescale: %d of %d outputs nonzero (amplitude 0.5 tone)\n", nz, NOUT + 8); }

    /* ---- passband droop: measured gain of the integer CIC code vs |sin(pi M f)/(M sin(pi f))|^3, order 3, M = 8 ---- */
    { const int M = 8, P = 64; double worst = 0; printf("  [info] order-3 M=8 droop (f relative to the OUTPUT rate; 0.5 = output Nyquist):\n");
      for (int j = 2; j <= 30; j += 4) {   /* tone at f_out = j/P output-rate cycles/sample */
          double fin = (double)j / (M * P);   /* cycles per input sample */
          cic_decimator_t d = { 0 }; static double y[8 + 64]; int m = 0; int64_t o;
          for (int n = 0; n < M * (P + 8); n++) { int32_t x = (int32_t)lrint(1000.0 * cos(2.0 * M_PI * fin * n)); if (cic_decimator_process(&d, x, M, &o)) y[m++] = (double)o; }
          double complex acc = 0; for (int k = 8; k < P + 8; k++) acc += y[k] * cexp(-2.0 * M_PI * I * (double)j * (k - 8) / P);
          double amp = 2.0 * cabs(acc) / P / (1000.0 * 512.0);     /* normalise by A * M^3 */
          double want = H_cic(fin, M, 3);
          printf("    f_out=%.4f  measured %.4f (%.2f dB)  closed form %.4f (%.2f dB)\n", (double)j / P, amp, 20 * log10(amp), want, 20 * log10(want));
          if (fabs(amp - want) > worst) worst = fabs(amp - want);
      }
      CHECK(worst < 2e-3, "measured CIC gain follows the closed form (worst |diff| %g)", worst);
      /* where is the -3 dB point?  solve |H| = 10^(-3/20) by bisection on the closed form, report as a fraction of the output Nyquist */
      double lo = 1e-4, hi = 1.0 / (2.0 * M); for (int it = 0; it < 60; it++) { double mid = 0.5 * (lo + hi); if (H_cic(mid, M, 3) > pow(10.0, -3.0 / 20.0)) lo = mid; else hi = mid; }
      double f3_rel_out_nyq = lo * M / 0.5;
      printf("  [info] -3 dB at f = %.4f of the output rate = %.0f%% of the output Nyquist; loss at output Nyquist = %.1f dB\n", lo * M, 100.0 * f3_rel_out_nyq, 20 * log10(H_cic(0.5 / M, M, 3)));
      CHECK(f3_rel_out_nyq < 1.0 && f3_rel_out_nyq > 0.5, "-3 dB point lies inside the output band (prose: 'about 3 dB well before the edge of the passband')");
      CHECK(20 * log10(H_cic(0.5 / M, M, 3)) < -9.0, "droop at the output Nyquist exceeds 9 dB, which is why the compensation FIR is needed"); }
    TEST_DONE("test_B_cic");
}
