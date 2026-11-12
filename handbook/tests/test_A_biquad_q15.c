/* GROUP A: Q2.14 biquad (HB[24]) and the prose claims around it.
 * References: the golden impulse response listed in the book, a double-precision DF1 biquad run on the SAME quantised coefficients,
 * scipy's float coefficients (hard-coded, with the call that produced them), and closed-form DC gain. */
#include <stdint.h>
#include "check.h"
#include "hb_biquad_q15.h"
#include "hbA_biquad_q15_trunc.h"   /* TEST-ONLY truncating variant generated from the listing */

/* scipy.signal.butter(2, 0.1):  b = [0.02008337 0.04016673 0.02008337]  a = [1 -1.56101808 0.64135154] */
static const double F_B[3] = { 0.02008336556421123, 0.04016673112842246, 0.02008336556421123 };
static const double F_A[2] = { -1.5610180758007182, 0.6413515380575631 };
/* scipy.signal.butter(2, 0.01): b = [0.00024136 0.00048272 0.00024136]  a = [1 -1.95557824 0.95654368]  (the book's a1 = -1.9556 example) */
static const double N_B[3] = { 0.00024135993427277, 0.00048271986854554, 0.00024135993427277 };
static const double N_A[2] = { -1.955578240315036, 0.9565436765112447 };

static int16_t q214(double v) { return FLOAT_TO_Q2_14((float)v); }

/* DC step on the narrowband section, run to steady state (200000 samples; the last 1000 are inspected). */
static int16_t nb_steady(int trunc, int16_t x, int16_t *tail_min, int16_t *tail_max) {
    biquad_q15_t r = { q214(N_B[0]), q214(N_B[1]), q214(N_B[2]), q214(N_A[0]), q214(N_A[1]), 0, 0 };
    biquad_q15_trunc_t t = { r.b0, r.b1, r.b2, r.a1, r.a2, 0, 0 };
    int16_t y = 0; *tail_min = 32767; *tail_max = -32768;
    for (int n = 0; n < 200000; n++) {
        y = trunc ? biquad_process_q15_trunc(&t, x) : biquad_process_q15(&r, x);
        if (n >= 199000) { if (y < *tail_min) *tail_min = y; if (y > *tail_max) *tail_max = y; } }
    return y;
}

