/* Chapter 11: 16-QAM Gray slicer (HB[75]).  Reference: the mapping the listing's comment and figure caption state
 * ({-3,-1,+1,+3} -> 00,01,11,10 per axis; symbol = I bits then Q bits), written independently as a table in the test. */
#include <stdint.h>
#include "check.h"
#include "hb_qam16.h"

static int popcount4(int v) { int c = 0; for (int i = 0; i < 4; i++) c += (v >> i) & 1; return c; }

int main(void) {
    /* independent table: level index 0..3 = -3,-1,+1,+3 (in units of u) -> Gray code */
    static const int gray[4] = { 0x0, 0x1, 0x3, 0x2 };
    static const int level[4] = { -3, -1, 1, 3 };
    const int units[] = { 1, 2, 7, 500, 4000, 10922 };               /* u; level_spacing = 2u; 3u must fit the int32 comparison trivially */
    for (unsigned ui = 0; ui < sizeof units / sizeof units[0]; ui++) {
        const int u = units[ui], ls = 2 * u;
        for (int li = 0; li < 4; li++) for (int lq = 0; lq < 4; lq++) {
            int want = (gray[li] << 2) | gray[lq];
            CHECK(qam16_demodulate_symbol(level[li] * u, level[lq] * u, ls) == want, "u=%d point (%d,%d)u: got %d want %d", u, level[li], level[lq], qam16_demodulate_symbol(level[li] * u, level[lq] * u, ls), want);
            CHECK(qam16_axis_decision(level[li] * u, ls) == gray[li], "axis u=%d level %d", u, level[li]);
        }
    }
    /* noise inside the decision region never changes the decision: +-0.99u around every point (u large enough that 0.99u is distinct) */
    lcg_state = 8u; int bad = 0;
    for (int trial = 0; trial < 20000; trial++) {
        const int u = 4000, ls = 2 * u; int li = (int)(lcg_next() & 3), lq = (int)(lcg_next() & 3);
        int ni = (int)(lcg_next() % 7921) - 3960, nq = (int)(lcg_next() % 7921) - 3960;        /* +-0.99 u */
        if (qam16_demodulate_symbol(level[li] * u + ni, level[lq] * u + nq, ls) != ((gray[li] << 2) | gray[lq])) bad++;
    }
    CHECK(bad == 0, "%d noisy points decoded wrongly inside the decision region", bad);
    /* Gray property: horizontally or vertically adjacent constellation points differ in exactly one bit; diagonal neighbours in two */
    for (int li = 0; li < 4; li++) for (int lq = 0; lq < 4; lq++) {
        int s = qam16_demodulate_symbol(level[li] * 100, level[lq] * 100, 200);
        if (li < 3) CHECK(popcount4(s ^ qam16_demodulate_symbol(level[li + 1] * 100, level[lq] * 100, 200)) == 1, "I-neighbour of (%d,%d)", li, lq);
        if (lq < 3) CHECK(popcount4(s ^ qam16_demodulate_symbol(level[li] * 100, level[lq + 1] * 100, 200)) == 1, "Q-neighbour of (%d,%d)", li, lq);
    }
    CHECK(qam16_demodulate_symbol(-3, -3, 2) == 0x0 && qam16_demodulate_symbol(3, 3, 2) == 0xA && qam16_demodulate_symbol(1, -1, 2) == 0xD, "spot values (16-QAM figure): (-3,-3)->0000, (+3,+3)->1010, (+1,-1)->1101");
    /* decision boundaries: strictly above +ls is the +3 level, above 0 is +1, above -ls is -1 (ties go DOWN: val == ls -> +1, 0 -> -1, -ls -> -3) */
    CHECK(qam16_axis_decision(201, 200) == 0x2 && qam16_axis_decision(200, 200) == 0x3, "boundary at +ls");
    CHECK(qam16_axis_decision(1, 200) == 0x3 && qam16_axis_decision(0, 200) == 0x1, "boundary at 0 (tie goes to the -1 level, a one-LSB asymmetry)");
    CHECK(qam16_axis_decision(-199, 200) == 0x1 && qam16_axis_decision(-200, 200) == 0x0, "boundary at -ls");
    TEST_DONE("test_C_ch8_qam16");
}
