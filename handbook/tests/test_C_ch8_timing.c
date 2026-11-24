/* Chapter 11: Gardner timing error detector + timing loop (HB[70], HB[73]), closed loop.
 *
 * Test signal (generated independently of the code under test): real BPSK, +-1 symbols, analytic raised-cosine pulse
 * (alpha = 0.5), sampled at 2 samples/symbol with a timing offset tau and an optional sampling-clock error eps:
 *      x[n] = A * sum_k a_k * rc(t_n - k),   t_n = n/2 * (1+eps) - tau,   A = 8192.
 * The listings give the TED, the interpolators and the loop filter but not the code that ties them together, so the receiver
 * below is THIS TEST'S harness: per symbol, curr = interp(mu) and mid = interp(mu) one sample earlier,
 * e = gardner_error(mid, curr), mu += timing_loop_update(e), with the integer part of mu rolled into the buffer index.
 * "mu" follows the interpolator convention: timing_interpolate_linear(a, b, mu) moves from a (older) to b (newer), so LARGER
 * mu = LATER sampling instant.  gardner_error() is POSITIVE when the sampling instant is late, so timing_loop_update() returns
 * minus the PI output (its result is ADDED to mu).
 *
 * Measured quantity: e_sym = distance (in symbol periods) of the actual sampling instant from the nearest ideal one; 0 is
 * perfect, +-0.5 is the half-symbol false lock where the eye is closed.  Also the eye height min|curr|/A over the last 300 symbols.
 *
 * Gains: Q16, in units of mu per detector unit: TIMING_GAIN_Q16(g, Kd, mu_per_symbol) with Kd the measured detector gain. */
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "hb_timing.h"
#include "hb_farrow.h"
#include "hb_timing_loop.h"
#include "hb_loop_gains.h"

#define NSYM   2500
#define PAD    24
#define A_AMP  8192.0
static double ALPHA = 0.5;

static double rc(double t) {
    double s = (fabs(t) < 1e-12) ? 1.0 : sin(M_PI * t) / (M_PI * t);
    double d = 1.0 - (2 * ALPHA * t) * (2 * ALPHA * t);
    if (fabs(d) < 1e-9) return M_PI / 4.0 * s;
    return s * cos(M_PI * ALPHA * t) / d;
}
static int g_sym[NSYM + 2 * PAD + 64], g_symq[NSYM + 2 * PAD + 64];
static double s_at_rail(const int *sym, double t) {   /* continuous-time signal, symbol k at t = k - PAD */
    int k0 = (int)floor(t) + PAD; double y = 0;
    for (int k = k0 - 14; k <= k0 + 15; k++) if (k >= 0 && k < NSYM + 2 * PAD + 64) y += sym[k] * rc(t - (k - PAD));
    return y;
}
static double s_at(double t) { return s_at_rail(g_sym, t); }

/* ---- S-curve: gardner_error() on exact (analytic) samples at a static timing offset d (in symbols; d > 0 = LATE) ---------- */
static double mean_e_n(double d, int kend) {
    gardner_ted_t t = { 0, 0, 0 }; double acc = 0; int cnt = 0;
    for (int k = 30; k < kend; k++) {
        int32_t mid  = (int32_t)lrint(A_AMP * s_at(k - 0.5 + d)), cur = (int32_t)lrint(A_AMP * s_at(k + d));
        int32_t e = gardner_error(&t, mid, cur);
        if (k > 40) { acc += e; cnt++; }
    }
    return acc / cnt;
}
static double mean_e(double d) { return mean_e_n(d, 330); }

typedef struct { double e_sym, e_sym_dev, eye; int settle; } result_t;

