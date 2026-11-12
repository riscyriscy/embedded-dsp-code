/* GROUP A: float biquad (HB[5]), q15_mul_round (HB[6], vs q15_mul_trunc HB[1]), DC blocker / one-pole LPF / notch designer (HB[8]),
 * the limit-cycle prose (HB[6] section), the flush-to-zero prose (HB[7] is reference-only; its premise is tested on the host FPU).
 * References: double-precision difference equations, closed-form frequency responses, floor()/round-half-up by definition. */
#include <stdint.h>
#include <float.h>
#include "check.h"
#include "hb_filters.h"
#if defined(__SSE__)
#include <xmmintrin.h>
#endif

/* ---- closed-form helpers ------------------------------------------------------------------------------------------- */
static double cabs2(double re, double im) { return sqrt(re * re + im * im); }
/* |H(e^{jw})| of (b0 + b1 z^-1 + b2 z^-2)/(1 + a1 z^-1 + a2 z^-2), evaluated in double from given coefficients. */
static double biquad_mag(double b0, double b1, double b2, double a1, double a2, double w) {
    double nr = b0 + b1 * cos(w) + b2 * cos(2 * w), ni = -(b1 * sin(w) + b2 * sin(2 * w));
    double dr = 1 + a1 * cos(w) + a2 * cos(2 * w), di = -(a1 * sin(w) + a2 * sin(2 * w));
    return cabs2(nr, ni) / cabs2(dr, di);
}
/* RBJ-cookbook notch, straight from the textbook formula in double (independent of the book's float code). */
static double notch_ref_mag(double f0, double fs, double q, double f) {
    double w0 = 2 * M_PI * f0 / fs, al = sin(w0) / (2 * q), c = cos(w0), a0 = 1 + al;
    return biquad_mag(1 / a0, -2 * c / a0, 1 / a0, -2 * c / a0, (1 - al) / a0, 2 * M_PI * f / fs);
}

/* ---- float biquad -------------------------------------------------------------------------------------------------------- */
static void test_biquad_float(void) {
    /* 2nd-order Butterworth low-pass, normalised cutoff 0.1 (the book's golden coefficients, scipy.signal.butter(2, 0.1)). */
    const double b0 = 0.02008337, b1 = 0.04016673, a1 = -1.56101808, a2 = 0.64135154;
    biquad_t bq = { (float)b0, (float)b1, (float)b0, (float)a1, (float)a2, 0, 0 };
    /* Reference: Direct Form I in double -- a different structure from the book's Direct Form II Transposed. */
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0, worst = 0; lcg_state = 5;
    for (int n = 0; n < 2000; n++) {
        double x = ((int)(lcg_next() & 0xFFFF) - 32768) / 32768.0;
        double y = b0 * x + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        double e = fabs((double)biquad_process(&bq, (float)x) - y); if (e > worst) worst = e;
    }
    CHECK(worst < 1e-5, "DF2T float biquad vs double DF1: worst error %g", worst);
    /* DC gain of the low-pass section is 1 (sum b / (1 + a1 + a2)). */
    biquad_t s = { (float)b0, (float)b1, (float)b0, (float)a1, (float)a2, 0, 0 }; float y = 0;
    for (int n = 0; n < 500; n++) y = biquad_process(&s, 1.0f);
    CHECK_NEAR(y, 1.0, 1e-4, "float biquad DC step settles at 1.0");
    /* Stability claim: poles inside the unit circle => impulse response decays (|a2| < 1 and |a1| < 1 + a2). */
    biquad_t d = { (float)b0, (float)b1, (float)b0, (float)a1, (float)a2, 0, 0 }; float last = 0;
    for (int n = 0; n < 400; n++) last = biquad_process(&d, n == 0 ? 1.0f : 0.0f);
    CHECK(fabsf(last) < 1e-6f, "impulse response decays (stable): %g", last);
}

