/* Chapter 10 (OFDM), HB[83]..HB[89] (src/hb_ofdm.c, with fft() from src/hb_fft.c and the QPSK table from src/hb_qpsk.c).
 *
 * Independent references used here (nothing below calls the code under test to compute an expectation):
 *   - a naive O(N^2) DFT/IDFT in long double, written from the definition;
 *   - a linear convolution of the time-domain TX stream with a short multipath impulse response (double), and the
 *     closed-form channel response H[k] = sum_l h[l] e^{-j 2 pi k l / N};
 *   - closed forms: an all-in-phase subcarrier set gives PAPR = N exactly; a CP-correlation phase of -2 pi eps; the
 *     raised-cosine ramp 0.5 (1 - cos(pi i / W)). */
#include <stdint.h>
#include <string.h>
#include <complex.h>
#include "check.h"
#include "hb_ofdm.h"

#define NMAX 2048
#define PI_D 3.14159265358979323846

static double frand(void) { return ((int)(lcg_next() & 0xFFFF) - 32768) / 32768.0; }   /* [-1, 1) */

/* sign = -1: forward DFT; sign = +1: inverse DFT (with 1/N). */
static void naive_dft(const double complex *x, double complex *X, int n, int sign) {
    for (int k = 0; k < n; k++) {
        long double re = 0, im = 0;
        for (int m = 0; m < n; m++) {
            long double ang = (long double)sign * 2.0L * 3.14159265358979323846L * (long double)((long long)k * m % n) / n;
            re += creal(x[m]) * cosl(ang) - cimag(x[m]) * sinl(ang);
            im += creal(x[m]) * sinl(ang) + cimag(x[m]) * cosl(ang);
        }
        X[k] = (double)re + I * (double)im;
        if (sign > 0) X[k] /= n;
    }
}

static complex_t cf(double complex z) { complex_t r = { (float)creal(z), (float)cimag(z) }; return r; }
static double complex cd(complex_t z) { return (double)z.re + I * (double)z.im; }

static double papr_db(const complex_t *x, int n) {
    double pk = 0, sum = 0;
    for (int i = 0; i < n; i++) { double p = (double)x[i].re * x[i].re + (double)x[i].im * x[i].im; if (p > pk) pk = p; sum += p; }
    return 10.0 * log10(pk / (sum / n));
}

static complex_t xs[NMAX], ys[NMAX], zs[NMAX], scratch[NMAX];
static double complex Xd[NMAX], Rd[NMAX];

/* reference QPSK point for a dibit, from the chapter's own description: bit1 = I sign (1 -> negative), bit0 = Q sign */
static double complex qpsk_ref(int dibit) {
    double a = 23170.0;
    return ((dibit & 2) ? -a : a) + I * ((dibit & 1) ? -a : a);
}