static result_t run_loop(double tau, double eps, int32_t kp, int32_t ki, int cubic, int nsym, int verbose) {
    static int32_t x[2 * (NSYM + 40) + 16];
    const int N = 2 * (nsym + 30);
    for (int n = 0; n < N; n++) x[n] = (int32_t)lrint(A_AMP * s_at(n * 0.5 * (1.0 + eps) - tau + 20.0 - PAD));
    gardner_ted_t ted = { 0, 0, 0 };
    timing_loop_t tl = { 0, kp, ki };
    int m = 40; int32_t mu = 0; result_t r = { 0, 0, 1e9, 0 };
    double ang_s = 0, ang_c = 0;               /* circular mean of e_sym over the last 100 symbols */
    double worst_dev = 0; double ev[100]; int nev = 0;
    for (int k = 0; k < nsym; k++) {
        int32_t curr, mid;
        if (cubic) { curr = timing_interpolate_farrow_cubic(x[m - 1], x[m], x[m + 1], x[m + 2], mu); mid = timing_interpolate_farrow_cubic(x[m - 2], x[m - 1], x[m], x[m + 1], mu); }
        else       { curr = timing_interpolate_linear(x[m], x[m + 1], mu);                          mid = timing_interpolate_linear(x[m - 1], x[m], mu); }
        double tsamp = (m + mu / 32768.0) * 0.5 * (1.0 + eps) - tau + 20.0;       /* sampling instant in symbol periods (same time base as the generator) */
        double es = tsamp - floor(tsamp + 0.5);
        if (fabs(es) > 0.05) r.settle = k + 1;                              /* last symbol at which the instant was more than 0.05 symbol off */
        if (k >= nsym - 300) { double c = fabs((double)curr) / A_AMP; if (c < r.eye) r.eye = c; }
        if (k >= nsym - 100) { ang_s += sin(2 * M_PI * es); ang_c += cos(2 * M_PI * es); ev[nev++] = es; }
        int32_t e = gardner_error(&ted, mid, curr);
        int32_t adj = timing_loop_update(&tl, e);
        mu += adj;
        while (mu >= 32768) { mu -= 32768; m++; }
        while (mu < 0)      { mu += 32768; m--; }
        m += 2;
        if (verbose && k % 250 == 0) printf("    k=%4d e_sym=%+.3f e=%d integ=%d\n", k, es, e, tl.integrator);
        if (m < 4 || m + 4 >= N) { r.e_sym = 9.0; return r; }              /* ran off the buffer: lock is lost */
    }
    r.e_sym = atan2(ang_s, ang_c) / (2 * M_PI);
    for (int i = 0; i < nev; i++) { double d = fabs(ev[i] - r.e_sym); if (d > 0.5) d = 1.0 - d; if (d > worst_dev) worst_dev = d; }
    r.e_sym_dev = worst_dev;
    return r;
}


/* QPSK, two rails sharing one sampling clock: timing error from I only, or the sum of the two rails' errors (the prose's fix).
 * Returns the circular-mean e_sym, its worst deviation over the last 100 symbols, and the QUADRATURE rail's eye height. */
static result_t run_loop_qpsk(double tau, int32_t kp, int32_t ki, int sum_rails, int nsym) {
    static int32_t xi[2 * (NSYM + 40) + 16], xq[2 * (NSYM + 40) + 16];
    const int N = 2 * (nsym + 30);
    for (int n = 0; n < N; n++) { double t = n * 0.5 - tau + 20.0 - PAD; xi[n] = (int32_t)lrint(A_AMP * s_at_rail(g_sym, t)); xq[n] = (int32_t)lrint(A_AMP * s_at_rail(g_symq, t)); }
    gardner_ted_t ti = { 0, 0, 0 }, tq = { 0, 0, 0 }; timing_loop_t tl = { 0, kp, ki };
    int m = 40; int32_t mu = 0; result_t r = { 0, 0, 1e9, 0 }; double as = 0, ac = 0, ev[100]; int nev = 0;
    for (int k = 0; k < nsym; k++) {
        int32_t ci = timing_interpolate_farrow_cubic(xi[m - 1], xi[m], xi[m + 1], xi[m + 2], mu), mi = timing_interpolate_farrow_cubic(xi[m - 2], xi[m - 1], xi[m], xi[m + 1], mu);
        int32_t cq = timing_interpolate_farrow_cubic(xq[m - 1], xq[m], xq[m + 1], xq[m + 2], mu), mq = timing_interpolate_farrow_cubic(xq[m - 2], xq[m - 1], xq[m], xq[m + 1], mu);
        double tsamp = (m + mu / 32768.0) * 0.5 - tau + 20.0, es = tsamp - floor(tsamp + 0.5);
        if (fabs(es) > 0.05) r.settle = k + 1;
        if (k >= nsym - 300) { double c = fabs((double)cq) / A_AMP; if (c < r.eye) r.eye = c; }
        if (k >= nsym - 100) { as += sin(2 * M_PI * es); ac += cos(2 * M_PI * es); ev[nev++] = es; }
        int32_t e = gardner_error(&ti, mi, ci); if (sum_rails) e += gardner_error(&tq, mq, cq);
        mu += timing_loop_update(&tl, e);
        while (mu >= 32768) { mu -= 32768; m++; } while (mu < 0) { mu += 32768; m--; }
        m += 2;
    }
    r.e_sym = atan2(as, ac) / (2 * M_PI);
    for (int i = 0; i < nev; i++) { double d = fabs(ev[i] - r.e_sym); if (d > 0.5) d = 1.0 - d; if (d > r.e_sym_dev) r.e_sym_dev = d; }
    return r;
}