/* ---- q15_mul_round vs q15_mul_trunc ------------------------------------------------------------------------------------ */
static void test_mul_round(void) {
    CHECK(q15_mul_round(24576, -16384) == -12288, "0.75 * -0.5 (book): %d", (int)q15_mul_round(24576, -16384));
    CHECK(q15_mul_trunc(19661, -16384) == -9831, "0.6 * -0.5 trunc -> %d (book -9831)", (int)q15_mul_trunc(19661, -16384));
    CHECK(q15_mul_round(19661, -16384) == -9830, "0.6 * -0.5 round -> %d (book -9830)", (int)q15_mul_round(19661, -16384));
    CHECK_NEAR(Q15_TO_FLOAT(-9830), -0.299988, 5e-7, "-9830 decodes to ~-0.299988 (book)");
    int bad = 0; double st = 0, sr = 0, worst_r = 0; const int N = 50000; lcg_state = 77;
    for (int i = 0; i < N; i++) {
        int32_t a = (int32_t)(lcg_next() & 0xFFFF) - 32768, b = (int32_t)(lcg_next() & 0xFFFF) - 32768;
        double exact = (double)a * (double)b / 32768.0;
        if ((double)q15_mul_round(a, b) != floor(exact + 0.5)) bad++;               /* round half up, by definition */
        double er = (double)q15_mul_round(a, b) - exact, et = (double)q15_mul_trunc(a, b) - exact;
        sr += er; st += et; if (fabs(er) > worst_r) worst_r = fabs(er);
    }
    CHECK(bad == 0, "q15_mul_round != floor(x + 1/2) on %d of %d", bad, N);
    CHECK(worst_r <= 0.5 + 1e-9, "round error bound %g", worst_r);
    CHECK_NEAR(sr / N, 0.0, 0.01, "round: mean error (LSB), the 'zero-mean' claim");
    CHECK_NEAR(st / N, -0.5, 0.02, "trunc: mean error (LSB), the 'one-directional bias' claim");
}

/* ---- DC blocker -------------------------------------------------------------------------------------------------------- */
static int32_t run_dc(dc_blocker_t *f, int32_t dc, int n) { int32_t y = 0; for (int i = 0; i < n; i++) y = dc_blocker_process(f, dc); return y; }

