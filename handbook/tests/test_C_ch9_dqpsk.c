/* Chapter 9: differential QPSK decoder (HB[82]).
 * References: nearest-of-four decision on the phase difference in double precision; a transmitter written in the test that
 * accumulates the phase (the "0 -> +90 -> 180 -> -90" Gray ring: 00, 10, 11, 01).  Claims tested: the quantiser, the
 * unsigned-wrap subtraction, immunity to any constant carrier rotation (the Costas ambiguity), and "a cycle slip costs only
 * the one symbol transition". */
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "hb_dqpsk.h"

static const int code_of_step[4] = { 0x0, 0x2, 0x3, 0x1 };    /* step index 0,1,2,3 = 0, +90, 180, -90 degrees -> dibit */
static uint32_t step_q31(int bits) {                          /* the transmitter's phase increment for a dibit, as uint32 (angle/pi * 2^31) */
    switch (bits) { case 0x0: return 0u; case 0x2: return 0x40000000u; case 0x3: return 0x80000000u; default: return 0xC0000000u; }
}
static int ref_decode(int32_t diff) {                         /* nearest of {0, +90, 180, -90} in double */
    double deg = diff / 2147483648.0 * 180.0; int best = 0; double bd = 1e9;
    static const double c[4] = { 0.0, 90.0, 180.0, -90.0 };
    for (int i = 0; i < 4; i++) { double d = fabs(remainder(deg - c[i], 360.0)); if (d < bd) { bd = d; best = i; } }
    return code_of_step[best];
}

int main(void) {
    /* 1. quantiser against the double reference over random phase differences (away from the four decision boundaries) */
    lcg_state = 2024u; int checked = 0, bad = 0;
    for (int i = 0; i < 200000; i++) {
        int32_t diff = (int32_t)((lcg_next() << 8) ^ lcg_next());
        double deg = diff / 2147483648.0 * 180.0; double nearest_boundary = 1e9;
        for (int b = -135; b <= 135; b += 90) { double d = fabs(deg - b); if (d < nearest_boundary) nearest_boundary = d; }
        if (fabs(fabs(deg) - 180.0) < 1e-3) continue;
        if (nearest_boundary < 1e-3) continue;
        dqpsk_decoder_t d = { 0 };
        if (dqpsk_decode_symbol(&d, diff) != ref_decode(diff)) bad++;
        checked++;
    }
    CHECK(bad == 0, "%d of %d random phase differences decoded differently from nearest-of-four", bad, checked);
    /* the nine anchor angles */
    for (int step = 0; step < 4; step++) for (int off = -40; off <= 40; off += 20) {
        dqpsk_decoder_t d = { 0 }; int32_t diff = (int32_t)(step_q31(code_of_step[step]) + (uint32_t)(int32_t)(off / 180.0 * 2147483648.0));
        CHECK(dqpsk_decode_symbol(&d, diff) == code_of_step[step], "step %d, off %d deg", step, off);
    }

    /* 2. the subtraction wraps correctly through +-pi: prev just below +pi, curr just above -pi is a SMALL positive step */
    { dqpsk_decoder_t d = { (int32_t)0x7FFFFFF0 }; CHECK(dqpsk_decode_symbol(&d, (int32_t)0x80000010u) == 0x0, "wrap across +-pi is a ~0 degree step");
      CHECK(d.prev_phase_q31 == (int32_t)0x80000010u, "decoder state advances");
      d.prev_phase_q31 = (int32_t)0x7FFFFFF0; CHECK(dqpsk_decode_symbol(&d, (int32_t)(0x7FFFFFF0u + 0x40000000u)) == 0x2, "+90 across the wrap"); }

    /* 3. boundaries (documented): ties at +-45 / +-135 degrees */
    { struct { int32_t diff; int want; } t[] = { { 0x20000000, 0x0 }, { 0x20000001, 0x2 }, { -0x20000000, 0x1 }, { -0x1FFFFFFF, 0x0 },
                                                { 0x60000000, 0x2 }, { 0x60000001, 0x3 }, { -0x60000000, 0x3 }, { -0x5FFFFFFF, 0x1 } };
      for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) { dqpsk_decoder_t d = { 0 }; CHECK(dqpsk_decode_symbol(&d, t[i].diff) == t[i].want, "boundary diff 0x%08x", (unsigned)t[i].diff); } }

    /* 4. end to end: random dibits, accumulated phase, constant unknown rotation (any of the 4 Costas lock points and arbitrary
     *    ones) plus +-20 degrees of phase noise: every symbol after the first decodes correctly */
    const uint32_t rots[] = { 0u, 0x40000000u, 0x80000000u, 0xC0000000u, 0x12345678u, 0xDEADBEEFu };
    for (unsigned ri = 0; ri < sizeof rots / sizeof rots[0]; ri++) {
        enum { NS = 4000 }; static int tx[NS]; dqpsk_decoder_t d = { 0 };
        uint32_t ph = 0; int errs = 0;
        for (int k = 0; k < NS; k++) {
            tx[k] = (int)((lcg_next() >> 3) & 3);
            ph += step_q31(tx[k]);
            int32_t noise = (int32_t)(((int64_t)(lcg_next() % 2001) - 1000) * (int64_t)(20.0 / 180.0 * 2147483648.0) / 1000);
            int rx = dqpsk_decode_symbol(&d, (int32_t)(ph + rots[ri] + (uint32_t)noise));
            if (k > 0 && rx != tx[k]) errs++;
        }
        CHECK(errs == 0, "rotation 0x%08x: %d symbol errors", (unsigned)rots[ri], errs);
    }
    /* 5. cycle slip: the carrier loop jumps by +90 degrees at symbol 500: exactly one decoded symbol is wrong */
    { enum { NS = 1000 }; dqpsk_decoder_t d = { 0 }; uint32_t ph = 0; int errs = 0, where = -1;
      for (int k = 0; k < NS; k++) {
          int tx = (int)((lcg_next() >> 3) & 3); ph += step_q31(tx);
          uint32_t rxph = ph + ((k >= 500) ? 0x40000000u : 0u);
          int rx = dqpsk_decode_symbol(&d, (int32_t)rxph);
          if (k > 0 && rx != tx) { errs++; where = k; }
      }
      CHECK(errs == 1 && where == 500, "cycle slip at symbol 500 cost %d symbol errors (last at %d), want exactly 1 at 500", errs, where); }
    TEST_DONE("test_C_ch9_dqpsk");
}
