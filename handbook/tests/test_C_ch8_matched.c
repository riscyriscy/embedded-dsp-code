/* Chapter 8: matched filtering (HB[69]).  The two calls (tests/shims/hb_C_snippet_69.inc, verbatim) run
 * a Q15 FIR (the Chapter 2 fir_process_q15, FIR_TAPS = 19) as an RRC matched filter on I and Q.
 * The taps here are generated in the test from the closed-form RRC impulse response
 * (alpha = 0.35, 2 samples/symbol, 19 taps) and quantised to Q15 -- an independent reference.
 * Claims tested: the call computes the Q15 convolution (double reference), and TX-RRC followed by this RX-RRC is ISI-free at
 * the symbol instants (the cascade equals a raised cosine). */
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "hb_fir_q15.h"

#define SPS 2
#define NSYM 200
#define NS (NSYM * SPS + 64)
static const double ALPHA = 0.35;

static double rrc(double t) {                         /* root-raised-cosine impulse response, T = 1, unit-energy normalisation applied later */
    if (fabs(t) < 1e-9) return 1.0 - ALPHA + 4.0 * ALPHA / M_PI;
    if (fabs(fabs(t) - 1.0 / (4.0 * ALPHA)) < 1e-9)
        return ALPHA / sqrt(2.0) * ((1 + 2 / M_PI) * sin(M_PI / (4 * ALPHA)) + (1 - 2 / M_PI) * cos(M_PI / (4 * ALPHA)));
    return (sin(M_PI * t * (1 - ALPHA)) + 4 * ALPHA * t * cos(M_PI * t * (1 + ALPHA))) / (M_PI * t * (1 - (4 * ALPHA * t) * (4 * ALPHA * t)));
}
static double rc(double t) {                           /* raised cosine, T = 1 */
    double s = (fabs(t) < 1e-12) ? 1.0 : sin(M_PI * t) / (M_PI * t);
    double d = 1.0 - (2 * ALPHA * t) * (2 * ALPHA * t);
    if (fabs(d) < 1e-9) return M_PI / 4.0 * s;              /* removable singularity at t = 1/(2 alpha) */
    return s * cos(M_PI * ALPHA * t) / d;
}

int main(void) {
    int16_t taps[FIR_TAPS]; double tapd[FIR_TAPS], norm = 0;
    for (int i = 0; i < FIR_TAPS; i++) { tapd[i] = rrc((i - (FIR_TAPS - 1) / 2) / (double)SPS); norm += tapd[i] * tapd[i]; }
    for (int i = 0; i < FIR_TAPS; i++) { tapd[i] /= sqrt(norm); taps[i] = (int16_t)lrint(tapd[i] * 32768.0 * 0.999); }   /* unit-energy taps, Q15 */
    /* what the Q15 taps actually are, in double (the reference filters use these exact values) */
    double hq[FIR_TAPS]; for (int i = 0; i < FIR_TAPS; i++) hq[i] = taps[i] / 32768.0;

    fir_q15_t rrc_filter_i, rrc_filter_q;
    fir_init_q15(&rrc_filter_i, taps); fir_init_q15(&rrc_filter_q, taps);

    /* QPSK symbols +-1 -> zero-stuffed -> TX RRC (double) -> int16 samples at amplitude 6000 */
    lcg_state = 777u;
    int ai[NSYM], aq[NSYM];
    for (int k = 0; k < NSYM; k++) { ai[k] = ((lcg_next() >> 4) & 1) ? 1 : -1; aq[k] = ((lcg_next() >> 4) & 1) ? 1 : -1; }
    static int16_t xi[NS], xq[NS];
    for (int n = 0; n < NS; n++) {
        double si = 0, sq = 0;
        for (int k = 0; k < NSYM; k++) { int t = n - k * SPS; if (t >= 0 && t < FIR_TAPS) { si += ai[k] * hq[t]; sq += aq[k] * hq[t]; } }
        xi[n] = (int16_t)lrint(si * 9000.0); xq[n] = (int16_t)lrint(sq * 9000.0);
    }

    /* RX: the pair of calls, verbatim, once per sample */
    static int16_t yi[NS], yq[NS];
    for (int n = 0; n < NS; n++) {
        int16_t i_sample = xi[n], q_sample = xq[n];
#include "hb_C_snippet_69.inc"
        yi[n] = matched_filter_i; yq[n] = matched_filter_q;
    }

    /* (a) the calls compute the Q15 convolution: double reference on the same int16 input, same Q15 taps */
    int worst = 0;
    for (int n = 0; n < NS; n++) {
        double ri = 0, rq = 0;
        for (int t = 0; t < FIR_TAPS; t++) if (n - t >= 0) { ri += hq[t] * xi[n - t]; rq += hq[t] * xq[n - t]; }
        int di = abs(yi[n] - (int)floor(ri)), dq = abs(yq[n] - (int)floor(rq));
        if (di > worst) worst = di;
        if (dq > worst) worst = dq;
    }
    CHECK(worst <= 1, "Q15 FIR vs double convolution: worst error %d LSB (floor of an exact sum should match to 1 LSB)", worst);

    /* (b) cascade impulse response g = h * h (double, from the Q15 taps) against the analytic raised cosine at the symbol instants */
    double g0 = 0; for (int i = 0; i < FIR_TAPS; i++) g0 += hq[i] * hq[i];
    double worst_isi = 0;
    for (int j = 1; j <= 4; j++) {
        double gj = 0; for (int i = 0; i + j * SPS < FIR_TAPS; i++) gj += hq[i] * hq[i + j * SPS];
        double rel = fabs(gj) / g0; if (rel > worst_isi) worst_isi = rel;
        CHECK_NEAR(gj / g0, rc((double)j), 0.02, "cascade at +-j symbols vs analytic RC (ideal 0)");
    }
    printf("  [info] 19-tap Q15 RRC cascade: worst ISI term %.4f of the main lobe (%.1f dB); main lobe g0 = %.4f\n", worst_isi, 20 * log10(worst_isi), g0);
    CHECK(worst_isi < 0.02, "ISI at the symbol instants %.4f of the peak", worst_isi);

    /* (c) end to end: decisions at the symbol instants (even samples after TX+RX delay 2*9 = 18 samples) recover every symbol */
    int errs = 0, nchk = 0; double evm = 0;
    const int delay = FIR_TAPS - 1;                         /* 9 + 9 samples */
    for (int k = 12; k < NSYM - 12; k++) {
        int n = delay + k * SPS;
        double want = g0 * 9000.0;
        double di = yi[n] / want - ai[k], dq = yq[n] / want - aq[k];
        evm += di * di + dq * dq; nchk++;
        if ((yi[n] > 0) != (ai[k] > 0) || (yq[n] > 0) != (aq[k] > 0)) errs++;
    }
    evm = sqrt(evm / nchk / 2.0);
    printf("  [info] matched-filter output vs transmitted symbols: rms error %.4f of a symbol amplitude, %d decision errors in %d symbols\n", evm, errs, nchk);
    CHECK(errs == 0, "decision errors through TX-RRC / RX-RRC: %d", errs);
    CHECK(evm < 0.05, "rms symbol error %.4f too large for a clean RRC cascade", evm);
    TEST_DONE("test_C_ch8_matched");
}