static void test_dc_blocker(void) {
    const int32_t R = 32604;  /* 0.995 */
    const double Rf = R / 32768.0, fs = 8000.0;
    CHECK_NEAR(R / 32768.0, 0.995, 1e-4, "R_q15 = 32604 is R = 0.995 (book)");

    /* Passes a 1 kHz tone at the analytic gain |1 - z^-1| / |1 - R z^-1|; steady-state rms ratio. */
    for (int hz = 100; hz <= 3000; hz += 1450) {
        dc_blocker_t g = { 0, 0, R }; double si = 0, so = 0; double w = 2 * M_PI * hz / fs;
        for (int n = 0; n < 16000; n++) { int32_t x = (int32_t)lrint(10000.0 * sin(w * n)); int32_t y = dc_blocker_process(&g, x);
            if (n >= 8000) { si += (double)x * x; so += (double)y * y; } }
        double want = cabs2(1 - cos(w), sin(w)) / cabs2(1 - Rf * cos(w), Rf * sin(w));
        CHECK_NEAR(sqrt(so / si), want, 0.01, "DC blocker tone gain");
    }
    /* -3 dB corner: book says R sets 'how low a frequency counts as DC'.  Corner ~ (1-R) fs / (2 pi) = 6.4 Hz at 8 kHz. */
    { dc_blocker_t g = { 0, 0, R }; double fc = (1 - Rf) * fs / (2 * M_PI), si = 0, so = 0, w = 2 * M_PI * fc / fs;
      for (int n = 0; n < 40000; n++) { int32_t x = (int32_t)lrint(20000.0 * sin(w * n)); int32_t y = dc_blocker_process(&g, x);
          if (n >= 20000) { si += (double)x * x; so += (double)y * y; } }
      CHECK_NEAR(sqrt(so / si), 1 / sqrt(2.0), 0.03, "gain at (1-R)fs/2pi is -3 dB"); }

    /* Settling: step of DC 5000 decays as R^n ('closer to 1.0 = slower settling'); the integer output is the ideal response rounded. */
    { dc_blocker_t g = { 0, 0, R }; double ideal = 5000.0; int ok = 1; int32_t y = 0;
      for (int n = 0; n < 1500; n++) { y = dc_blocker_process(&g, 5000); if (fabs(ideal - y) > 0.51) ok = 0;
          ideal *= Rf; }
      CHECK(ok, "DC step: integer response stays within half a count of the ideal 5000 * R^n");
      for (int n = 1500; n < 20000; n++) y = dc_blocker_process(&g, 5000);
      CHECK(y == 0, "DC step fully removed (residual %d after 20000 samples)", (int)y); }
    { /* time constant: 1/(1-R) = 200 samples; after 1000 samples (5 tau) the ideal residual is 5000 * R^1000 = 33.6 */
      dc_blocker_t g = { 0, 0, R }; int32_t y = run_dc(&g, 5000, 1000);
      CHECK_NEAR(y, 5000.0 * pow(Rf, 1000), 0.51, "residual after 1000 samples ~ 5000 R^1000"); }
    { /* a higher R settles more slowly: R = 0.9997 (book's upper value) vs 0.995, residual after 1000 samples */
      dc_blocker_t slow = { 0, 0, 32758 }; int32_t y = run_dc(&slow, 5000, 1000);
      CHECK(y > 3000, "R = 0.9997 (32758) has barely moved after 1000 samples: %d", (int)y); }

    /* The blocker removes a DC offset of either sign and of any size, including the small ones where an integer-only state would stall. */
    { static const int32_t dcs[8] = { -30000, -5000, -100, -1, 1, 100, 5000, 30000 };
      for (int i = 0; i < 8; i++) { dc_blocker_t g = { 0, 0, R }; int32_t y = run_dc(&g, dcs[i], 20000);
          CHECK(y == 0, "DC input %d settles at %d (want 0)", (int)dcs[i], (int)y); } }
    /* ...and adds no offset of its own: a zero-mean input has a zero-mean output (an integer-only state would add about -0.5/(1-R) = -100 counts). */
    { dc_blocker_t g = { 0, 0, R }; double sum = 0; int cnt = 0;
      for (int n = 0; n < 40000; n++) { int32_t x = (int32_t)lrint(100.0 * sin(2 * M_PI * 250.0 * n / fs)); int32_t y = dc_blocker_process(&g, x); if (n >= 20000) { sum += y; cnt++; } }
      CHECK(fabs(sum / cnt) < 0.5, "output mean %.3f counts on a zero-mean 250 Hz tone of amplitude 100", sum / cnt); }
    /* The output tracks the ideal double-precision recursion y = x - x1 + R y1 to within the final rounding. */
    { dc_blocker_t t = { 0, 0, R }; double px = 0, py = 0, w = 0; lcg_state = 3;
      for (int n = 0; n < 5000; n++) { int32_t x = (int32_t)((lcg_next() & 0x7FFF)) - 16384; double yy = x - px + Rf * py; px = x; py = yy;
          double e = fabs((double)dc_blocker_process(&t, x) - yy); if (e > w) w = e; }
      CHECK(w <= 0.51, "vs ideal double recursion: worst error %g counts", w); }
    /* Extreme inputs: the int64_t state stays within 2 * max|x| * 2^15 for any int32_t input (no overflow; run under UBSan too). */
    { dc_blocker_t g = { 0, 0, 32767 }; int ok = 1; lcg_state = 9;
      for (int n = 0; n < 200000; n++) { int32_t x = (lcg_next() & 1) ? INT32_MAX : INT32_MIN; dc_blocker_process(&g, x);
          if (llabs((long long)g.y_q15) > 2LL * 2147483648LL * 32768LL) ok = 0; }
      CHECK(ok, "state bound |y_q15| <= 2 * 2^31 * 2^15 holds for full-range int32_t input at R = 32767/32768"); }
}

