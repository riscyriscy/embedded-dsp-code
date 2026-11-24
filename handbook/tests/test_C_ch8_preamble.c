/* Chapter 11: sliding-correlator preamble detection (HB[68]).
 * Reference: a naive double-precision correlation written independently in the test.  known_preamble[] is given in
 * transmission order: known_preamble[0] meets the OLDEST of the last PREAMBLE_LEN samples, so the output peaks on the
 * preamble's last sample. */
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "hb_preamble.h"

#define STREAM 300
#define START  100                      /* preamble occupies samples START .. START+PREAMBLE_LEN-1 */

int main(void) {
    int32_t p[PREAMBLE_LEN], prev[PREAMBLE_LEN];            /* template (transmission order) and its time reverse; Q16 +-1.0 */
    lcg_state = 4242u;
    for (int i = 0; i < PREAMBLE_LEN; i++) p[i] = ((lcg_next() >> 3) & 1) ? 65536 : -65536;
    for (int i = 0; i < PREAMBLE_LEN; i++) prev[i] = p[PREAMBLE_LEN - 1 - i];
    int palindrome = 1; for (int i = 0; i < PREAMBLE_LEN; i++) if (p[i] != prev[i]) palindrome = 0;
    CHECK(!palindrome, "test fixture: the random preamble must not be a palindrome");

    int32_t x[STREAM];                                       /* +-1000-amplitude preamble in +-120 uniform noise */
    for (int n = 0; n < STREAM; n++) x[n] = (int32_t)(lcg_next() % 241) - 120;
    for (int i = 0; i < PREAMBLE_LEN; i++) x[START + i] += (p[i] > 0) ? 1000 : -1000;
    const int end = START + PREAMBLE_LEN - 1;                /* index of the last preamble sample */

    /* independent reference (double): r_corr[n] = sum_i p[i] x[n-N+1+i]  (/ 2^16) */
    static double r_corr[STREAM];
    for (int n = 0; n < STREAM; n++) {
        double b = 0;
        for (int i = 0; i < PREAMBLE_LEN; i++) if (n - PREAMBLE_LEN + 1 + i >= 0) b += (double)p[i] * x[n - PREAMBLE_LEN + 1 + i];
        r_corr[n] = b / 65536.0;
    }
    CHECK_NEAR(r_corr[end], 32000.0, 1200.0, "reference: correlation peak is N*1000 +- 3 sigma of the noise sum (sigma = 69.3*sqrt(32) = 392)");

    /* 1. the transmission-order template peaks at the preamble's last sample and equals the reference correlation */
    preamble_corr_t c; memset(&c, 0, sizeof c);
    int best_n = -1; int32_t best = INT32_MIN, second = INT32_MIN; int bad = 0;
    for (int n = 0; n < STREAM; n++) {
        int32_t y = preamble_correlate(&c, p, x[n]);
        if (n >= PREAMBLE_LEN - 1 && y != (int32_t)floor(r_corr[n]) && y != (int32_t)ceil(r_corr[n])) bad++;
        if (y > best) { second = best; best = y; best_n = n; } else if (y > second) second = y;
    }
    CHECK(bad == 0, "correlator deviates from the correlation reference at %d samples", bad);
    CHECK(best_n == end, "correlator: peak at %d, preamble ends at %d", best_n, end);
    CHECK(best > 28000 && best < 36000, "correlator peak %d, want ~32000", best);
    CHECK(second < best / 3, "correlator: best sidelobe %d vs peak %d (detection margin)", second, best);

    /* 2. a time-reversed template is a different, non-matching filter: no peak at the preamble end */
    preamble_corr_t cr; memset(&cr, 0, sizeof cr); int32_t at_end = 0;
    for (int n = 0; n < STREAM; n++) { int32_t y = preamble_correlate(&cr, prev, x[n]); if (n == end) at_end = y; }
    CHECK(at_end < 16000, "reversed template: value at the preamble end is %d (a matched template gives ~32000)", at_end);

    /* 3. detection by threshold: at half the peak the correlator fires exactly once, at the right place */
    preamble_corr_t c4; memset(&c4, 0, sizeof c4); int fires = 0, first = -1;
    for (int n = 0; n < STREAM; n++) if (preamble_correlate(&c4, p, x[n]) > 16000) { fires++; if (first < 0) first = n; }
    CHECK(fires == 1 && first == end, "threshold detection: %d firings, first at %d (want 1 at %d)", fires, first, end);
    TEST_DONE("test_C_ch8_preamble");
}
