/* HB[37], HB[38], HB[39]: decimate_naive(), its call-site snippet, and polyphase_decimate()
 * (src/hb_decimate.c on the float fir_filter_t with FIR_TAPS = 32, src/hb_polyphase.c incl. polyphase_init()).
 * References: direct convolution in double / int64 (y[n] = sum h[k] x[n-k], kept at n = M-1, 2M-1, ...).
 * HB[38] is a snippet (statements, not a function): it is included below inside a harness function, which is why its status is syntax-only. */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "check.h"
#include "hb_fir_float.h"
#include "hb_decimate.h"
#include "hb_polyphase.h"

#define NIN 400
static int16_t xin[NIN];
static float hf[FIR_TAPS];      /* prototype, multiples of 1/256 (exact in float) */
static int32_t hq[FIR_TAPS];    /* the same prototype in Q15 integers: hf*32768 */

static void make_proto(int positive_only) {
    for (int k = 0; k < FIR_TAPS; k++) {
        int c = (int)(lcg_next() % 41) - (positive_only ? 0 : 20);   /* -20..20 or 0..40 */
        hf[k] = c / 256.0f; hq[k] = c * 128;
    }
}
static void make_input(int positive_only) { for (int n = 0; n < NIN; n++) xin[n] = (int16_t)((int)(lcg_next() % 2047) - (positive_only ? 0 : 1023)); }

/* ---- HB[38] harness: supplies the names the snippet uses ---- */
#define DECIM_FACTOR 4
static int run_snippet(fir_filter_t f, const int16_t *adc_samples, int num_input_samples, int32_t *output_buffer) {
#include "hb_decimate_callsite.inc"
    return out_count;
}