/* ---- one-pole low-pass ---------------------------------------------------------------------------------------------- */
static void test_one_pole(void) {
    const int32_t A = 25956; const double a = A / 32768.0, fs = 8000.0;
    /* step response y[n] = T (1 - (1-a)^(n+1)) */
    { one_pole_lpf_t f = { 0, A }; int ok = 1;
      for (int n = 0; n < 12; n++) { int32_t y = one_pole_lpf_process(&f, 1000); double want = 1000.0 * (1 - pow(1 - a, n + 1)); if (fabs(y - want) > 2.0) ok = 0; }
      CHECK(ok, "one-pole step response vs closed form"); }
    /* alpha = 1.0 (32768): no smoothing, output == input (book: allowed because alpha is int32_t) */
    { one_pole_lpf_t f = { 0, 32768 }; int ok = 1; lcg_state = 11;
      for (int n = 0; n < 200; n++) { int32_t x = (int32_t)(lcg_next() & 0xFFFF) - 32768; if (one_pole_lpf_process(&f, x) != x) ok = 0; }
      CHECK(ok, "alpha = 32768 passes the input through unchanged"); }
    /* smaller alpha = more smoothing, slower response: 10-sample response to a step */
    { one_pole_lpf_t lo = { 0, 3277 }, hi = { 0, 26214 }; int32_t yl = 0, yh = 0;
      for (int n = 0; n < 5; n++) { yl = one_pole_lpf_process(&lo, 1000); yh = one_pole_lpf_process(&hi, 1000); }
      CHECK(yl < 450 && yh > 990, "alpha 0.1 -> %d, alpha 0.8 -> %d after 5 samples", (int)yl, (int)yh); }
    /* Lab 0 uses alpha = 25956 at fs = 8 kHz and says -3 dB at ~2.7 kHz.  Measure the actual gain with tones through the integer filter;
     * the closed form |H| = alpha / |1 - (1-alpha) e^-jw| gives the same numbers. */
    { double g[2]; const double hzs[2] = { 2000.0, 2680.0 };
      for (int k = 0; k < 2; k++) { one_pole_lpf_t f = { 0, A }; double si = 0, so = 0, w = 2 * M_PI * hzs[k] / fs;
          for (int n = 0; n < 16000; n++) { int32_t x = (int32_t)lrint(10000.0 * sin(w * n)); int32_t y = one_pole_lpf_process(&f, x); if (n >= 8000) { si += (double)x * x; so += (double)y * y; } }
          g[k] = sqrt(so / si); }
      for (int k = 0; k < 2; k++) { double w = 2 * M_PI * hzs[k] / fs, ref = a / cabs2(1 - (1 - a) * cos(w), (1 - a) * sin(w));
          CHECK_NEAR(20 * log10(g[k]), 20 * log10(ref), 0.05, "measured gain matches the closed form"); }
      CHECK_NEAR(20 * log10(g[0]), -2.2, 0.2, "gain at 2 kHz is -2.2 dB");
      CHECK_NEAR(20 * log10(g[1]), -3.0, 0.2, "the -3 dB point is ~2.7 kHz"); }
    /* The update floors: a rising step stops up to about 1/alpha counts short of the target, a falling step lands exactly. */
    { one_pole_lpf_t up = { 0, 655 }, dn = { 0, 655 }; int32_t yu = 0, yd = 0;   /* alpha = 0.02 */
      for (int n = 0; n < 5000; n++) { yu = one_pole_lpf_process(&up, 1000); yd = one_pole_lpf_process(&dn, -1000); }
      int expect = 0; for (int k = 1; k < 1000; k++) { if (((int64_t)655 * k) >> 15 == 0) expect = k; }
      CHECK(expect == 50, "alpha = 655: largest gap k with floor(655 k / 32768) = 0 is %d", expect);
      CHECK(yu == 1000 - expect, "alpha 0.02: step to +1000 stops %d short (at %d)", expect, (int)yu);
      CHECK(yd == -1000, "alpha 0.02: step to -1000 reaches %d exactly", (int)yd);
      one_pole_lpf_t l8 = { 0, A }; int32_t y8 = 0; for (int n = 0; n < 200; n++) y8 = one_pole_lpf_process(&l8, 1000);
      CHECK(y8 == 999, "alpha 0.8 (Lab 0): step to +1000 stops 1 count short (at %d)", (int)y8); }
}