/* Open-loop reference: eye height min|curr|/A when sampling EXACTLY at the ideal instants (no loop), same interpolator.  The
 * interpolators themselves distort a 2-sample/symbol signal, so this (not 1.0) is what a perfect timing loop can reach. */
static double static_eye(double tau, int cubic) {
    static int32_t x[2 * (NSYM + 40) + 16];
    const int N = 2 * (400 + 30);
    for (int n = 0; n < N; n++) x[n] = (int32_t)lrint(A_AMP * s_at(n * 0.5 - tau + 20.0 - PAD));
    double eye = 1e9;
    for (int j = 60; j < 360; j++) {
        double pos = 2.0 * (j - 20 + tau);                       /* sample position of symbol j's peak */
        int m = (int)floor(pos); int32_t mu = (int32_t)lrint((pos - m) * 32768.0);
        if (mu >= 32768) { mu -= 32768; m++; }
        int32_t curr = cubic ? timing_interpolate_farrow_cubic(x[m - 1], x[m], x[m + 1], x[m + 2], mu) : timing_interpolate_linear(x[m], x[m + 1], mu);
        double c = fabs((double)curr) / A_AMP; if (c < eye) eye = c;
    }
    return eye;
}

int main(void) {
    lcg_state = 31415u;
    for (unsigned k = 0; k < sizeof g_sym / sizeof g_sym[0]; k++) g_sym[k] = ((lcg_next() >> 4) & 1) ? 1 : -1;

    /* ---- the TED's S-curve on exact samples ------------------------------------------------------------------------ */
    double e_p[5], e_m[5]; const double ds[5] = { 0.05, 0.1, 0.2, 0.3, 0.4 };
    for (int i = 0; i < 5; i++) { e_p[i] = mean_e(ds[i]); e_m[i] = mean_e(-ds[i]); }
    double e0 = mean_e(0.0), e_half = mean_e(0.5);
    printf("  [info] S-curve (mean gardner_error, A=8192): d=-0.4..+0.4 : %.0f %.0f %.0f %.0f | 0: %.1f | %.0f %.0f %.0f %.0f ; at +-0.5: %.1f\n",
           e_m[4], e_m[3], e_m[2], e_m[1], e0, e_p[1], e_p[2], e_p[3], e_p[4], e_half);
    CHECK(fabs(e0) < 0.02 * e_p[2], "S-curve passes through zero at the ideal instant (got %.1f vs %.1f)", e0, e_p[2]);
    for (int i = 0; i < 5; i++) {
        CHECK(e_p[i] > 0.0, "late sampling (d=+%.2f) gives POSITIVE error %.1f", ds[i], e_p[i]);
        CHECK(e_m[i] < 0.0, "early sampling (d=-%.2f) gives negative error %.1f", ds[i], e_m[i]);
        CHECK_NEAR(e_p[i], -e_m[i], 0.06 * fabs(e_p[i]) + 3.0, "S-curve is odd about the lock point");
    }
    CHECK(fabs(e_half) < 0.02 * e_p[2], "second zero crossing at +-0.5 symbol (the false-lock point)");
    const double Kd = (e_p[0] - e_m[0]) / 0.1;                                  /* slope at the origin, error units per symbol of offset */
    printf("  [info] detector gain Kd = %.0f error-units per symbol (closed-form scaling 1.5*A*A/65536 = %.0f)\n", Kd, 1.5 * A_AMP * A_AMP / 65536.0);
    CHECK(Kd > 0.8 * 1.5 * A_AMP * A_AMP / 65536.0 && Kd < 1.2 * 1.5 * A_AMP * A_AMP / 65536.0, "Kd %.0f vs 1.5*A^2/2^16", Kd);
    /* Kd for both pulse shapes in units of A^2/2^16 per rail: measured over 2000 symbols against the closed-form expectation
     * for i.i.d. +-1 data, dE/dd of sum_j (rc(d-j) - rc(d-1-j)) rc(d-1/2-j) -- about 1.08 (alpha 0.35) and 1.51 (alpha 0.5),
     * the "about 1.1 to 1.5" of the listing comment.  (The measured value depends on the data pattern by ~10%.) */
    { const double u = A_AMP * A_AMP / 65536.0; double k[2], th[2];
      for (int i = 0; i < 2; i++) {
          ALPHA = i ? 0.5 : 0.35;
          k[i] = (mean_e_n(0.05, 2000) - mean_e_n(-0.05, 2000)) / 0.1 / u;
          double h = 1e-4, ep = 0, em = 0;
          for (int j = -40; j <= 40; j++) { ep += (rc(h - j) - rc(h - 1 - j)) * rc(h - 0.5 - j); em += (rc(-h - j) - rc(-h - 1 - j)) * rc(-h - 0.5 - j); }
          th[i] = (ep - em) / (2 * h);
      }
      ALPHA = 0.5;
      printf("  [info] Kd / (A^2/2^16) per rail: alpha 0.35 -> measured %.3f, closed form %.3f; alpha 0.5 -> measured %.3f, closed form %.3f\n", k[0], th[0], k[1], th[1]);
      CHECK(th[0] > 1.05 && th[0] < 1.10 && th[1] > 1.48 && th[1] < 1.54, "closed-form Kd: %.3f (alpha 0.35), %.3f (alpha 0.5)", th[0], th[1]);
      CHECK(fabs(k[0] - th[0]) < 0.2 * th[0] && fabs(k[1] - th[1]) < 0.2 * th[1], "measured Kd %.3f / %.3f vs closed form %.3f / %.3f", k[0], k[1], th[0], th[1]); }

    /* ---- timing_loop_update(): arithmetic vs a 64-bit round-to-nearest reference, sign convention, no floor bias ------------ */
    const int32_t kp_f = TIMING_GAIN_Q16(0.05192, Kd, 65536.0), ki_f = TIMING_GAIN_Q16(0.001385, Kd, 65536.0);
    printf("  [info] gains: Kp=0.05192, Ki=0.001385 in this detector's units, 2 samples/symbol: kp %d, ki %d\n", kp_f, ki_f);
    CHECK_NEAR(kp_f, 0.05192 / Kd * 65536.0 * 65536.0, 1e-6 * kp_f + 1.0, "TIMING_GAIN_Q16 closed form");
    { timing_loop_t l = { 0, kp_f, ki_f }; int64_t integ = 0; int ok = 1;
      for (int i = 0; i < 200; i++) { int32_t e = (int32_t)(lcg_next() % 20001) - 10000;
          integ += ((int64_t)ki_f * e + 32768) >> 16; int64_t want = -((((int64_t)kp_f * e + 32768) >> 16) + integ);
          if (timing_loop_update(&l, e) != want) ok = 0; }
      CHECK(ok, "timing_loop_update arithmetic vs 64-bit reference (adjustment = minus the PI output)"); }
    { timing_loop_t l = { 0, kp_f, ki_f };
      CHECK(timing_loop_update(&l, 500) < 0, "late sampling (positive error) moves mu earlier (negative adjustment)");
      timing_loop_t m = { 0, kp_f, ki_f };
      CHECK(timing_loop_update(&m, -500) > 0, "early sampling (negative error) moves mu later (positive adjustment)"); }
    { timing_loop_t fx = { 0, 3403, 91 };               /* zero-mean error sequence +300, -300, ... */
      for (int i = 0; i < 1000; i++) { int32_t e = (i & 1) ? -300 : 300; timing_loop_update(&fx, e); }
      CHECK(abs(fx.integrator) <= 1, "rounded integrator stays at zero on zero-mean error (got %d)", fx.integrator); }

    /* ---- closed loop ----------------------------------------------------------------------------------------------- */
    const double taus[] = { 0.15, -0.3, 0.4, -0.05, 0.25 };
    const int NT = (int)(sizeof taus / sizeof taus[0]);

    /* locks at the eye centre from every start, linear interpolator and cubic */
    for (int cubic = 0; cubic <= 1; cubic++) for (int i = 0; i < NT; i++) {
        result_t c = run_loop(taus[i], 0.0, kp_f, ki_f, cubic, NSYM, 0);
        printf("  [%s interpolator] tau=%+.2f: e_sym %+.4f (dev %.4f) eye %.3f, within 0.05 symbol from symbol %d on\n", cubic ? "cubic" : "linear", taus[i], c.e_sym, c.e_sym_dev, c.eye, c.settle);
        CHECK(c.settle < 400, "loop (%s) tau=%+.2f settles at symbol %d", cubic ? "cubic" : "linear", taus[i], c.settle);
        CHECK(fabs(c.e_sym) < (cubic ? 0.02 : 0.06) && c.e_sym_dev < 0.05, "loop (%s) tau=%+.2f: e_sym %.4f dev %.4f", cubic ? "cubic" : "linear", taus[i], c.e_sym, c.e_sym_dev);
        double ref = static_eye(taus[i], cubic);
        printf("      open-loop eye height at the exact ideal instants (same interpolator): %.3f\n", ref);
        CHECK(c.eye > 0.9 * ref, "loop (%s) tau=%+.2f: eye height %.3f vs %.3f at the exact optimum", cubic ? "cubic" : "linear", taus[i], c.eye, ref);
    }
    /* the gains are in mu units per detector unit: a 1/40-size gain (the dimensionless design gain used unscaled) is far too slow */
    for (int i = 0; i < NT; i++) {
        result_t d = run_loop(taus[i], 0.0, kp_f / 40, ki_f / 40, 0, NSYM, 0);
        CHECK(d.settle > 800 || fabs(taus[i]) < 0.1, "40x weaker gains settle in %d symbols from tau=%+.2f; the design gains need < 400", d.settle, taus[i]);
    }
    /* Chapter 11 prose: both QPSK rails share one sampling clock, so the scalar Gardner TED on the I rail alone locks BOTH rails;
     * summing the two rails' errors doubles the detector gain Kd (so the gains are halved here to compare like with like). */
    {
        for (unsigned k = 0; k < sizeof g_symq / sizeof g_symq[0]; k++) g_symq[k] = ((lcg_next() >> 4) & 1) ? 1 : -1;
        const double tq = 0.3; double ref = static_eye(tq, 1);
        result_t one = run_loop_qpsk(tq, kp_f, ki_f, 0, NSYM), both = run_loop_qpsk(tq, kp_f / 2, ki_f / 2, 1, NSYM);
        printf("  [prose claim] QPSK, tau=%.1f: I-rail-only TED -> e_sym %+.4f (jitter %.4f), Q-rail eye %.3f ; summed rails (half gains) -> e_sym %+.4f (jitter %.4f), Q-rail eye %.3f ; open-loop eye %.3f\n",
               tq, one.e_sym, one.e_sym_dev, one.eye, both.e_sym, both.e_sym_dev, both.eye, ref);
        CHECK(fabs(one.e_sym) < 0.03 && one.eye > 0.8 * ref, "I-only Gardner locks the shared clock: e_sym %.4f, quadrature-rail eye %.3f vs %.3f", one.e_sym, one.eye, ref);
        CHECK(fabs(both.e_sym) < 0.03 && both.eye > 0.8 * ref, "summed-rail Gardner also locks: e_sym %.4f eye %.3f", both.e_sym, both.eye);
    }
    /* tracks a sampling-clock error: 0.1 % fast / slow = the optimum drifts by +-2.5 symbols over the run */
    const double epss[] = { 0.001, -0.001 };
    for (int j = 0; j < 2; j++) {
        result_t c = run_loop(0.1, epss[j], kp_f, ki_f, 1, NSYM, 0);
        printf("  [cubic] clock error %+.1f ppm: e_sym %+.4f (dev %.4f) eye %.3f\n", epss[j] * 1e6, c.e_sym, c.e_sym_dev, c.eye);
        CHECK(fabs(c.e_sym) < 0.03 && c.e_sym_dev < 0.06 && c.eye > 0.8, "loop tracks a %+.0f ppm clock error: e_sym %.4f eye %.3f", epss[j] * 1e6, c.e_sym, c.eye);
    }
    TEST_DONE("test_C_ch8_timing");
}