int main(void) {
    /* ===================== HB[83] ifft(): conjugate - fft - conjugate - /N ===================== */
    for (int trial = 0; trial < 3; trial++) {
        int n = (trial == 0) ? 8 : (trial == 1) ? 64 : 256;
        for (int k = 0; k < n; k++) { xs[k].re = (float)frand(); xs[k].im = (float)frand(); Xd[k] = cd(xs[k]); }
        naive_dft(Xd, Rd, n, +1);                       /* IDFT reference */
        ifft(xs, ys, n);
        double e = 0, pk = 0;
        for (int k = 0; k < n; k++) { double d = cabs(cd(ys[k]) - Rd[k]); if (d > e) e = d; if (cabs(Rd[k]) > pk) pk = cabs(Rd[k]); }
        CHECK(e < 1e-5 * (pk > 0 ? pk : 1) + 1e-7, "ifft n=%d vs naive IDFT: max abs error %g (peak %g)", n, e, pk);
        /* documented side effect: the input buffer is conjugated in place */
        int ok = 1;
        for (int k = 0; k < n; k++) if (xs[k].re != (float)creal(Xd[k]) || xs[k].im != -(float)cimag(Xd[k])) ok = 0;
        CHECK(ok, "ifft n=%d must leave its input conjugated in place (as the listing's comment says)", n);
    }
    /* round trip fft(ifft(X)) = X up to the maximum size the listing sizes its workspace for */
    {
        int n = OFDM_MAX_SUBCARRIERS;
        for (int k = 0; k < n; k++) { xs[k].re = (float)frand(); xs[k].im = (float)frand(); zs[k] = xs[k]; }
        ifft(xs, ys, n);
        fft(ys, scratch, n);
        double e = 0; for (int k = 0; k < n; k++) { double d = hypot(scratch[k].re - zs[k].re, scratch[k].im - zs[k].im); if (d > e) e = d; }
        CHECK(e < 5e-5, "fft(ifft(X)) round trip at N = OFDM_MAX_SUBCARRIERS = %d: max error %g", n, e);
    }

    /* ===================== HB[83] ofdm_modulate_symbol(): bit packing, QPSK mapping, 1/N scaling ===================== */
    {
        int n = 64; uint8_t bits[16];
        for (int b = 0; b < 16; b++) bits[b] = (uint8_t)(lcg_next() >> 3);
        for (int k = 0; k < n; k++) {
            /* independent description of the packing: the packed stream is LSB-first, bit number m = byte m/8, bit m%8;
             * subcarrier k takes stream bits 2k (low bit of the dibit) and 2k+1 (high bit). */
            int b0 = (bits[(2 * k) / 8] >> ((2 * k) % 8)) & 1, b1 = (bits[(2 * k + 1) / 8] >> ((2 * k + 1) % 8)) & 1;
            Xd[k] = qpsk_ref(b0 | (b1 << 1));
        }
        naive_dft(Xd, Rd, n, +1);
        ofdm_modulate_symbol(bits, n, scratch, ys);
        double e = 0; for (int k = 0; k < n; k++) { double d = cabs(cd(ys[k]) - Rd[k]); if (d > e) e = d; }
        CHECK(e < 0.05, "ofdm_modulate_symbol vs naive IDFT of the Gray-coded QPSK subcarriers: max error %g (amplitude ~ 23170/N)", e);
        /* and recover the subcarriers with the receive FFT: sign decisions must return the original dibits */
        fft(ys, zs, n);
        int wrong = 0;
        for (int k = 0; k < n; k++) {
            int dibit = (cd(zs[k]) != 0) ? ((creal(Xd[k]) < 0) ? 2 : 0) | ((cimag(Xd[k]) < 0) ? 1 : 0) : 0;
            int got = ((zs[k].re < 0) ? 2 : 0) | ((zs[k].im < 0) ? 1 : 0);
            if (got != dibit) wrong++;
        }
        CHECK(wrong == 0, "TX -> RX FFT recovers all %d subcarrier dibits (%d wrong)", n, wrong);
        /* example_ofdm_tx_call is a one-line wrapper around the same thing with a static workspace */
        ofdm_modulate_symbol(bits, n, scratch, ys);
        example_ofdm_tx_call(bits, n, zs);
        CHECK(memcmp(ys, zs, n * sizeof(complex_t)) == 0, "example_ofdm_tx_call == ofdm_modulate_symbol");
    }

    /* ===================== HB[84] cyclic prefix ===================== */
    {
        int n = 64, cp = 16; uint8_t bits[16];
        for (int b = 0; b < 16; b++) bits[b] = (uint8_t)(lcg_next() >> 4);
        ofdm_modulate_symbol(bits, n, scratch, ys);
        add_cyclic_prefix(ys, n, cp, zs);
        int ok = 1;
        for (int i = 0; i < cp; i++) if (zs[i].re != ys[n - cp + i].re || zs[i].im != ys[n - cp + i].im) ok = 0;
        for (int i = 0; i < n; i++) if (zs[cp + i].re != ys[i].re || zs[cp + i].im != ys[i].im) ok = 0;
        CHECK(ok, "CP = last cp_len samples copied in front, then the whole symbol");
        /* remove_cp_and_demod: after dropping the CP, the FFT must equal the FFT of the symbol (naive DFT reference) */
        remove_cp_and_demod(zs, n, cp, xs);
        for (int k = 0; k < n; k++) Xd[k] = cd(ys[k]);
        naive_dft(Xd, Rd, n, -1);
        double e = 0, pk = 0; for (int k = 0; k < n; k++) { double d = cabs(cd(xs[k]) - Rd[k]); if (d > e) e = d; if (cabs(Rd[k]) > pk) pk = cabs(Rd[k]); }
        CHECK(e < 1e-4 * pk, "remove_cp_and_demod == DFT of the payload part: max error %g (peak %g)", e, pk);
    }

    /* ===================== HB[84]+[79]: CP turns a multipath channel into one complex gain per subcarrier ===================== */
    {
        int n = 64, cp = 16, L = 4;
        double complex h[4] = { 1.0, 0.5 * cexp(I * PI_D / 3), 0.3 * cexp(-I * PI_D / 5), 0.2 };
        double complex Hk[64];
        for (int k = 0; k < n; k++) { Hk[k] = 0; for (int l = 0; l < L; l++) Hk[k] += h[l] * cexp(-I * 2 * PI_D * k * l / n); }
        for (int pass = 0; pass < 2; pass++) {
            int cpl = pass == 0 ? cp : 0;                          /* pass 1 is the negative control: no cyclic prefix */
            /* two consecutive symbols with their CPs; analyse the second one (the first one's tail leaks into it) */
            uint8_t b1[16], b2[16];
            for (int b = 0; b < 16; b++) { b1[b] = (uint8_t)(lcg_next() >> 2); b2[b] = (uint8_t)(lcg_next() >> 5); }
            static complex_t s1[NMAX], s2[NMAX], c1[NMAX], c2[NMAX];
            ofdm_modulate_symbol(b1, n, scratch, s1);
            ofdm_modulate_symbol(b2, n, scratch, s2);
            add_cyclic_prefix(s1, n, cpl, c1);
            add_cyclic_prefix(s2, n, cpl, c2);
            int len1 = n + cpl, total = 2 * (n + cpl);
            static double complex tx[2 * NMAX], rx[2 * NMAX];
            for (int i = 0; i < len1; i++) { tx[i] = cd(c1[i]); tx[len1 + i] = cd(c2[i]); }
            for (int i = 0; i < total; i++) { rx[i] = 0; for (int l = 0; l < L; l++) if (i - l >= 0) rx[i] += h[l] * tx[i - l]; }
            for (int i = 0; i < len1; i++) zs[i] = cf(rx[len1 + i]);        /* received second symbol incl. its CP slot */
            remove_cp_and_demod(zs, n, cpl, xs);                             /* Y[k] */
            /* transmitted subcarriers of symbol 2 (independent reference: DFT of the time samples that were sent) */
            for (int k = 0; k < n; k++) Xd[k] = cd(s2[k]);
            naive_dft(Xd, Rd, n, -1);                                        /* X[k] */
            double e_gain = 0, e_eq = 0, pkX = 0;
            for (int k = 0; k < n; k++) {
                double d = cabs(cd(xs[k]) - Hk[k] * Rd[k]); if (d > e_gain) e_gain = d;
                complex_t q = equalize_subcarrier(xs[k], cf(Hk[k]));
                double d2 = cabs(cd(q) - Rd[k]); if (d2 > e_eq) e_eq = d2;
                if (cabs(Rd[k]) > pkX) pkX = cabs(Rd[k]);
            }
            if (pass == 0) {
                CHECK(e_gain < 2e-4 * pkX, "with CP: Y[k] == H[k] X[k] for every subcarrier (max err %g, |X| ~ %g)", e_gain, pkX);
                CHECK(e_eq < 2e-4 * pkX, "with CP: one-tap equalizer recovers X[k] (max err %g)", e_eq);
                int wrong = 0;
                for (int k = 0; k < n; k++) {
                    /* the equalized point's quadrant must match the transmitted one */
                    complex_t q = equalize_subcarrier(xs[k], cf(Hk[k]));
                    int tx_d = ((creal(Rd[k]) < 0) ? 2 : 0) | ((cimag(Rd[k]) < 0) ? 1 : 0);
                    int rx_d = ((q.re < 0) ? 2 : 0) | ((q.im < 0) ? 1 : 0);
                    if (tx_d != rx_d) wrong++;
                }
                CHECK(wrong == 0, "with CP: all %d QPSK subcarriers decoded correctly through a 4-tap multipath channel (%d wrong)", n, wrong);
            } else {
                CHECK(e_eq > 0.05 * pkX, "negative control: WITHOUT a CP the same equalizer must NOT recover X (max err %g vs |X| %g)", e_eq, pkX);
            }
        }
    }

    /* ===================== HB[86] equalize_subcarrier against complex division in double ===================== */
    {
        double worst = 0;
        for (int t = 0; t < 2000; t++) {
            double complex r = frand() * 3 + I * frand() * 3, hh = (frand() + 1.1) * cexp(I * PI_D * frand());
            if (cabs(hh) < 0.05) continue;
            complex_t q = equalize_subcarrier(cf(r), cf(hh));
            double complex ref = r / hh;
            double d = cabs(cd(q) - ref) / (cabs(ref) + 1e-9); if (d > worst) worst = d;
        }
        CHECK(worst < 1e-5, "equalize_subcarrier == received / H (relative error %g)", worst);
        complex_t zero = { 0.0f, 0.0f }, one = { 1.0f, 0.0f };
        complex_t q = equalize_subcarrier(one, zero);
        CHECK(!isfinite(q.re) || !isfinite(q.im), "H = 0 (deep null) gives a non-finite result: the equalizer has no guard (got %g,%g)", q.re, q.im);
    }

    /* ===================== HB[87] comb-pilot channel estimate ===================== */
    {
        int n = 64; static complex_t rxs[NMAX], pil[NMAX], est[NMAX];
        double complex a0 = 0.8 + 0.3 * I, slope = (0.004 - 0.002 * I);
        double complex H[NMAX];
        for (int k = 0; k < n; k++) {
            H[k] = a0 + slope * k;                                    /* channel exactly linear in k */
            pil[k] = cf(qpsk_ref((k / 8) & 3) / 23170.0);              /* known pilot symbols (unit magnitude * sqrt2) */
            double complex data = qpsk_ref((k * 7 + 1) & 3) / 23170.0;
            rxs[k] = cf(H[k] * ((k % PILOT_SPACING == 0) ? cd(pil[k]) : data));
            est[k].re = 9999.0f; est[k].im = 9999.0f;                 /* sentinel */
        }
        estimate_channel_comb_pilots(rxs, pil, est, n);
        double e = 0; int last_pilot = ((n - 1) / PILOT_SPACING) * PILOT_SPACING;   /* 56 */
        for (int k = 0; k <= last_pilot; k++) { double d = cabs(cd(est[k]) - H[k]); if (d > e) e = d; }
        CHECK(e < 1e-5, "pilot estimates and linear interpolation between pilots are exact for a linear channel (max err %g)", e);
        /* the subcarriers after the last pilot get the last pilot's estimate, whatever the buffer held before */
        int tail_ok = 1; for (int k = last_pilot + 1; k < n; k++) if (est[k].re != est[last_pilot].re || est[k].im != est[last_pilot].im) tail_ok = 0;
        CHECK(tail_ok, "the %d subcarriers after the last pilot (k=%d..%d) hold the last pilot's estimate", PILOT_SPACING - 1, last_pilot + 1, n - 1);
        {
            /* a receiver that equalizes all subcarriers from a zero-initialised estimate buffer gets finite values everywhere */
            static complex_t est0[NMAX]; memset(est0, 0, sizeof est0);
            estimate_channel_comb_pilots(rxs, pil, est0, n);
            int finite = 1; for (int k = 0; k < n; k++) { complex_t q = equalize_subcarrier(rxs[k], est0[k]); if (!isfinite(q.re) || !isfinite(q.im)) finite = 0; }
            CHECK(finite, "every one of the %d subcarriers equalizes to a finite value", n);
            /* a non-multiple-of-8 size exercises the general tail formula */
            for (int k = 0; k < 60; k++) { est[k].re = 9999.0f; est[k].im = 9999.0f; }
            estimate_channel_comb_pilots(rxs, pil, est, 60);
            CHECK(est[59].re == est[56].re && est[57].im == est[56].im && est[56].re != 9999.0f, "N = 60: pilots at 0..56, tail 57..59 held");
            /* and sizes where the last subcarrier is itself a pilot (N = 9, 57) or the only pilot is k = 0 (N = 1, 5, 8) */
            static const int sizes[] = { 1, 5, 8, 9, 57 };
            for (unsigned z = 0; z < sizeof sizes / sizeof *sizes; z++) {
                int nn = sizes[z], lp = ((nn - 1) / PILOT_SPACING) * PILOT_SPACING, ok = 1;
                for (int k = 0; k < nn; k++) { est[k].re = 9999.0f; est[k].im = 9999.0f; }
                estimate_channel_comb_pilots(rxs, pil, est, nn);
                for (int k = 0; k < nn; k++) if (est[k].re == 9999.0f && est[k].im == 9999.0f) ok = 0;
                for (int k = lp + 1; k < nn; k++) if (est[k].re != est[lp].re || est[k].im != est[lp].im) ok = 0;
                CHECK(ok, "N = %d: every subcarrier receives an estimate, tail after pilot %d held", nn, lp);
            }
        }
        /* and a frequency-selective channel: result must equal linear interpolation between the true pilot gains */
        double complex h2[4] = { 1.0, 0.5 * cexp(I * PI_D / 3), 0.3 * cexp(-I * PI_D / 5), 0.2 };
        for (int k = 0; k < n; k++) { H[k] = 0; for (int l = 0; l < 4; l++) H[k] += h2[l] * cexp(-I * 2 * PI_D * k * l / n); rxs[k] = cf(H[k] * cd(pil[k])); }
        estimate_channel_comb_pilots(rxs, pil, est, n);
        double e2 = 0, e_vs_truth = 0;
        for (int k = 0; k < last_pilot; k++) {
            int k0 = (k / 8) * 8; double fr = (double)(k - k0) / 8.0;
            double complex ref = H[k0] + fr * (H[k0 + 8] - H[k0]);
            double d = cabs(cd(est[k]) - ref); if (d > e2) e2 = d;
            double dt = cabs(cd(est[k]) - H[k]); if (dt > e_vs_truth) e_vs_truth = dt;
        }
        CHECK(e2 < 1e-5, "frequency-selective channel: estimate == linear interpolation of the true pilot gains (max err %g)", e2);
        printf("  [info] comb-pilot interpolation error vs true 4-tap channel: max %.4f (|H| up to ~1.7)\n", e_vs_truth);
        CHECK(e_vs_truth > 0.001, "interpolation error vs the true channel is real for a 4-tap channel at spacing 8: max %g (the prose says wider pilot spacing needs a flatter channel)", e_vs_truth);
    }

    /* ===================== HB[85] CFO estimate from the cyclic prefix ===================== */
    {
        int n = 64, cp = 16; uint8_t bits[16];
        for (int b = 0; b < 16; b++) bits[b] = (uint8_t)(lcg_next() >> 6);
        static complex_t sym[NMAX], tx_cp[NMAX], rx[NMAX], blk[NMAX];
        ofdm_modulate_symbol(bits, n, scratch, sym);
        add_cyclic_prefix(sym, n, cp, tx_cp);
        for (int k = 0; k < n; k++) Xd[k] = cd(sym[k]);
        naive_dft(Xd, Rd, n, -1);                                   /* transmitted subcarriers */
        static const double eps_list[] = { 0.0, 0.05, -0.1, 0.25, -0.33, 0.45, -0.49 };
        for (unsigned t = 0; t < sizeof eps_list / sizeof *eps_list; t++) {
            double eps = eps_list[t];                               /* frequency offset in subcarrier spacings */
            for (int i = 0; i < n + cp; i++) rx[i] = cf(cd(tx_cp[i]) * cexp(I * 2 * PI_D * eps * i / n));
            complex_t corr = cfo_estimate_from_cp(rx, n, cp);
            double ph = atan2((double)corr.im, (double)corr.re);
            double want = -2.0 * PI_D * eps;                        /* a(i) * conj(a(i)*e^{j 2 pi eps}) = |a|^2 e^{-j 2 pi eps} */
            if (eps == 0.0) want = 0.0;
            CHECK_NEAR(ph, want, 2e-3, "CP correlation phase = -2 pi eps");
            /* feed phase/n to cfo_correct (the intended pairing) and compare with the transmitted subcarriers.
             * What remains is a common phase 2 pi eps cp/n (the symbol starts cp samples into the block). */
            for (int i = 0; i < n; i++) blk[i] = rx[cp + i];
            cfo_correct(blk, n, (float)(ph / n));
            fft(blk, zs, n);
            double cpe = 2.0 * PI_D * eps * cp / n;
            double e = 0, pk = 0;
            for (int k = 0; k < n; k++) { double d = cabs(cd(zs[k]) - Rd[k] * cexp(I * cpe)); if (d > e) e = d; if (cabs(Rd[k]) > pk) pk = cabs(Rd[k]); }
            CHECK(e < 5e-3 * pk, "eps=%+.2f: after cfo_correct(phase/n) every subcarrier equals the transmitted one up to the common phase (err %g, peak %g)", eps, e, pk);
            if (eps != 0.0) {
                /* the opposite sign must make it worse: rotating the wrong way doubles the offset */
                for (int i = 0; i < n; i++) blk[i] = rx[cp + i];
                cfo_correct(blk, n, (float)(-ph / n));
                fft(blk, zs, n);
                double e2 = 0; for (int k = 0; k < n; k++) { double d = cabs(cd(zs[k]) - Rd[k] * cexp(I * cpe)); if (d > e2) e2 = d; }
                CHECK(e2 > 10 * e, "sign convention: the CP phase is the NEGATIVE of the CFO phase; passing -phase/n doubles the offset (err %g vs %g)", e2, e);
            }
        }
        /* float phase accumulation inside cfo_correct over a full 2048-sample block (largest N the listings size for) */
        {
            int nn = 2048; double ps = 2.0 * PI_D * 0.45 / nn;
            for (int i = 0; i < nn; i++) { xs[i].re = 1.0f; xs[i].im = 0.0f; }
            cfo_correct(xs, nn, (float)ps);
            double worst = 0; for (int i = 0; i < nn; i++) { double a = ps * i, d = hypot(xs[i].re - cos(a), xs[i].im - sin(a)); if (d > worst) worst = d; }
            CHECK(worst < 1e-4, "cfo_correct over 2048 samples at 0.45 subcarrier: float phase accumulation error %g rad", worst);
        }
        /* the +-0.5 subcarrier ambiguity stated in the prose: eps = 0.7 is indistinguishable from eps = -0.3 */
        {
            double eps = 0.7;
            for (int i = 0; i < n + cp; i++) rx[i] = cf(cd(tx_cp[i]) * cexp(I * 2 * PI_D * eps * i / n));
            complex_t corr = cfo_estimate_from_cp(rx, n, cp);
            double est_eps = -atan2((double)corr.im, (double)corr.re) / (2 * PI_D);
            CHECK_NEAR(est_eps, -0.3, 2e-3, "eps = 0.7 aliases to the estimate -0.3 (true offset is 1.0 subcarrier larger)");
            /* eps = 1.0 (an integer number of subcarriers) is invisible to the CP correlator */
            for (int i = 0; i < n + cp; i++) rx[i] = cf(cd(tx_cp[i]) * cexp(I * 2 * PI_D * 1.0 * i / n));
            corr = cfo_estimate_from_cp(rx, n, cp);
            CHECK_NEAR(atan2((double)corr.im, (double)corr.re), 0.0, 2e-3, "integer-subcarrier offset gives zero CP phase (needs a pilot/preamble stage, as the prose says)");
        }
    }

    /* ===================== HB[88] PAPR and hard clipping ===================== */
    {
        int n = 64; uint8_t bits[16];
        memset(bits, 0, sizeof bits);                               /* every subcarrier carries the same QPSK point */
        ofdm_modulate_symbol(bits, n, scratch, ys);
        double p = papr_db(ys, n);
        CHECK_NEAR(p, 10.0 * log10((double)n), 1e-3, "all subcarriers in phase: PAPR = N exactly (prose: 10 log10 N)");
        CHECK(p > 18.0 && p < 18.1, "N=64 worst case is 18.06 dB (prose: 18.1 dB), got %.4f", p);

        n = 256;
        static uint8_t big[64];
        for (int b = 0; b < 64; b++) big[b] = (uint8_t)(lcg_next() >> 5);
        ofdm_modulate_symbol(big, n, scratch, ys);
        double p_before = papr_db(ys, n);
        double rms = 0; for (int i = 0; i < n; i++) rms += (double)ys[i].re * ys[i].re + (double)ys[i].im * ys[i].im; rms = sqrt(rms / n);
        float ceiling = (float)(1.5 * rms);
        for (int i = 0; i < n; i++) zs[i] = ys[i];
        ofdm_clip_papr(zs, n, ceiling);
        double p_after = papr_db(zs, n);
        int over = 0, changed_below = 0, clipped = 0; double worst_phase = 0;
        for (int i = 0; i < n; i++) {
            double m0 = hypot(ys[i].re, ys[i].im), m1 = hypot(zs[i].re, zs[i].im);
            if (m1 > ceiling * (1 + 1e-6)) over++;
            if (m0 <= ceiling) { if (zs[i].re != ys[i].re || zs[i].im != ys[i].im) changed_below++; }
            else { clipped++; double cross = (double)ys[i].re * zs[i].im - (double)ys[i].im * zs[i].re; double ph = fabs(cross) / (m0 * m1); if (ph > worst_phase) worst_phase = ph; }
        }
        CHECK(over == 0, "no clipped sample exceeds the ceiling (%d do)", over);
        CHECK(changed_below == 0, "samples already below the ceiling are untouched (%d changed)", changed_below);
        CHECK(clipped > 0, "test symbol actually exercises the clipper (%d of %d samples clipped)", clipped, n);
        CHECK(worst_phase < 1e-6, "clipping preserves phase (worst sin(angle) %g)", worst_phase);
        CHECK(p_after < p_before - 0.5, "clipping lowers PAPR: %.2f dB -> %.2f dB", p_before, p_after);
        /* in-band distortion: the FFT of the clipped symbol no longer equals the transmitted subcarriers */
        fft(zs, xs, n);
        for (int k = 0; k < n; k++) Xd[k] = cd(ys[k]);
        naive_dft(Xd, Rd, n, -1);
        double es = 0, rs = 0; for (int k = 0; k < n; k++) { double complex d = cd(xs[k]) - Rd[k]; es += creal(d) * creal(d) + cimag(d) * cimag(d); rs += creal(Rd[k]) * creal(Rd[k]) + cimag(Rd[k]) * cimag(Rd[k]); }
        double evm = 100.0 * sqrt(es / rs);
        printf("  [info] N=256 random QPSK symbol: PAPR %.2f dB -> %.2f dB after clipping at 1.5x rms (%d of %d samples clipped), in-band EVM %.1f %%\n", p_before, p_after, clipped, n, evm);
        CHECK(evm > 1.0 && evm < 60.0, "prose: clipping trades peak reduction for in-band distortion; measured %.1f %% EVM at a 1.5x-rms ceiling", evm);
        /* a ceiling above the peak must be a no-op */
        for (int i = 0; i < n; i++) zs[i] = ys[i];
        ofdm_clip_papr(zs, n, 1e9f);
        CHECK(memcmp(zs, ys, n * sizeof(complex_t)) == 0, "huge ceiling: unchanged");
    }

    /* ===================== HB[89] raised-cosine window ===================== */
    {
        int total = 80, W = 16;
        for (int i = 0; i < total; i++) { xs[i].re = 1.0f; xs[i].im = -2.0f; }
        ofdm_apply_window(xs, total, W);
        double e = 0; int mid_ok = 1;
        for (int i = 0; i < total; i++) {
            double w = 1.0;
            if (i < W) w = 0.5 * (1.0 - cos(PI_D * i / W));
            if (i >= total - W) w = 0.5 * (1.0 - cos(PI_D * (total - 1 - i) / W));
            double d = fabs(xs[i].re - w) + fabs(xs[i].im + 2 * w); if (d > e) e = d;
            if (i >= W && i < total - W && (xs[i].re != 1.0f || xs[i].im != -2.0f)) mid_ok = 0;
        }
        CHECK(e < 1e-5, "window == 0.5(1 - cos(pi i/W)) on the head and mirrored on the tail (max err %g)", e);
        CHECK(mid_ok, "samples between the two ramps are untouched");
        CHECK(xs[0].re == 0.0f && xs[total - 1].re == 0.0f, "first and last sample are zeroed (taper(0) = 0)");
        CHECK(xs[W - 1].re < 1.0f && xs[W].re == 1.0f, "ramp has not quite reached 1.0 at i = W-1 (%.6f)", xs[W - 1].re);
        /* overlap-add of two adjacent symbols whose W-sample ramps overlap sample for sample does not sum to exactly 1 */
        double worst = 0;
        for (int i = 0; i < W; i++) {
            double tail = 0.5 * (1.0 - cos(PI_D * i / W));                 /* gain on the tail sample total-1-i of symbol A */
            double head = 0.5 * (1.0 - cos(PI_D * (W - 1 - i) / W));       /* gain on the head sample (W-1-i) of symbol B that lands on the same instant */
            double dev = fabs(tail + head - 1.0); if (dev > worst) worst = dev;
        }
        printf("  [info] window overlap-add gain deviation from 1.0 (W=%d): max %.4f\n", W, worst);
        CHECK(worst > 0.01 && worst < 0.2, "overlapped ramps sum to 1 only within %.3f (ramp is sampled at i/W, not (i+1/2)/W)", worst);
    }
    TEST_DONE("test_D_ofdm");
}