/* ---- notch --------------------------------------------------------------------------------------------------------- */
static int settle_samples(float q) {
    biquad_t n; design_notch_biquad(&n, 60.0f, 1000.0f, q); int last = 0;
    for (int i = 0; i < 20000; i++) { float y = biquad_process(&n, i == 0 ? 1.0f : 0.0f); if (fabsf(y) > 1e-3f) last = i; }
    return last;
}
static void test_notch(void) {
    const double fs = 1000.0;
    biquad_t n; design_notch_biquad(&n, 60.0f, (float)fs, 10.0f);
    CHECK(n.z1 == 0.0f && n.z2 == 0.0f, "design clears state");
    /* Response of the coefficients the book's code produced, in double, vs the textbook closed form. */
    double worst = 0;
    for (int f = 1; f < 500; f += 7) {
        double h = biquad_mag(n.b0, n.b1, n.b2, n.a1, n.a2, 2 * M_PI * f / fs), r = notch_ref_mag(60, fs, 10, f);
        if (fabs(h - r) > worst) worst = fabs(h - r); }
    CHECK(worst < 1e-5, "notch |H(f)| vs cookbook closed form: worst %g", worst);
    double hn = biquad_mag(n.b0, n.b1, n.b2, n.a1, n.a2, 2 * M_PI * 60 / fs);
    CHECK(hn < 1e-5, "|H(60 Hz)| = %g (a true null)", hn);
    CHECK_NEAR(biquad_mag(n.b0, n.b1, n.b2, n.a1, n.a2, 0.0), 1.0, 1e-6, "unity gain at DC");
    CHECK_NEAR(biquad_mag(n.b0, n.b1, n.b2, n.a1, n.a2, M_PI), 1.0, 1e-6, "unity gain at Nyquist");
    /* bandwidth = f0/Q: find the two -3 dB points of the coefficient response by scanning */
    { double lo = 0, hi = 0, thr = 1 / sqrt(2.0);
      for (double f = 60.0; f > 40.0; f -= 0.001) { if (biquad_mag(n.b0, n.b1, n.b2, n.a1, n.a2, 2 * M_PI * f / fs) >= thr) { lo = f; break; } }
      for (double f = 60.0; f < 80.0; f += 0.001) { if (biquad_mag(n.b0, n.b1, n.b2, n.a1, n.a2, 2 * M_PI * f / fs) >= thr) { hi = f; break; } }
      CHECK_NEAR(hi - lo, 6.0, 0.3, "-3 dB bandwidth is f0/Q = 6 Hz"); }
    /* time-domain: 60 Hz tone through the book's float biquad is driven down far below the input after settling */
    { biquad_t g; design_notch_biquad(&g, 60.0f, (float)fs, 10.0f); double so = 0, si = 0;
      for (int i = 0; i < 6000; i++) { float x = (float)sin(2 * M_PI * 60.0 * i / fs); float y = biquad_process(&g, x); if (i >= 3000) { si += x * (double)x; so += y * (double)y; } }
      CHECK(10 * log10(so / si) < -60.0, "60 Hz attenuation %.1f dB (float noise floor limited)", 10 * log10(so / si)); }
    { biquad_t g; design_notch_biquad(&g, 60.0f, (float)fs, 10.0f); double so = 0, si = 0;
      for (int i = 0; i < 6000; i++) { float x = (float)sin(2 * M_PI * 120.0 * i / fs); float y = biquad_process(&g, x); if (i >= 3000) { si += x * (double)x; so += y * (double)y; } }
      CHECK_NEAR(10 * log10(so / si), 20 * log10(notch_ref_mag(60, fs, 10, 120)), 0.05, "120 Hz passes at the analytic gain"); }
    /* "higher Q ... takes longer to settle and is more sensitive to the exact frequency being right" */
    int s5 = settle_samples(5.0f), s20 = settle_samples(20.0f);
    CHECK(s20 > 2 * s5, "settle time (to 1e-3): Q=5 -> %d samples, Q=20 -> %d samples", s5, s20);
    { double m5 = notch_ref_mag(60, fs, 5, 61.2), m20 = notch_ref_mag(60, fs, 20, 61.2);
      CHECK(m20 > 2 * m5, "2%% mistuned: |H| Q=5 %.3f, Q=20 %.3f (higher Q rejects the mistuned tone less)", m5, m20); }
}

