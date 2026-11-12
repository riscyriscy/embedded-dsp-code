/* Chapter 12 (OOK), HB[93]: envelope_magnitude() (alpha-max-plus-beta-min, alpha=1, beta=3/8) and ook_decide_bit() (src/hb_ook.c).
 * HB[94] (tag_backscatter_bit) is a HAL-dependent conceptual listing, see tests/test_D_snippets.c.
 * References: sqrt(i^2+q^2) in double; the closed-form error of max + 3/8 min (maximum at min/max = 3/8: sqrt(1+9/64) = 1.0680);
 * a synthesized Manchester-coded OOK envelope with known chips. */
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "hb_ook.h"

int main(void) {
    /* ---- exact small cases (hand computed) ---- */
    CHECK(envelope_magnitude(3, 4) == 5 && envelope_magnitude(-3, 4) == 5 && envelope_magnitude(3, -4) == 5 && envelope_magnitude(-4, -3) == 5, "3-4-5 triangle, all quadrants: 4 + 3*3/8 = 5");
    CHECK(envelope_magnitude(0, 0) == 0 && envelope_magnitude(1000, 0) == 1000 && envelope_magnitude(0, -1000) == 1000, "axis values are exact");
    CHECK(envelope_magnitude(800, 800) == 1100, "45 degrees: 800 + 300 = 1100 (true 1131.4, -2.8%%)");
    /* ---- the prose's accuracy claim "~6.8% worst-case error": sweep the whole circle at radius 1000..30000 ---- */
    {
        double worst_pos = -1, worst_neg = 1;
        for (int r = 1000; r <= 30000; r += 2999) {
            for (int a = 0; a < 3600; a++) {
                double th = a * 3.14159265358979323846 / 1800.0;
                int32_t i = (int32_t)lround(r * cos(th)), q = (int32_t)lround(r * sin(th));
                double t = sqrt((double)i * i + (double)q * q);
                double e = (envelope_magnitude(i, q) - t) / t;
                if (e > worst_pos) worst_pos = e;
                if (e < worst_neg) worst_neg = e;
            }
        }
        printf("  [info] alpha-max-plus-beta-min (1, 3/8): worst over-estimate %+.2f %%, worst under-estimate %+.2f %% (radius >= 1000)\n", 100 * worst_pos, 100 * worst_neg);
        CHECK(worst_pos > 0.066 && worst_pos < 0.069, "worst over-estimate %.4f: prose says ~6.8%% (closed form sqrt(1+9/64)-1 = 6.80%%)", worst_pos);
        CHECK(worst_neg < -0.026 && worst_neg > -0.030, "worst under-estimate %.4f (closed form 11/8/sqrt(2) - 1 = -2.7%%)", worst_neg);
        /* the integer truncation of min*3/8 is invisible at large magnitudes but dominates tiny ones */
        double e11 = (envelope_magnitude(1, 1) - sqrt(2.0)) / sqrt(2.0);
        CHECK(envelope_magnitude(1, 1) == 1 && e11 < -0.29, "tiny inputs: (1,1) -> %d vs true 1.414 (%.0f %%): min*3/8 truncates to 0 below 3 counts", (int)envelope_magnitude(1, 1), 100 * e11);
    }

    /* ---- adaptive-threshold OOK decision on a Manchester-coded stream (1 sample per chip: data bit 1 = hi,lo; 0 = lo,hi) ---- */
    {
        enum { NBITS = 1500 };
        int bits[NBITS]; for (int i = 0; i < NBITS; i++) bits[i] = (lcg_next() >> 7) & 1;
        for (int mode = 0; mode < 4; mode++) {
            ook_detector_t d = { 0 };
            int wrong = 0, wrong_flat = 0, counted = 0, total = 2 * NBITS;
            for (int n = 0; n < total; n++) {
                int chip = (n & 1) ? !bits[n / 2] : bits[n / 2];                    /* hi/lo chip */
                double amp = (mode == 0) ? 1000 : (mode == 1) ? 60 : (mode == 2) ? 20000 : 1000.0 * pow(0.1, (double)n / total);   /* mode 3: 20 dB fade */
                double noise = ((int)(lcg_next() & 0xFF) - 128) / 128.0 * amp * 0.10;
                int32_t env = (int32_t)lround(amp * chip + noise); if (env < 0) env = 0;
                uint8_t dec = ook_decide_bit(&d, env);
                int fixed = env > 500;
                if (n >= 100) { counted++; if (dec != chip) wrong++; if (fixed != chip) wrong_flat++; }
            }
            if (mode == 3) {
                CHECK(wrong == 0, "mode %d (amplitude 1000 -> 100 over the run): adaptive threshold: %d of %d chips wrong", mode, wrong, counted);
                CHECK(wrong_flat > counted / 4, "control: a FIXED threshold of 500 fails on the faded signal (%d of %d wrong), which is the motivation for the adaptive threshold", wrong_flat, counted);
            } else if (mode == 1) {
                /* below ~16 counts of envelope the >>4 average cannot move: see next block; at amplitude 60 it still works */
                CHECK(wrong == 0, "mode %d (amplitude 60): %d of %d chips wrong", mode, wrong, counted);
            } else {
                CHECK(wrong == 0, "mode %d (amplitude %s): %d of %d chips wrong", mode, mode == 0 ? "1000" : "20000", wrong, counted);
            }
        }
    }
    /* ---- the >>4 average: quantization consequences of the integer exponential average ---- */
    {
        ook_detector_t d = { 0 };
        for (int n = 0; n < 2000; n++) ook_decide_bit(&d, 1000);
        CHECK(d.running_avg == 985, "constant envelope 1000 from 0: the average stops 15 counts short (%d) because (1000-985)>>4 == 0", (int)d.running_avg);
        CHECK(ook_decide_bit(&d, 1000) == 1, "so a long run of ones keeps deciding 1 (the average never reaches the envelope)");
        for (int n = 0; n < 2000; n++) ook_decide_bit(&d, 0);
        CHECK(d.running_avg == 0, "decaying toward a lower envelope DOES reach it (arithmetic shift of negative values rounds down): %d", (int)d.running_avg);
        /* dead band: with an envelope swing below 16 counts the average never leaves 0, so any non-zero baseline reads as 1 */
        ook_detector_t e = { 0 }; int ones = 0, n_lo = 0;
        for (int n = 0; n < 400; n++) { int32_t env = (n & 1) ? 12 : 3;    /* 'on' = 12, 'off' = 3 (small receiver offset) */
                                         uint8_t b = ook_decide_bit(&e, env); if (!(n & 1)) { n_lo++; ones += b; } }
        CHECK(e.running_avg == 0 && ones == n_lo, "envelope below 16 counts: threshold stuck at 0, every 'off' chip (level 3) reads as 1 (%d of %d)", ones, n_lo);
    }
    TEST_DONE("test_D_ook");
}
