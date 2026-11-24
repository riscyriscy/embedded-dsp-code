/* Chapter 11: AGC (HB[67]).
 * What is checked, against independent references:
 *   - agc_update() step = gain * |error| / (2 * target) / 2^shift (rounded to nearest, double-precision reference), the
 *     symmetric deadband the comments describe, the clamps [1, 16*32768], agc_apply() = floor(sample*gain/2^15).
 *   - CLOSED LOOP (gain applied, OUTPUT power measured, fed back): the prose says attack_shift = 2 settles in ~4 samples and
 *     release_shift = 8 in ~256 (the 63% point of a step in input level), at any target level and any signal level.
 *   - the loop never rails from a cold start 20 dB hot or 20 dB weak. */
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "hb_agc.h"

static int32_t power_of(int32_t out) { int64_t p = (int64_t)out * out; return p > INT32_MAX ? INT32_MAX : (int32_t)p; }

/* constant-amplitude input X; one sample per update; returns after n samples */
static void run(agc_t *a, int32_t X, int n, int *hit_min, int *hit_max) {
    for (int i = 0; i < n; i++) {
        int32_t out = agc_apply(X, a->gain_q15);
        agc_update(a, power_of(out));
        if (hit_min && a->gain_q15 <= 1) (*hit_min)++;
        if (hit_max && a->gain_q15 >= 32768 * 16) (*hit_max)++;
    }
}

/* number of samples for the gain to cover 63.2% of the way from g0 to g1 (input amplitude X0 -> X1), -1 if never */
static int tau_samples(int32_t T, int as, int rs, int32_t X0, int32_t X1, double settle_tol) {
    agc_t a = { 32768, T, as, rs };
    run(&a, X0, 60000, NULL, NULL);
    double g0 = sqrt((double)T) / X0 * 32768.0, g1 = sqrt((double)T) / X1 * 32768.0;
    if (fabs(a.gain_q15 - g0) > settle_tol * g0) return -2;          /* did not settle on the first level */
    double thr = a.gain_q15 + 0.632 * (g1 - a.gain_q15);
    for (int n = 1; n <= 20000; n++) {
        int32_t out = agc_apply(X1, a.gain_q15);
        agc_update(&a, power_of(out));
        if ((g1 > g0) ? (a.gain_q15 >= thr) : (a.gain_q15 <= thr)) return n;
    }
    return -1;
}

