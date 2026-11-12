/* Chapter 8: complex LMS equaliser (HB[76]).
 * References: double-precision complex arithmetic for lms_filter()/lms_update(); the exact Wiener (MMSE) solution of the
 * normal equations for the test channel, solved in double with a small complex Gaussian elimination; a pure-rotation
 * channel whose ideal equaliser weight is exp(-j*phi) (checks the conjugate in the update).
 * Step size: lms_update() adds (mu * e * conj(h)) >> 16 to Q15 weights, so the real step size is mu / 2^31 and the stability
 * bound on the integer mu is 2^32 / (EQ_TAPS * P), with P the average input power in raw sample units squared.  At a signal
 * level of 8000 (a quarter of Q15 full scale) that is a single-digit integer.  Measured below: stable at 0.85x the bound,
 * divergent at 1.15x. */
#include <stdint.h>
#include <string.h>
#include <complex.h>
#include "check.h"
#include "hb_lms_eq.h"

typedef void (*upd_fn)(lms_equalizer_t *, int32_t, int32_t);
static const double complex CH[3] = { 1.0, 0.35 - 0.2 * I, -0.15 + 0.1 * I };

typedef struct { double mse_rel, pin; int diverged; lms_equalizer_t eq; } run_t;

static void sym(double A, int32_t *si, int32_t *sq) { *si = ((lcg_next() >> 5) & 1) ? (int32_t)A : -(int32_t)A; *sq = ((lcg_next() >> 6) & 1) ? (int32_t)A : -(int32_t)A; }

/* training run: QPSK (+-A,+-A) through CH, equaliser sees x, desired = transmitted symbol (no delay: CH[0] is the main tap) */
static run_t run_lms(double A, int32_t mu, upd_fn upd, int nsym) {
    run_t r; memset(&r, 0, sizeof r);
    r.eq.weights_i[0] = 32768; r.eq.mu = mu;
    lcg_state = 12345u;
    int32_t hi[3] = { 0, 0, 0 }, hq[3] = { 0, 0, 0 };
    double acc = 0; int cnt = 0; double pin = 0;
    for (int n = 0; n < nsym; n++) {
        int32_t ai, aq; sym(A, &ai, &aq);
        for (int k = 2; k > 0; k--) { hi[k] = hi[k - 1]; hq[k] = hq[k - 1]; }
        hi[0] = ai; hq[0] = aq;
        double complex y = 0; for (int k = 0; k < 3; k++) y += CH[k] * (hi[k] + I * hq[k]);
        int32_t xi = (int32_t)lrint(creal(y)), xq = (int32_t)lrint(cimag(y));
        pin += (double)xi * xi + (double)xq * xq;
        int32_t oi, oq, ei, eq; lms_filter(&r.eq, xi, xq, &oi, &oq);
        lms_compute_error(oi, oq, ai, aq, &ei, &eq);
        if (llabs(ei) > (1 << 24) || llabs(eq) > (1 << 24)) { r.diverged = 1; break; }       /* stop before the int32 weights overflow */
        if (n >= nsym - 1000) { acc += (double)ei * ei + (double)eq * eq; cnt++; }
        upd(&r.eq, ei, eq);
    }
    r.pin = pin / nsym; r.mse_rel = cnt ? acc / cnt / (2.0 * A * A) : 1e30;
    return r;
}

/* ---- Wiener solution for the test channel: R w = p with R_kl = E[conj(x_k) x_l], p_k = E[conj(x_k) d], unit-power symbols (sigma^2 = 1) ---- */
static void wiener(double complex *w, double *jmin) {
    enum { N = EQ_TAPS }; double complex R[N][N + 1];
    for (int k = 0; k < N; k++) {
        for (int l = 0; l < N; l++) { double complex s = 0; for (int m = 0; m < 3; m++) { int m2 = m + k - l; if (m2 >= 0 && m2 < 3) s += conj(CH[m]) * CH[m2]; } R[k][l] = s; }
        R[k][N] = (k == 0) ? conj(CH[0]) : 0.0;                       /* d[n] = s[n] correlates only with x[n] (through h_0) */
    }
    for (int c = 0; c < N; c++) {
        int piv = c; for (int r = c + 1; r < N; r++) if (cabs(R[r][c]) > cabs(R[piv][c])) piv = r;
        for (int j = 0; j <= N; j++) { double complex t = R[c][j]; R[c][j] = R[piv][j]; R[piv][j] = t; }
        for (int r = 0; r < N; r++) if (r != c) { double complex f = R[r][c] / R[c][c]; for (int j = c; j <= N; j++) R[r][j] -= f * R[c][j]; }
    }
    double complex dot = 0;
    for (int k = 0; k < N; k++) { w[k] = R[k][N] / R[k][k]; dot += w[k] * ((k == 0) ? CH[0] : 0.0) ; }
    /* J_min = sigma_d^2 - Re( sum_k w_k * conj(p_k) ),  p_0 = conj(h_0)  =>  conj(p_0) = h_0 */
    *jmin = 1.0 - creal(dot);
}