int main(void) {
    /* --- coefficient formats ------------------------------------------------------------------------------------------ */
    CHECK(q214(F_B[0]) == 329 && q214(F_B[1]) == 658 && q214(F_B[2]) == 329, "b quantises to %d %d %d, book lists 329 658 329", q214(F_B[0]), q214(F_B[1]), q214(F_B[2]));
    CHECK(q214(F_A[0]) == -25576 && q214(F_A[1]) == 10508, "a quantises to %d %d, book lists -25576 10508", q214(F_A[0]), q214(F_A[1]));
    CHECK_NEAR(F_A[0], -1.561018, 5e-7, "book's a1 = -1.561018"); CHECK_NEAR(F_A[1], 0.641352, 5e-7, "book's a2 = 0.641352"); CHECK_NEAR(F_B[0], 0.020083, 5e-7, "book's b0 = 0.020083");
    CHECK_NEAR(N_A[0], -1.9556, 5e-5, "butter(2, 0.01): a1 = -1.9556 (the book's narrowband example)");
    /* Q1.15 clamp: FLOAT_TO_Q15 silently turns -1.9556 into exactly -1.0 (book); Q2.14 keeps it. */
    CHECK(FLOAT_TO_Q15(-1.9556f) == -32768, "FLOAT_TO_Q15(-1.9556) = %d (clamped to -1.0)", (int)FLOAT_TO_Q15(-1.9556f));
    CHECK(FLOAT_TO_Q2_14(-1.9556f) == -32041, "FLOAT_TO_Q2_14(-1.9556) = %d (round(-1.9556 * 16384) = round(-32040.9))", (int)FLOAT_TO_Q2_14(-1.9556f));
    CHECK(FLOAT_TO_Q2_14(2.0f) == 32767 && FLOAT_TO_Q2_14(-2.0f) == -32768, "Q2.14 range is [-2, 2): +2.0 clamps to 32767");
    CHECK_NEAR(FLOAT_TO_Q2_14(1.9999f) / 16384.0, 1.9999, 1e-4, "just under 2.0 is representable");

    /* --- q2_14_mul_round against its bit definition ------------------------------------------------------------------------ */
    { int bad = 0; lcg_state = 31;
      for (int i = 0; i < 50000; i++) { int32_t c = (int32_t)(lcg_next() & 0xFFFF) - 32768, s = (int32_t)(lcg_next() & 0xFFFF) - 32768;
          if ((double)q2_14_mul_round(c, s) != floor((double)c * s / 16384.0 + 0.5)) bad++; }
      CHECK(bad == 0, "q2_14_mul_round != floor(c*s/2^14 + 1/2) on %d samples", bad);
      CHECK(q2_14_mul_round(-25576, 16384) == -25576, "-1.561 (Q2.14) x 0.5 (Q15) = %d", (int)q2_14_mul_round(-25576, 16384)); }

    /* --- golden vector (listed in the book) ---------------------------------------------------------------------------------- */
    { static const int16_t gold[8] = { 329, 1172, 1948, 2289, 2324, 2160, 1881, 1551 };
      biquad_q15_t bq = { 329, 658, 329, -25576, 10508, 0, 0 }; int bad = 0;
      for (int n = 0; n < 8; n++) { int16_t y = biquad_process_q15(&bq, n == 0 ? 16384 : 0); if (y != gold[n]) { bad++; printf("    n=%d got %d want %d\n", n, y, gold[n]); } }
      CHECK(bad == 0, "golden impulse response (first 8 samples) differs at %d positions", bad); }

    /* --- vs double-precision DF1 on the same quantised coefficients ----------------------------------------------------------------- */
    /* Book: impulse of 0.5 'matching the float biquad_process reference ... to within a few LSBs at every sample ... not a growing or diverging error'. */
    { biquad_q15_t bq = { 329, 658, 329, -25576, 10508, 0, 0 };
      double x1 = 0, x2 = 0, y1 = 0, y2 = 0; int worst = 0, worst_late = 0;
      for (int n = 0; n < 600; n++) { double x = n == 0 ? 16384.0 : 0.0;
          double y = F_B[0] * x + F_B[1] * x1 + F_B[2] * x2 - F_A[0] * y1 - F_A[1] * y2; x2 = x1; x1 = x; y2 = y1; y1 = y;   /* double DF1, FLOAT coefficients */
          int d = abs((int)lrint(y) - biquad_process_q15(&bq, (int16_t)x)); if (d > worst) worst = d; if (n >= 100 && d > worst_late) worst_late = d; }
      CHECK(worst <= 3, "impulse response vs float design (unquantised coefficients): worst error %d LSB (book: a few)", worst);
      printf("  [info] impulse 0.5: worst error vs float-coefficient reference %d LSB (n>=100: %d)\n", worst, worst_late); }
    /* Step / noise input against a double-precision DF1 run on the same quantised coefficients. */
    { biquad_q15_t bq = { 329, 658, 329, -25576, 10508, 0, 0 };
      const double b0 = 329 / 16384.0, b1 = 658 / 16384.0, a1 = -25576 / 16384.0, a2 = 10508 / 16384.0;
      double x1 = 0, x2 = 0, y1 = 0, y2 = 0; int worst = 0; lcg_state = 17;
      for (int n = 0; n < 4000; n++) {
          double x = (n < 1000) ? 16384.0 : (n < 2000 ? -10000.0 : (double)((int)(lcg_next() & 0x7FFF) - 16384));
          double y = b0 * x + b1 * x1 + b0 * x2 - a1 * y1 - a2 * y2; x2 = x1; x1 = x; y2 = y1; y1 = y;
          int d = abs((int)lrint(y) - biquad_process_q15(&bq, (int16_t)x)); if (d > worst) worst = d; }
      /* Rounding noise through the 1/(1+a1+a2) DC path: 5 products x 0.5 LSB / (1316/16384) = 31 LSB worst case. */
      CHECK(worst <= 32, "Q2.14 biquad vs double on the same coefficients (steps + noise): worst error %d LSB (bound 31)", worst);
      printf("  [info] butter(2,0.1) Q2.14 biquad vs double (same coefficients), steps+noise: worst error %d LSB\n", worst); }
    /* Float design vs fixed: quantising the COEFFICIENTS moves the response; DC gain of the quantised set is exactly 1316/1316 = 1.0. */
    CHECK(329 + 658 + 329 == 16384 - 25576 + 10508, "quantised DC gain is exactly 1 (sum b = %d, 1 + a1 + a2 = %d in Q2.14)", 329 + 658 + 329, 16384 - 25576 + 10508);
    { biquad_q15_t st = { 329, 658, 329, -25576, 10508, 0, 0 }; int16_t y = 0; for (int n = 0; n < 20000; n++) y = biquad_process_q15(&st, 8000);
      /* Each of the five products carries <= 0.5 LSB rounding error; divided by the loop's DC sensitivity 1316/16384 = 0.0803 -> |error| <= 31.1 LSB. */
      CHECK(abs(y - 8000) <= 32, "DC step 8000 settles at %d (bound |err| <= 5*0.5/0.0803 = 31)", y); }

    /* --- the -1.9556 example: DC accuracy of the narrowband section --------------------------------------------------------------- */
    /* butter(2, 0.01) quantised to Q2.14 gives b = 4 8 4, a = -32040 15672: 1 + a1 + a2 is only 16 LSB wide, so a half-LSB product error is
     * amplified ~1000x at DC and the settled value depends on the signal level (truncating and rounding both).  DC gain of the set is 1. */
    CHECK(q214(N_B[0]) == 4 && q214(N_B[1]) == 8 && q214(N_B[2]) == 4 && q214(N_A[0]) == -32040 && q214(N_A[1]) == 15672, "narrowband Q2.14 coefficients: %d %d %d %d %d",
          q214(N_B[0]), q214(N_B[1]), q214(N_B[2]), q214(N_A[0]), q214(N_A[1]));
    CHECK(4 + 8 + 4 == 16384 - 32040 + 15672, "quantised narrowband DC gain is exactly 16/16 = 1.0 (so the ideal steady state equals the input)");
    CHECK(16384 - 32040 + 15672 == 16, "1 + a1 + a2 = %d LSB of Q2.14 (0.001): about 1000x amplification of a half-LSB error", 16384 - 32040 + 15672);
    int16_t lo, hi, y;
    y = nb_steady(0, 2000, &lo, &hi);
    CHECK(lo == 1048 && hi == 1048, "rounding, input 2000: settles at %d = %.1f%% (48%% low)", lo, 100.0 * (lo - 2000.0) / 2000);
    y = nb_steady(0, 8000, &lo, &hi);
    CHECK(lo == 8180 && hi == 8180, "rounding, input 8000: settles at %d = %+.2f%% (about 2%% high)", lo, 100.0 * (lo - 8000.0) / 8000);
    y = nb_steady(0, 16384, &lo, &hi);
    CHECK(lo == 16373 && hi == 16373, "rounding, input 16384: settles at %d (%+.2f%%, within 0.1%%)", lo, 100.0 * (lo - 16384.0) / 16384);
    y = nb_steady(0, 32000, &lo, &hi);
    CHECK(lo == 32767, "rounding, input 32000: %d (output clips; the unclipped settled value would be about +2.4%%)", lo);
    y = nb_steady(1, 2000, &lo, &hi);
    CHECK(lo == 0 && hi == 0, "truncating, input 2000: output stays at %d (the whole input is lost)", lo);
    y = nb_steady(1, 8000, &lo, &hi);
    CHECK(lo == 6121 && hi == 6144, "truncating, input 8000: settles at %d..%d = %.1f%% (23%% low)", lo, hi, 100.0 * ((lo + hi) / 2.0 - 8000) / 8000);
    y = nb_steady(1, 16384, &lo, &hi);
    CHECK(lo == 17373 && hi == 17373, "truncating, input 16384: settles at %d = %+.1f%% (6%% high)", lo, 100.0 * (lo - 16384.0) / 16384);
    y = nb_steady(1, 32000, &lo, &hi);
    CHECK(lo == 30673 && hi == 30673, "truncating, input 32000: %d = %+.1f%% (4%% low)", lo, 100.0 * (lo - 32000.0) / 32000);
    (void)y;

    /* --- limit cycles / saturation of the (rounding) biquad -------------------------------------------------------------- */
    { biquad_q15_t r = { 329, 658, 329, -25576, 10508, 0, 0 }; biquad_q15_trunc_t t = { 329, 658, 329, -25576, 10508, 0, 0 };
      int16_t rl = 32767, rh = -32768, tl = 32767, th = -32768;
      for (int n = 0; n < 6000; n++) { int16_t a = biquad_process_q15(&r, n == 0 ? 16384 : 0), b = biquad_process_q15_trunc(&t, n == 0 ? 16384 : 0);
          if (n >= 5800) { if (a < rl) rl = a; if (a > rh) rh = a; if (b < tl) tl = b; if (b > th) th = b; } }
      printf("  [info] zero-input tail of the golden section after an impulse: rounding %d..%d, truncating %d..%d\n", rl, rh, tl, th);
      CHECK(rl == 2 && rh == 2, "rounding biquad: zero-input tail sticks at constant %d..%d (dead band; ideal is 0)", rl, rh);
      CHECK(tl == 20 && th == 20, "truncating variant: tail sticks at constant %d..%d, ten times larger (the truncation bias)", tl, th); }
    /* Saturation: a large step drives the output into the rail; afterwards the section must recover (no overflow limit cycle). */
    { biquad_q15_t r = { 329, 658, 329, -25576, 10508, 0, 0 }; int16_t y = 0, peak = 0;
      for (int n = 0; n < 3000; n++) { y = biquad_process_q15(&r, 32767); if (y > peak) peak = y; }
      CHECK(y > 32000 && peak > 32000, "full-scale step: output %d (clamped, no wrap to negative)", y);
      int16_t mn = 32767, mx = -32768; for (int n = 0; n < 6000; n++) { y = biquad_process_q15(&r, 0); if (n >= 5800) { if (y < mn) mn = y; if (y > mx) mx = y; } }
      CHECK(abs(mn) <= 40 && abs(mx) <= 40, "after the saturating step and zero input, the section settles to %d..%d (no sustained overflow oscillation)", mn, mx); }
    /* A hard-driven resonant section (Q2.14 allows |a1| up to 2): square wave near the pole frequency, saturating the output, then silence. */
    { /* butter-like high-Q resonator: poles r = 0.98 at 0.1 fs: a1 = -2 r cos(0.2 pi) = -1.5857, a2 = 0.9604, unity-ish gain numerator b0 = 0.1 */
      biquad_q15_t r = { q214(0.1), 0, q214(-0.1), q214(-1.5857), q214(0.9604), 0, 0 }; int16_t y = 0; int peak = 0, rail = 0;
      for (int n = 0; n < 4000; n++) { int16_t x = ((n / 5) & 1) ? -32768 : 32767; y = biquad_process_q15(&r, x); if (abs(y) > peak) peak = abs(y); if (y == 32767 || y == -32768) rail++; }
      int16_t mn = 32767, mx = -32768; for (int n = 0; n < 8000; n++) { y = biquad_process_q15(&r, 0); if (n >= 7800) { if (y < mn) mn = y; if (y > mx) mx = y; } }
      printf("  [info] resonator driven to peak |y| = %d, after silence settles to %d..%d\n", peak, mn, mx);
      CHECK(rail > 100, "resonator output hit the +-full-scale rails %d times (peak %d)", rail, peak);
      CHECK(mn <= -5 && mx >= 5 && mn >= -60 && mx <= 60, "rounding does not remove the dead band: the high-Q resonator (r = 0.98) still rings %d..%d with zero input (granular limit cycle; no overflow runaway, but not 0)", mn, mx); }
    TEST_DONE("test_A_biquad_q15");
}