/* ---- limit cycles (HB[6] prose) ------------------------------------------------------------------------------------------ */
typedef int32_t (*mulfn)(int32_t, int32_t);
/* HARNESS (not book code): 2nd-order all-pole resonator y[n] = x[n] + a1*y[n-1] + a2*y[n-2] in Q15, with the quantising multiply in the
 * feedback path chosen by the caller.  Zero input after a Q15 impulse of 20000. */
static void resonator(mulfn m, int32_t a1, int32_t a2, int32_t *tail_min, int32_t *tail_max, int n) {
    int32_t y1 = 20000, y2 = 0; *tail_min = INT32_MAX; *tail_max = INT32_MIN;
    for (int i = 0; i < n; i++) {
        int32_t y = m(a1, y1) + m(a2, y2); y2 = y1; y1 = y;
        if (i >= n - 200) { if (y < *tail_min) *tail_min = y; if (y > *tail_max) *tail_max = y; } }
}
static int32_t mul_trunc_fn(int32_t a, int32_t b) { return q15_mul_trunc(a, b); }
static int32_t mul_round_fn(int32_t a, int32_t b) { return q15_mul_round(a, b); }
/* Magnitude truncation (round toward zero) by definition: the quantisation error always opposes the sign of the value. */
static int32_t mul_mag_fn(int32_t a, int32_t b) { int64_t p = (int64_t)a * b; return (int32_t)(p >= 0 ? (p >> 15) : -((-p) >> 15)); }

static void test_limit_cycles(void) {
    /* Premise: in exact arithmetic these poles are inside the unit circle (|a2| < 1, |a1| < 1 + a2) and the response decays to 0. */
    { double a1 = 24000 / 32768.0, a2 = -29000 / 32768.0, y1 = 20000, y2 = 0;
      for (int i = 0; i < 3000; i++) { double y = a1 * y1 + a2 * y2; y2 = y1; y1 = y; }
      CHECK(fabs(y1) < 1e-6, "ideal double response decays to %g", y1); }
    int32_t lo, hi;
    /* Truncation (floor) is biased: this section comes to rest at a stuck non-zero constant. */
    resonator(mul_trunc_fn, 24000, -29000, &lo, &hi, 3000);
    CHECK(lo == -1 && hi == -1, "truncating: a1=24000 a2=-29000 sticks at constant %d..%d", (int)lo, (int)hi);
    /* Rounding has zero mean error, but it does not remove the dead band: it rings. */
    resonator(mul_round_fn, 24000, -29000, &lo, &hi, 3000);
    CHECK(lo == -4 && hi == 4, "rounding: the same section rings %d..%d with zero input (granular limit cycle, zero-mean)", (int)lo, (int)hi);
    resonator(mul_trunc_fn, 0, -29491, &lo, &hi, 3000);
    CHECK(lo == 0 && hi == 0, "truncating: a1=0 a2=-29491 decays to exactly 0 (%d..%d)", (int)lo, (int)hi);
    resonator(mul_round_fn, 0, -29491, &lo, &hi, 3000);
    CHECK(lo == -4 && hi == 4, "rounding: a1=0 a2=-29491 rings %d..%d with zero input", (int)lo, (int)hi);
    resonator(mul_mag_fn, 24000, -29000, &lo, &hi, 3000);
    CHECK(lo == 0 && hi == 0, "magnitude truncation: a1=24000 a2=-29000 decays to exactly 0 (%d..%d)", (int)lo, (int)hi);
    /* Sweep of 300 random stable pole pairs (r in [0.8, 0.99], angle in [0.2, 2.9] rad, impulse start). */
    int tz = 0, tc = 0, to = 0, rz = 0, rc = 0, ro = 0, mz = 0, mo = 0, total = 0; lcg_state = 4242;
    for (int i = 0; i < 300; i++) {
        double r = 0.8 + 0.19 * (lcg_next() & 0xFFFF) / 65536.0, th = 0.2 + 2.7 * (lcg_next() & 0xFFFF) / 65536.0;
        int32_t a1 = (int32_t)lrint(2 * r * cos(th) * 32768), a2 = (int32_t)lrint(-r * r * 32768);
        if (a1 >= 32768 || a1 <= -32768) continue;
        total++;
        resonator(mul_trunc_fn, a1, a2, &lo, &hi, 3000); if (lo == 0 && hi == 0) tz++; else if (lo == hi) tc++; else to++;
        resonator(mul_round_fn, a1, a2, &lo, &hi, 3000); if (lo == 0 && hi == 0) rz++; else if (lo == hi) rc++; else ro++;
        resonator(mul_mag_fn, a1, a2, &lo, &hi, 3000); if (lo == 0 && hi == 0) mz++; else mo++;
    }
    printf("  [info] limit-cycle sweep (%d sections): truncate -> %d decay to 0, %d stuck at a constant, %d oscillate; round -> %d decay to 0, %d stuck, %d oscillate; magnitude truncation -> %d decay to 0, %d not\n",
           total, tz, tc, to, rz, rc, ro, mz, mo);
    CHECK(total == 150, "sweep size %d", total);
    CHECK(tc > 0 && to == 0, "truncation leaves constant-offset residues (%d) and none oscillate (%d)", tc, to);
    CHECK(tz * 4 >= total * 3 - total / 10 && tz * 4 <= total * 3 + total / 10, "about three quarters of the truncating sections decay to exactly 0: %d of %d", tz, total);
    CHECK(rz == 0 && rc == 0 && ro == total, "rounding: %d of %d sections still oscillate after 3000 zero-input samples (%d reach 0, %d stuck)", ro, total, rz, rc);
    CHECK(mz == total, "magnitude truncation: %d of %d sections decay to exactly 0", mz, total);
}