int main(void) {
    /* ---- decimate_naive against direct convolution, M = 1..8 ---- */
    for (int M = 1; M <= 8; M++) {
        make_proto(0); make_input(0);
        fir_filter_t f; fir_init(&f, hf);
        int count = 0, nout = 0, ok = 1; int32_t out = 0x7EADBEEF;
        for (int n = 0; n < NIN; n++) {
            int32_t before = out;
            bool got = decimate_naive(&f, xin[n], M, &count, &out);
            double y = 0; for (int k = 0; k < FIR_TAPS; k++) if (n - k >= 0) y += (double)hf[k] * xin[n - k];
            bool want_kept = ((n + 1) % M) == 0;
            if (got != want_kept) ok = 0;
            if (got) { nout++; if (out != (int32_t)y) { ok = 0; printf("  M=%d n=%d: got %d want %d\n", M, n, out, (int32_t)y); } }
            else if (out != before) { ok = 0; printf("  *output written on a 'false' call (n=%d)\n", n); }
        }
        CHECK(ok, "decimate_naive M=%d: kept samples and values", M);
        CHECK(nout == NIN / M, "decimate_naive M=%d: %d outputs, want %d", M, nout, NIN / M);
        CHECK(count == NIN, "sample_count counts every input (%d)", count);
    }
    /* the first kept output arrives on the M-th input, not the first (counter starts at 0, incremented before the test) */
    { fir_filter_t f; fir_init(&f, hf); int c = 0; int32_t o = 0;
      CHECK(!decimate_naive(&f, 1, 3, &c, &o) && !decimate_naive(&f, 1, 3, &c, &o) && decimate_naive(&f, 1, 3, &c, &o), "M=3: false, false, true"); }

    /* ---- HB[38] snippet (syntax-only listing, exercised in a harness) ---- */
    { make_proto(0); make_input(0); static int32_t outbuf[NIN]; fir_filter_t f; fir_init(&f, hf);
      int n_out = run_snippet(f, xin, NIN, outbuf);
      CHECK(n_out == NIN / DECIM_FACTOR, "call-site snippet yields %d outputs, want %d", n_out, NIN / DECIM_FACTOR);
      int ok = 1;
      for (int m = 0; m < n_out; m++) { int n = m * DECIM_FACTOR + DECIM_FACTOR - 1; double y = 0; for (int k = 0; k < FIR_TAPS; k++) if (n - k >= 0) y += (double)hf[k] * xin[n - k]; if (outbuf[m] != (int32_t)y) ok = 0; }
      CHECK(ok, "call-site snippet output equals the direct-convolution reference"); }

    /* ---- polyphase: structure ---- */
    CHECK(POLY_M * POLY_TAPS_PER_PHASE == FIR_TAPS, "N = POLY_M * taps_per_phase = %d matches the 32-tap prototype used here", POLY_M * POLY_TAPS_PER_PHASE);
    { polyphase_decim_t p; memset(&p, 0, sizeof p); int got; int ok = 1;
      for (int n = 0; n < 40; n++) { int32_t r = polyphase_decimate(&p, 1000, &got); int want = ((n + 1) % POLY_M) == 0; if (got != want || (!got && r != 0)) ok = 0; }
      CHECK(ok, "output flag is set once every POLY_M calls (the Mth), 0 otherwise"); }

    /* ---- polyphase: the OUTPUT against direct convolution (y[n] = sum h[k] x[n-k], kept at n = M-1, 2M-1, ...) ---- */
    for (int trial = 0; trial < 5; trial++) {
        make_proto(0); make_input(0);
        polyphase_decim_t p; polyphase_init(&p, hq);
        int got, nout = 0, ok = 1;
        for (int n = 0; n < NIN; n++) {
            int32_t r = polyphase_decimate(&p, xin[n], &got);
            if (!got) continue;
            int64_t acc = 0; for (int k = 0; k < FIR_TAPS; k++) if (n - k >= 0) acc += (int64_t)hq[k] * xin[n - k];
            int32_t want = (int32_t)(acc >> 15);
            if (r != want) { ok = 0; printf("  trial %d n=%d: got %d want %d\n", trial, n, r, want); }
            nout++;
        }
        CHECK(ok && nout == NIN / POLY_M, "polyphase_init + polyphase_decimate == direct FIR, decimated (trial %d, %d outputs)", trial, nout);
    }
    /* ---- "same output as the naive filter-then-drop" (the prose's claim), positive data so floor == truncation ---- */
    { make_proto(1); make_input(1);
      fir_filter_t f; fir_init(&f, hf); polyphase_decim_t p; polyphase_init(&p, hq);
      int c = 0, got, ok = 1, nout = 0; int32_t o_naive = 0;
      for (int n = 0; n < NIN; n++) {
          bool kn = decimate_naive(&f, xin[n], POLY_M, &c, &o_naive);
          int32_t o_poly = polyphase_decimate(&p, xin[n], &got);
          if (kn != (bool)got) ok = 0;
          if (kn) { nout++; if (o_naive != o_poly) { ok = 0; printf("  n=%d naive %d poly %d\n", n, o_naive, o_poly); } }
      }
      CHECK(ok && nout == NIN / POLY_M, "polyphase output is identical to decimate_naive's (%d outputs)", nout); }

    /* ---- polyphase_init: phase p holds h[M-1-p], h[M-1-p+M], ...; it also clears the state, so a used filter can be reloaded ---- */
    { make_proto(0); make_input(0);
      polyphase_decim_t p; polyphase_init(&p, hq);
      int ok = 1;
      for (int ph = 0; ph < POLY_M; ph++) for (int i = 0; i < POLY_TAPS_PER_PHASE; i++) if (p.coeffs[ph][i] != hq[(POLY_M - 1 - ph) + i * POLY_M]) ok = 0;
      CHECK(ok, "phase p holds h[M-1-p + i*M]");
      int got; for (int n = 0; n < 37; n++) polyphase_decimate(&p, xin[n], &got);     /* dirty state, mid-cycle phase */
      polyphase_init(&p, hq);
      polyphase_decim_t q; polyphase_init(&q, hq); int g2, bad = 0, nout = 0;
      for (int n = 0; n < NIN; n++) { int32_t r1 = polyphase_decimate(&p, xin[n], &got), r2 = polyphase_decimate(&q, xin[n], &g2); if (got != g2 || r1 != r2) bad++; nout += got; }
      CHECK(bad == 0 && nout == NIN / POLY_M, "re-initialised filter behaves like a fresh one (%d mismatches, %d outputs)", bad, nout); }
    /* ---- the same prototype dealt out as h[p + i*M] (phase p starting at tap p) does NOT reproduce the FIR: the commutator emits right after
     * phase M-1 is filled, so tap order matters even for a symmetric (linear-phase) prototype ---- */
    { for (int k = 0; k < FIR_TAPS / 2; k++) { int c = (int)(lcg_next() % 41); hq[k] = hq[FIR_TAPS - 1 - k] = c * 128; }
      make_input(1);
      polyphase_decim_t p; memset(&p, 0, sizeof p);
      for (int ph = 0; ph < POLY_M; ph++) for (int i = 0; i < POLY_TAPS_PER_PHASE; i++) p.coeffs[ph][i] = hq[ph + i * POLY_M];
      int got, bad = 0, nout = 0;
      for (int n = 0; n < NIN; n++) {
          int32_t r = polyphase_decimate(&p, xin[n], &got); if (!got) continue;
          int64_t acc = 0; for (int k = 0; k < FIR_TAPS; k++) if (n - k >= 0) acc += (int64_t)hq[k] * xin[n - k];
          if (r != (int32_t)(acc >> 15)) bad++;
          nout++;
      }
      CHECK(bad > nout / 2, "tap p + i*M in phase p is the wrong split: %d of %d outputs differ from the FIR", bad, nout); }
    TEST_DONE("test_B_decimate");
}
