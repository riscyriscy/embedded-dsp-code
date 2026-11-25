/* Behavioral tests for the Chapter 2-4 filters: FIR, symmetric FIR, biquad, DC blocker,
 * one-pole LPF, notch, Goertzel, CIC, PDM, zero-stuff interpolation. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "check.h"
#include "fm_qformat.h"
#include "fm_fir.h"
#include "fm_biquad.h"
#include "fm_simple_filters.h"
#include "fm_goertzel.h"
#include "fm_cic.h"
#include "fm_adc.h"

static void test_fir(void) {
    fir_q15_t f; fir_init_q15(&f, fir_coeffs_q15);
    /* Impulse response == coefficients (to within one LSB of Q15 rounding for a 32767 impulse). */
    for (int n = 0; n < FIR_TAPS + 4; n++) {
        int16_t y = fir_process_q15(&f, n == 0 ? 32767 : 0);
        int want = (n < FIR_TAPS) ? fir_coeffs_q15[n] : 0;
        CHECK(abs(y - want) <= 1, "fir impulse[%d]: got %d want %d", n, y, want);
    }
    /* Symmetric implementation must match the direct one exactly on a symmetric coefficient set. */
    fir_q15_t a, b; fir_init_q15(&a, fir_coeffs_q15); fir_init_q15(&b, fir_coeffs_q15);
    int mism = 0;
    for (int n = 0; n < 2000; n++) {
        int16_t x = (int16_t)((int)(lcg_next() & 0xFFFF) - 32768);
        if (fir_process_q15(&a, x) != fir_process_symmetric_q15(&b, x)) mism++;
    }
    CHECK(mism == 0, "symmetric FIR differs from direct FIR on %d of 2000 samples", mism);
    /* Saturation: all-max input must clip, not wrap. DC gain of this filter is sum(c)/32768. */
    fir_q15_t s; fir_init_q15(&s, fir_coeffs_q15);
    int32_t csum = 0; for (int i = 0; i < FIR_TAPS; i++) csum += fir_coeffs_q15[i];
    int16_t y = 0; for (int n = 0; n < 4 * FIR_TAPS; n++) y = fir_process_q15(&s, 32767);
    double dc_expect = (double)csum * 32767.0 / 32768.0;
    if (dc_expect > 32767.0) dc_expect = 32767.0;   /* this coefficient set has DC gain slightly > 1: must clip */
    CHECK_NEAR(y, dc_expect, 2.0, "fir DC gain at full scale");
    CHECK(y > 0, "fir full-scale DC output went non-positive (wrapped?)");
}

static void test_fir_golden_and_qformat(void) {
    /* The book's golden vector for fir_process_q15 (Chapter 2): impulse of 16384 through the 19-tap filter
     * gives exactly these 19 values, then exact zeros. */
    static const int16_t want[19] = { -106, 3, 280, 339, -237, -988, -590, 1709, 4749, 6170, 4749, 1709, -590, -988, -237, 339, 280, 3, -106 };
    fir_q15_t f; fir_init_q15(&f, fir_coeffs_q15);
    for (int n = 0; n < 24; n++) {
        int16_t y = fir_process_q15(&f, n == 0 ? 16384 : 0);
        CHECK(y == (n < 19 ? want[n] : 0), "FIR golden vector[%d]: got %d want %d", n, y, n < 19 ? want[n] : 0);
    }
    /* Q-format helpers (Chapter 1). */
    CHECK(FLOAT_TO_Q15(0.5f) == 16384 && FLOAT_TO_Q15(-1.0f) == -32768, "FLOAT_TO_Q15 landmarks");
    CHECK(FLOAT_TO_Q15(1.0f) == 32767 && FLOAT_TO_Q15(5.0f) == 32767 && FLOAT_TO_Q15(-5.0f) == -32768, "FLOAT_TO_Q15 clamps (1.0 would be 32768)");
    CHECK(FLOAT_TO_Q15(0.0731f) == 2395, "FLOAT_TO_Q15(0.0731) = %d", FLOAT_TO_Q15(0.0731f));
    CHECK_NEAR(Q15_TO_FLOAT(16384), 0.5, 1e-9, "Q15_TO_FLOAT(16384)");
    CHECK(q15_mul_trunc(16384, 16384) == 8192 && q15_mul_trunc(-16384, 16384) == -8192, "q15_mul_trunc 0.5*0.5");
    CHECK(q15_mul_trunc(-1, 1) == -1, "q15_mul_trunc floors toward -inf (the truncation bias the book warns about)");
    CHECK(q15_wide_add_sat(INT32_MAX, 1) == INT32_MAX && q15_wide_add_sat(INT32_MIN, -1) == INT32_MIN && q15_wide_add_sat(100000, 100000) == 200000, "q15_wide_add_sat");
    CHECK(q15_add_sat(32767, 1) == 32767 && q15_add_sat(-32768, -1) == -32768 && q15_add_sat(1000, 2000) == 3000, "q15_add_sat");
}