/* ---- denormals (HB[7] prose; the FPSCR listing itself is reference-only) ---------------------------------------------------- */
static int count_subnormal_tail(int ftz, float *last) {
#if defined(__SSE__)
    unsigned old = _mm_getcsr();
    if (ftz) _mm_setcsr(old | 0x8000u);   /* MXCSR.FTZ: the x86 analogue of ARM FPSCR bit 24 */
#else
    (void)ftz;
#endif
    biquad_t b = { 1.0f, 0.0f, 0.0f, -0.9f, 0.0f, 0, 0 }; int sub = 0; float y = 0;
    for (int i = 0; i < 4000; i++) { y = biquad_process(&b, i == 0 ? 1.0f : 0.0f); if (fpclassify(y) == FP_SUBNORMAL) sub++; }
    *last = y;
#if defined(__SSE__)
    _mm_setcsr(old);
#endif
    return sub;
}
static void test_denormals(void) {
    CHECK(FLT_MIN > 1.17e-38f && FLT_MIN < 1.18e-38f, "float denormal threshold %g ('roughly 1.18e-38' in the book)", (double)FLT_MIN);
    CHECK((1u << 24) == 0x01000000u, "FZ is bit 24 of FPSCR");
    /* y[n] = 0.9^n: crosses FLT_MIN at n = ln(FLT_MIN)/ln(0.9) = 829 and the smallest subnormal 1.4e-45 at n ~ 980. */
    float last; int n = count_subnormal_tail(0, &last);
    CHECK(n > 100, "a decaying one-pole float biquad produces %d subnormal outputs", n);
    CHECK(fpclassify(last) == FP_SUBNORMAL, "without FZ the state never reaches 0: after 4000 samples it is stuck at the subnormal %g (round-to-nearest 0.9 * 4 ulp -> 4 ulp)", (double)last);
#if defined(__SSE__)
    float last2; int n2 = count_subnormal_tail(1, &last2);
    CHECK(n2 == 0 && last2 == 0.0f, "with flush-to-zero set (x86 MXCSR analogue of FPSCR.FZ): %d subnormal outputs, final state %g", n2, (double)last2);
#endif
}

int main(void) {
    test_biquad_float(); test_mul_round(); test_dc_blocker(); test_one_pole(); test_notch(); test_limit_cycles(); test_denormals();
    TEST_DONE("test_A_filters");
}