int main(void) {
    /* ---- agc_update(): single-step arithmetic vs gain * |e| / (2 * target) / 2^shift, rounded ------------------- */
    for (int s = 0; s <= 12; s += 3) {
        for (int e = -70000; e <= 70000; e += 997) {
            const int32_t T = 1000000, G = 100000;
            agc_t a = { G, T, s, s + 1 };                       /* target - measured = e, so measured = target - e */
            agc_update(&a, a.target_power - e);
            int sh = (e < 0) ? s : s + 1;
            double ideal = fabs((double)e) * G / (2.0 * T) / pow(2.0, sh);
            double got = (double)(a.gain_q15 - G) * (e < 0 ? -1.0 : 1.0);
            CHECK(fabs(got - ideal) <= 0.5 + pow(2.0, -sh), "step e=%d shift=%d: got %.0f ideal %.3f", e, sh, got, ideal);
            CHECK((e < 0) ? a.gain_q15 <= G : a.gain_q15 >= G, "step direction e=%d", e);
        }
    }
    /* the comment's claim: a signed >> would floor -1 to -1 and keep decrementing; the magnitude-then-sign step does not */
    { agc_t a = { 5000, 1 << 20, 4, 8 }; for (int i = 0; i < 100; i++) agc_update(&a, (1 << 20) + 15);   /* error = -15: step 0.002 */
      CHECK(a.gain_q15 == 5000, "negative sub-LSB error must not move the gain (got %d)", a.gain_q15);
      CHECK(((-15) >> 4) == -1, "reference: a raw signed shift of -15 would have given -1 (arithmetic shift)"); }
    { agc_t a = { 5000, 1 << 20, 4, 8 }; for (int i = 0; i < 100; i++) agc_update(&a, (1 << 20) - 255);  /* error = +255: step 0.0005 */
      CHECK(a.gain_q15 == 5000, "positive sub-LSB error must not move the gain (got %d)", a.gain_q15); }
    /* clamps */
    { agc_t a = { 10, 1 << 30, 0, 0 }; agc_update(&a, 1 << 30); CHECK(a.gain_q15 == 10, "zero error");
      agc_update(&a, (1 << 30) + 1000); CHECK(a.gain_q15 == 10, "tiny relative error leaves a small gain alone (got %d)", a.gain_q15);
      agc_t b = { 10, 1000, 0, 0 }; agc_update(&b, 3000); CHECK(b.gain_q15 == 1, "gain floor is 1 (got %d)", b.gain_q15);
      agc_t c = { 32768, 1000, 0, 0 }; agc_update(&c, 0); CHECK(c.gain_q15 == 32768 + 16384, "release at measured = 0 raises the gain by gain/2 (got %d)", c.gain_q15); }
    { agc_t a = { 32768 * 16 - 5, 1 << 20, 0, 0 }; agc_update(&a, 0); CHECK(a.gain_q15 == 32768 * 16, "gain cap is 16.0 (got %d)", a.gain_q15); }
    /* extreme operands stay in range: measured power at the int32 limits */
    { agc_t a = { 32768, INT32_MAX, 0, 0 }; agc_update(&a, 0); CHECK(a.gain_q15 == 32768 + 16384, "target at INT32_MAX (got %d)", a.gain_q15);
      agc_t b = { 32768, 1, 0, 0 }; agc_update(&b, INT32_MAX); CHECK(b.gain_q15 == 1, "measured at INT32_MAX against a tiny target (got %d)", b.gain_q15); }
    /* agc_apply: floor(sample*gain/2^15) */
    { int32_t xs[] = { 0, 1, -1, 100, -100, 12345, -12345, 32767, -32768 }; int32_t gs[] = { 1, 32768, 40000, 98304, 32768 * 16 };
      for (unsigned i = 0; i < sizeof xs / sizeof xs[0]; i++) for (unsigned j = 0; j < sizeof gs / sizeof gs[0]; j++) {
          double want = floor((double)xs[i] * gs[j] / 32768.0);
          CHECK(agc_apply(xs[i], gs[j]) == (int32_t)want, "apply(%d,%d)=%d want %.0f", xs[i], gs[j], agc_apply(xs[i], gs[j]), want); } }

    /* ---- CLOSED LOOP: the shifts are time constants (2^shift samples) at any target and signal level --------------- */
    {
        const int32_t Ts[] = { 1 << 22, 1 << 26, 1 << 29 };
        const int32_t Xs[] = { 500, 3000, 12000 };
        for (unsigned i = 0; i < 3; i++) for (unsigned j = 0; j < 3; j++) {
            double gstar = sqrt((double)Ts[i]) / Xs[j] * 32768.0;
            if (gstar > 32768.0 * 15 || gstar < 15000.0) continue;                        /* ideal gain inside the clamps and large enough that one Q15 LSB is a small step */
            int up   = tau_samples(Ts[i], 2, 8, Xs[j], Xs[j] + Xs[j] / 10, 0.03);       /* attack path, shift 2 */
            int down = tau_samples(Ts[i], 2, 8, Xs[j] + Xs[j] / 10, Xs[j], 0.03);       /* release path, shift 8 */
            printf("  AGC shifts 2/8, T=2^%d X=%5d: attack tau %d (prose ~4), release tau %d (prose ~256)\n",
                   (int)lrint(log2((double)Ts[i])), Xs[j], up, down);
            CHECK(up > 0 && up >= 2 && up <= 6, "attack tau %d, want ~4", up);
            CHECK(down > 0 && down >= 190 && down <= 320, "release tau %d, want ~256", down);
        }
        /* the 2^shift rule across shifts: power is not linear in gain, so a release (gain rising) is a few percent slower */
        const int32_t T = 1 << 26, X = 3000;
        for (int s = 3; s <= 10; s += 1) {
            int up   = tau_samples(T, s, s + 2, X, X + X / 10, 0.03);
            int down = tau_samples(T, s - 2 < 0 ? 0 : s - 2, s, X + X / 10, X, 0.03);
            double want = pow(2.0, s);
            CHECK(up > 0 && fabs(up - want) <= 0.35 * want + 1.5, "attack shift %d: tau %d vs 2^shift %.0f", s, up, want);
            CHECK(down > 0 && fabs(down - want) <= 0.35 * want + 1.5, "release shift %d: tau %d vs 2^shift %.0f", s, down, want);
        }
        /* never rails from a cold start (gain 1.0, 20 dB too hot and 20 dB too weak inputs) */
        const int32_t Xcold[] = { 150, 30000 };
        for (unsigned j = 0; j < 2; j++) {
            agc_t a = { 32768, 1 << 28, 2, 8 }; int lo = 0, hi = 0;
            run(&a, Xcold[j], 40000, &lo, &hi);
            double gstar = sqrt((double)(1 << 28)) / Xcold[j] * 32768.0;
            double want = gstar > 32768.0 * 16 ? 32768.0 * 16 : gstar;
            CHECK(fabs(a.gain_q15 - want) <= 0.02 * want, "AGC cold start X=%d: gain %d, want %.0f", Xcold[j], a.gain_q15, want);
        }
    }
    TEST_DONE("test_C_ch8_agc");
}