static void test_biquad(void) {
    /* Book example: Q2.14 Butterworth section, 0.5 impulse; compare against float DF2T reference
     * built from the same quantized coefficients. */
    biquad_q15_t bq = { .b0 = 329, .b1 = 658, .b2 = 329, .a1 = -25576, .a2 = 10508 };
    double b0 = 329 / 16384.0, b1 = 658 / 16384.0, b2 = 329 / 16384.0, a1 = -25576 / 16384.0, a2 = 10508 / 16384.0;
    double z1 = 0, z2 = 0; int worst = 0;
    for (int n = 0; n < 400; n++) {
        double x = (n == 0) ? 0.5 * 32768.0 : 0.0;
        int16_t yq = biquad_process_q15(&bq, (int16_t)x);
        double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y;
        int d = abs((int)lrint(y) - yq); if (d > worst) worst = d;
    }
    CHECK(worst <= 3, "biquad vs float reference: worst error %d LSB", worst);
    /* Rounded (not truncated) feedback: DC step should settle at the float answer, not ~3% low
     * (the book's own warning in the Q2.14 section). DC gain = (b0+b1+b2)/(1+a1+a2). */
    biquad_q15_t st = bq; st.z1 = st.z2 = 0;
    int16_t y = 0; for (int n = 0; n < 20000; n++) y = biquad_process_q15(&st, 8000);
    double dcg = (b0 + b1 + b2) / (1 + a1 + a2);
    CHECK_NEAR(y, 8000 * dcg, 8000 * dcg * 0.01 + 2, "biquad DC step steady state (within 1%)");
}

static void test_adc_and_dc(void) {
    CHECK(adc_to_q15(0) == -32768 && adc_to_q15(2048) == 0 && adc_to_q15(4095) == 32752 && adc_to_q15(2049) == 16, "adc_to_q15 landmarks");
    int mono = 1; for (int r = 1; r < 4096; r++) if (adc_to_q15((uint16_t)r) <= adc_to_q15((uint16_t)(r - 1))) mono = 0;
    CHECK(mono, "adc_to_q15 is strictly monotonic over all 4096 codes");
    /* DC of either sign decays to exactly 0 (R = 0.995). */
    int32_t pos = 0, neg = 0; dc_blocker_t p = { .r_q15 = 32604 }, n = { .r_q15 = 32604 };
    for (int i = 0; i < 100000; i++) { pos = dc_blocker_process(&p, 5000); neg = dc_blocker_process(&n, -5000); }
    CHECK(pos == 0 && neg == 0, "DC blocker residuals after 100000 samples: +5000 -> %d, -5000 -> %d", pos, neg);
    /* A zero-mean input stays zero-mean (the mean of an alternating +-1000 input is 0 at the output). */
    dc_blocker_t z = { .r_q15 = 32604 }; double sz = 0; int N = 100000;
    for (int i = 0; i < N; i++) { int32_t x = (i & 1) ? 1000 : -1000; int32_t y = dc_blocker_process(&z, x); if (i > N / 2) sz += y; }
    CHECK(fabs(sz / (N / 2)) < 0.1, "DC blocker mean on zero-mean input %.3f", sz / (N / 2));
    /* Against the double-precision recursion y[n] = x[n] - x[n-1] + R*y[n-1] on an input with a negative offset:
     * the integer output stays within 1 count of it. */
    dc_blocker_t r = { .r_q15 = 32604 }; double Rd = 32604.0 / 32768.0, yd = 0, xp = 0, worst = 0; lcg_state = 777u;
    for (int i = 0; i < 20000; i++) {
        double xd = -3000.0 + 800.0 * sin(0.05 * i) + (double)((int)(lcg_next() & 255) - 128);
        int32_t x = (int32_t)lrint(xd); xd = x;
        yd = xd - xp + Rd * yd; xp = xd;
        double e = fabs(dc_blocker_process(&r, x) - yd); if (e > worst) worst = e;
    }
    CHECK(worst <= 1.0, "DC blocker vs double-precision reference: worst error %.3f counts", worst);
}