int main(void) {
    /* ---- lms_filter(): history shift + complex multiply vs double ---------------------------------------------------- */
    {
        lms_equalizer_t eq; memset(&eq, 0, sizeof eq); lcg_state = 77u;
        double complex w[EQ_TAPS], h[EQ_TAPS]; for (int k = 0; k < EQ_TAPS; k++) { w[k] = 0; h[k] = 0; }
        for (int k = 0; k < EQ_TAPS; k++) { eq.weights_i[k] = (int32_t)(lcg_next() % 65537) - 32768; eq.weights_q[k] = (int32_t)(lcg_next() % 65537) - 32768; w[k] = eq.weights_i[k] + I * eq.weights_q[k]; }
        int bad = 0;
        for (int n = 0; n < 500; n++) {
            int32_t xi = (int32_t)(lcg_next() % 20001) - 10000, xq = (int32_t)(lcg_next() % 20001) - 10000;
            for (int k = EQ_TAPS - 1; k > 0; k--) h[k] = h[k - 1];
            h[0] = xi + I * xq;
            double complex y = 0; for (int k = 0; k < EQ_TAPS; k++) y += w[k] * h[k];
            int32_t oi, oq; lms_filter(&eq, xi, xq, &oi, &oq);
            if (oi != (int32_t)floor(creal(y) / 32768.0) || oq != (int32_t)floor(cimag(y) / 32768.0)) bad++;
        }
        CHECK(bad == 0, "lms_filter vs double complex FIR: %d mismatches", bad);
    }
    /* ---- lms_update(): one step vs the double formula w += mu e conj(h) / 2^31 (floor) ------------------------------------ */
    {
        lms_equalizer_t eq; memset(&eq, 0, sizeof eq); lcg_state = 78u; int bad = 0;
        for (int trial = 0; trial < 300; trial++) {
            for (int k = 0; k < EQ_TAPS; k++) { eq.history_i[k] = (int32_t)(lcg_next() % 20001) - 10000; eq.history_q[k] = (int32_t)(lcg_next() % 20001) - 10000;
                                                  eq.weights_i[k] = (int32_t)(lcg_next() % 65537) - 32768; eq.weights_q[k] = (int32_t)(lcg_next() % 65537) - 32768; }
            eq.mu = (int32_t)(lcg_next() % 50) + 1; int32_t e_i = (int32_t)(lcg_next() % 8001) - 4000, e_q = (int32_t)(lcg_next() % 8001) - 4000;
            int32_t wi[EQ_TAPS], wq[EQ_TAPS]; memcpy(wi, eq.weights_i, sizeof wi); memcpy(wq, eq.weights_q, sizeof wq);
            lms_update(&eq, e_i, e_q);
            for (int k = 0; k < EQ_TAPS; k++) {
                double complex d = (e_i + I * e_q) * conj(eq.history_i[k] + I * eq.history_q[k]);
                double want_i = wi[k] + floor(eq.mu * creal(d) / 65536.0), want_q = wq[k] + floor(eq.mu * cimag(d) / 65536.0);
                if (eq.weights_i[k] != (int32_t)want_i || eq.weights_q[k] != (int32_t)want_q) bad++;
            }
        }
        CHECK(bad == 0, "lms_update vs double complex formula: %d mismatches", bad);
    }
    { int32_t ei, eq; lms_compute_error(100, -50, 300, 40, &ei, &eq); CHECK(ei == 200 && eq == 90, "lms_compute_error = decided - output"); }

    /* ---- conjugate check: a pure phase rotation must be undone, weight -> exp(-j phi) ---------------------------------- */
    for (int deg = 30; deg < 360; deg += 90) {
        double phi = deg * M_PI / 180.0; lms_equalizer_t eq; memset(&eq, 0, sizeof eq); eq.weights_i[0] = 32768; eq.mu = 200; lcg_state = 4u;
        const double A = 300.0; double complex rot = cexp(I * phi);
        for (int n = 0; n < 4000; n++) {
            int32_t ai, aq; sym(A, &ai, &aq); double complex y = rot * (ai + I * aq);
            int32_t oi, oq, ei, eqq; lms_filter(&eq, (int32_t)lrint(creal(y)), (int32_t)lrint(cimag(y)), &oi, &oq);
            lms_compute_error(oi, oq, ai, aq, &ei, &eqq); lms_update(&eq, ei, eqq);
        }
        CHECK_NEAR(eq.weights_i[0] / 32768.0, cos(phi), 0.02, "weight real part = cos(phi)");
        CHECK_NEAR(eq.weights_q[0] / 32768.0, -sin(phi), 0.02, "weight imaginary part = -sin(phi): the conjugate in the update is right");
        for (int k = 1; k < EQ_TAPS; k++) CHECK(abs(eq.weights_i[k]) < 800 && abs(eq.weights_q[k]) < 800, "idle tap %d stays near 0 (%d,%d)", k, eq.weights_i[k], eq.weights_q[k]);
    }

    /* ---- multipath channel: steady-state error vs the Wiener MMSE, weights vs the Wiener weights ------------------------ */
    double complex wopt[EQ_TAPS]; double jmin; wiener(wopt, &jmin);
    printf("  [info] Wiener solution for the %d-tap equaliser: J_min = %.5f of the symbol power; w0 = %.4f%+.4fj\n", EQ_TAPS, jmin, creal(wopt[0]), cimag(wopt[0]));
    CHECK(jmin > 0.0 && jmin < 0.2, "MMSE sanity (%.5f)", jmin);
    {
        const double A = 1000.0; run_t r = run_lms(A, 20, lms_update, 20000);
        double wd = 0; for (int k = 0; k < EQ_TAPS; k++) { double complex w = (r.eq.weights_i[k] + I * r.eq.weights_q[k]) / 32768.0; if (cabs(w - wopt[k]) > wd) wd = cabs(w - wopt[k]); }
        printf("  [info] LMS (mu=20, A=1000): steady MSE %.5f of symbol power vs MMSE %.5f; worst weight deviation from Wiener %.4f\n", r.mse_rel, jmin, wd);
        CHECK(!r.diverged, "LMS diverged");
        CHECK(r.mse_rel >= 0.9 * jmin && r.mse_rel < 1.3 * jmin + 1e-4, "steady-state MSE %.5f vs Wiener MMSE %.5f (excess of LMS at this mu is a few %%)", r.mse_rel, jmin);
        CHECK(wd < 0.03, "weights vs Wiener solution: worst deviation %.4f", wd);
        /* and the open-eye consequence: uncompensated, the channel has a larger error than the equalised link */
        CHECK(r.mse_rel < 0.5 * (cabs(CH[1]) * cabs(CH[1]) + cabs(CH[2]) * cabs(CH[2])), "equaliser must remove most of the ISI power");
    }

    /* ---- stability bound: the prose's 2/(N P), in the units that make it true (mu_int < 2^32/(N P)) ----------------------- */
    const double As[] = { 300.0, 8000.0 };
    for (int ai = 0; ai < 2; ai++) {
        const double A = As[ai]; run_t probe = run_lms(A, 1, lms_update, 2000);
        const double bound = 4294967296.0 / (EQ_TAPS * probe.pin);
        printf("  A=%.0f: input power P = %.3g, bound mu_int < 2^32/(N*P) = %.2f", A, probe.pin, bound);
        int mu_lo = (int)floor(0.85 * bound), mu_hi = (int)ceil(1.15 * bound); if (mu_lo < 1) mu_lo = 1;
        run_t lo = run_lms(A, mu_lo, lms_update, 8000), hi = run_lms(A, mu_hi, lms_update, 8000);
        printf("; mu=%d (%.2fx): %s mse %.2g ; mu=%d (%.2fx): %s\n", mu_lo, mu_lo / bound, lo.diverged ? "DIVERGED" : "stable", lo.mse_rel, mu_hi, mu_hi / bound, hi.diverged ? "DIVERGED" : "stable");
        CHECK(!lo.diverged && lo.mse_rel < 0.05, "A=%.0f: mu=%d (%.2fx the unit-corrected bound) must be stable (mse %.3g)", A, mu_lo, mu_lo / bound, lo.mse_rel);
        CHECK(hi.diverged || hi.mse_rel > 100.0, "A=%.0f: mu=%d (%.2fx the bound) must diverge", A, mu_hi, mu_hi / bound);
        if (ai == 1) CHECK(bound < 10.0, "at a Q15-scale signal the integer mu has only %d usable values", (int)floor(bound));
    }

    TEST_DONE("test_C_ch8_lms");
}