static void test_simple_filters(void) {
    dc_blocker_t dc = { .r_q15 = 32604 };
    int32_t y = 0; for (int n = 0; n < 20000; n++) y = dc_blocker_process(&dc, 5000);
    CHECK(abs(y) <= 40, "DC blocker: residual %d after 20000 samples of DC 5000", y);
    one_pole_lpf_t lp = { .alpha_q15 = 25956 };
    for (int n = 0; n < 200; n++) y = one_pole_lpf_process(&lp, 1000);
    CHECK(abs(y - 1000) <= 2, "one-pole LPF step: got %d", y);

    /* 60 Hz notch at fs = 1 kHz, Q = 10: 60 Hz tone must drop by >= 20 dB, 120 Hz must pass. */
    biquad_q15_t nb; design_notch_biquad(&nb, 60.0f, 1000.0f, 10.0f);
    double p60 = 0, p120 = 0, pin = 0; biquad_q15_t n2 = nb;
    for (int n = 0; n < 6000; n++) {
        int16_t x = (int16_t)lrint(10000.0 * sin(2 * M_PI * 60.0 * n / 1000.0));
        int16_t o = biquad_process_q15(&nb, x);
        if (n >= 3000) { p60 += (double)o * o; pin += (double)x * x; }
    }
    for (int n = 0; n < 6000; n++) {
        int16_t x = (int16_t)lrint(10000.0 * sin(2 * M_PI * 120.0 * n / 1000.0));
        int16_t o = biquad_process_q15(&n2, x);
        if (n >= 3000) p120 += (double)o * o;
    }
    double atten60 = 10 * log10(p60 / pin);
    double gain120 = 10 * log10(p120 / pin);
    CHECK(atten60 < -20.0, "notch: 60 Hz only attenuated %.1f dB", atten60);
    CHECK(gain120 > -1.0, "notch: 120 Hz passband gain %.2f dB", gain120);
    CHECK(q15_mul_round(16384, 16384) == 8192, "q15_mul_round(0.5,0.5)");
    CHECK(q15_mul_round(-1, 16384) == 0 || q15_mul_round(-1, 16384) == -1, "q15_mul_round tiny negative");
}

static void test_goertzel(void) {
    enum { N = 105, KBIN = 18 };   /* DTMF-style block length from the book's example */
    float x[N]; double dre = 0, dim = 0;
    for (int n = 0; n < N; n++) { x[n] = (float)(0.7 * sin(2 * M_PI * KBIN * n / N + 0.3) + 0.2 * sin(2 * M_PI * 5 * n / N));
        double a = -2.0 * M_PI * KBIN * n / N; dre += x[n] * cos(a); dim += x[n] * sin(a); }
    goertzel_t g; goertzel_init(&g, KBIN, N);
    for (int n = 0; n < N; n++) goertzel_process_sample(&g, x[n]);
    float re, im; goertzel_result(&g, &re, &im);
    CHECK_NEAR(re, dre, 1e-3, "goertzel re"); CHECK_NEAR(im, dim, 1e-3, "goertzel im");
    double dp = dre * dre + dim * dim;
    CHECK_NEAR(goertzel_power(&g), dp, 1e-3 * dp, "goertzel_power");
}

/* Reference for an order-3 CIC: boxcar(M) convolved with itself three times, sampled every M inputs (exact, including the
 * start-up transient, because the integrators/combs start at zero). */
static void cic_reference(const int32_t *x, int n_in, int M, int64_t *y, int *n_out) {
    int hlen = 3 * M - 2; int64_t *h = calloc((size_t)hlen, sizeof *h), *t = calloc((size_t)hlen, sizeof *t);
    h[0] = 1; int cur = 1;
    for (int pass = 0; pass < 3; pass++) {            /* convolve with boxcar(M) three times */
        memset(t, 0, (size_t)hlen * sizeof *t);
        for (int i = 0; i < cur; i++) for (int k = 0; k < M; k++) if (i + k < hlen) t[i + k] += h[i];
        cur += M - 1; memcpy(h, t, (size_t)hlen * sizeof *h);
    }
    *n_out = 0;
    for (int n = M - 1; n < n_in; n += M) {
        int64_t acc = 0; for (int i = 0; i < hlen && i <= n; i++) acc += h[i] * x[n - i];
        y[(*n_out)++] = acc;
    }
    free(h); free(t);
}

static void test_cic(void) {
    /* Bit-exact against brute force, at several decimation ratios. Amplitudes are chosen so M^3 * amp fits the int32
     * the function narrows its output to (M = 64 needs |x| <= 8191). */
    const int Ms[] = { 4, 8, 16, 64 }; const int amps[] = { 32767, 32767, 32767, 8000 };
    for (int mi = 0; mi < 4; mi++) {
        int M = Ms[mi], N = 40 * M; int32_t *x = malloc(N * sizeof *x); int64_t *ref = malloc((size_t)(N / M + 1) * sizeof *ref); int nref;
        for (int n = 0; n < N; n++) x[n] = (int32_t)((lcg_next() % (2u * amps[mi] + 1)) - (uint32_t)amps[mi]);
        cic_reference(x, N, M, ref, &nref);
        cic_decimator_t d = { 0 }; int got = 0, bad = 0; int32_t out;
        for (int n = 0; n < N; n++) if (cic_decimator_process(&d, x[n], M, &out)) { if (got >= nref || out != (int32_t)ref[got]) bad++; got++; }
        CHECK(got == nref && got == N / M, "CIC M=%d: %d outputs, want %d", M, got, nref);
        CHECK(bad == 0, "CIC M=%d: %d of %d outputs differ from the brute-force boxcar^3 reference", M, bad, got);
        free(x); free(ref);
    }
    /* DC gain M^3 and the bounded decimation counter. */
    enum { M = 8 }; cic_decimator_t d = { 0 }; int32_t out = 0, last = 0;
    for (int n = 0; n < 400; n++) if (cic_decimator_process(&d, 1000, M, &out)) last = out;
    CHECK(last == 1000 * M * M * M, "CIC DC gain: got %d want %d (M^3 * input)", last, 1000 * M * M * M);
    CHECK(d.decim_counter < (uint32_t)M, "decim_counter escaped [0,M): %u", d.decim_counter);
    /* The 34-bit claim: full-scale 16-bit input at M = 64 drives the 64-bit integrators past 2^31 but not the output
     * the book narrows to int32 only when it fits: check the integrator itself holds the exact value. */
    cic_decimator_t big = { 0 }; int32_t o2;
    for (int n = 0; n < 64 * 8; n++) cic_decimator_process(&big, 32767, 64, &o2);
    CHECK(big.integ[2].accum > (uint64_t)INT32_MAX, "third integrator should exceed int32 range at M=64 full scale (34-bit claim); got %lld", (long long)big.integ[2].accum);
    /* A DC input that runs the first integrator past 2^32 (1000 * 5,120,000 = 5.12e9). Every integrator must hold the exact
     * running sum and every output must stay M^3 * input. A stage that narrowed its input to int32 left the 64-bit results
     * wrong from input sample 2112 at M = 64 (the int32 output hid it, because the error is a multiple of 2^32). */
    {
        enum { MM = 64, AMP = 1000 }; const uint64_t n_in = 80000u * MM; cic_decimator_t w = { 0 }; int32_t o3 = 0; uint64_t wrong = 0;
        for (uint64_t n = 1; n <= n_in; n++) if (cic_decimator_process(&w, AMP, MM, &o3) && n > 3u * MM && o3 != AMP * MM * MM * MM) wrong++;
        CHECK(wrong == 0, "CIC DC input past 2^32: %llu outputs differ from M^3 * input", (unsigned long long)wrong);
        CHECK(w.integ[0].accum == (uint64_t)AMP * n_in, "CIC integrator 1 holds %llu, want %llu", (unsigned long long)w.integ[0].accum, (unsigned long long)AMP * n_in);
        CHECK(w.integ[1].accum == (uint64_t)AMP * (n_in * (n_in + 1) / 2), "CIC integrator 2 holds %llu, want %llu", (unsigned long long)w.integ[1].accum, (unsigned long long)AMP * (n_in * (n_in + 1) / 2));
    }
    /* PDM: all-ones bitstream is a +1 DC input; alternating bits average to zero. */
    cic_decimator_t p = { 0 }; bool rdy; int32_t pcm = 0, plast = 0;
    for (int n = 0; n < 400; n++) { pdm_cic_feed_bit(&p, 1, M, &rdy, &pcm); if (rdy) plast = pcm; }
    CHECK(plast == M * M * M, "PDM all-ones -> %d, want %d", plast, M * M * M);
    cic_decimator_t q = { 0 }; int nz = 0;
    for (int n = 0; n < 800; n++) { pdm_cic_feed_bit(&q, n & 1, M, &rdy, &pcm); if (rdy && n > 100 && pcm != 0) nz++; }
    CHECK(nz == 0, "PDM 0101... must average to 0 after the transient (%d non-zero outputs)", nz);
}

/* interpolate_by_L against an independent reference: zero-stuff, FIR with the same rounding/saturation, times L, saturate. */
static void test_interpolate(void) {
    const int Ls[] = { 2, 4, 8 };
    for (int li = 0; li < 3; li++) {
        int L = Ls[li]; fir_q15_t f; fir_init_q15(&f, fir_coeffs_q15);
        int N = 300; int16_t *s = calloc((size_t)N * L, sizeof *s), *y = malloc((size_t)N * L * sizeof *y); int mism = 0;
        for (int n = 0; n < N; n++) { int16_t in = (int16_t)((int)(lcg_next() & 0xFFFF) - 32768); s[n * L] = in; interpolate_by_L(&f, in, L, &y[n * L]); }
        for (int n = 0; n < N * L; n++) {
            long long acc = 0; for (int i = 0; i < FIR_TAPS && i <= n; i++) acc += (long long)fir_coeffs_q15[i] * s[n - i];
            long long q = (acc + (1LL << 14)) >> 15; if (q > 32767) q = 32767; if (q < -32768) q = -32768;
            q *= L; if (q > 32767) q = 32767; if (q < -32768) q = -32768;
            if (y[n] != (int16_t)q) mism++;
        }
        CHECK(mism == 0, "interpolate_by_L L=%d: %d of %d outputs differ from the zero-stuff reference", L, mism, N * L);
        free(s); free(y);
    }
}

int main(void) {
    test_fir(); test_fir_golden_and_qformat(); test_biquad(); test_simple_filters(); test_adc_and_dc(); test_goertzel(); test_cic(); test_interpolate();
    TEST_DONE("test_filters");
}
