/* Chapter 12: lock detector (HB[81]).
 * d->locked flips only after lock_dwell_target (unlock_dwell_target) CONSECUTIVE samples on that side; the average is an
 * exponential average of |error| (alpha = 1/16) that carries 4 extra fraction bits so no step is lost to flooring.
 * Independent references: a double-precision exponential average of |e|, and the dwell semantics counted directly in the test. */
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "hb_lock_detector.h"

static lock_detector_t mk(int32_t thr, int32_t lt, int32_t ut) { lock_detector_t d; memset(&d, 0, sizeof d); d.lock_threshold = thr; d.lock_dwell_target = lt; d.unlock_dwell_target = ut; return d; }

int main(void) {
    /* ---- the average ------------------------------------------------------------------------------------------------------ */
    { lock_detector_t d = mk(1 << 30, 5, 3); for (int i = 0; i < 400; i++) lock_detector_update(&d, 10000);
      CHECK(d.error_magnitude_avg == 10000, "avg from below converges to the input (got %d)", d.error_magnitude_avg);
      for (int i = 0; i < 400; i++) lock_detector_update(&d, 100);
      CHECK(d.error_magnitude_avg == 100, "avg from above converges exactly (got %d)", d.error_magnitude_avg);
      lock_detector_update(&d, -100); CHECK(d.error_magnitude_avg == 100, "negative errors are rectified"); }
    /* step response vs the double-precision exponential average with alpha = 1/16 */
    { lock_detector_t d = mk(1 << 30, 5, 3); double ref = 0; double worst = 0;
      for (int i = 0; i < 200; i++) { int32_t e = (i < 100) ? 8000 : -2000; lock_detector_update(&d, e); ref += (fabs((double)e) - ref) / 16.0; double diff = fabs(d.error_magnitude_avg - ref); if (diff > worst) worst = diff; }
      CHECK(worst <= 1.5, "integer average within 1.5 counts of the double-precision exponential average (worst %.2f)", worst); }

    /* ---- dwell semantics: the promise is `target` consecutive samples ------------------------------------------------------ */
    { lock_detector_t d = mk(100, 5, 3); int n_lock = 0;
      for (int i = 1; i <= 50 && !n_lock; i++) if (lock_detector_update(&d, 10)) n_lock = i;                  /* |e| = 10 << threshold */
      CHECK(n_lock == 5, "locks after exactly 5 good samples (got %d)", n_lock);
      int run = 0, unl = 0;                      /* count consecutive over-threshold samples (average >= threshold) until the detector lets go */
      for (int i = 1; i <= 100 && !unl; i++) { lock_detector_update(&d, 1 << 20); if (d.error_magnitude_avg >= d.lock_threshold) run++; if (!d.locked) unl = run; }
      CHECK(unl == 3, "unlocks after exactly 3 over-threshold samples (got %d)", unl); }
    /* the anti-chatter claim: a short excursion of the average above the threshold does not unlock; a long one does.
     * One spike of 16800 puts the average at 1050 (> 1000) for exactly ONE sample (then 984); a spike of 20800 keeps it above for 5. */
    {
        int over_short = 0, over_long = 0, ever_unlocked_short = 0, ever_unlocked_long = 0;
        lock_detector_t d = mk(1000, 5, 3); for (int i = 0; i < 40; i++) lock_detector_update(&d, 0);
        CHECK(d.locked, "setup: locked");
        for (int i = 0; i < 12; i++) { lock_detector_update(&d, (i == 0) ? 16800 : 0); if (d.error_magnitude_avg >= 1000) over_short++; if (!d.locked) ever_unlocked_short = 1; }
        CHECK(over_short == 1 && !ever_unlocked_short, "short excursion (%d over-threshold sample) must not unlock (unlocked at some point: %d)", over_short, ever_unlocked_short);
        for (int i = 0; i < 60; i++) lock_detector_update(&d, 0);                         /* re-settle (it never unlocked: still locked) */
        for (int i = 0; i < 12; i++) { lock_detector_update(&d, (i == 0) ? 20800 : 0); if (d.error_magnitude_avg >= 1000) over_long++; if (!d.locked) ever_unlocked_long = 1; }
        CHECK(over_long >= 4 && ever_unlocked_long, "long excursion (%d over-threshold samples) must unlock (unlocked at some point: %d)", over_long, ever_unlocked_long);
    }
    /* threshold is strict: avg == threshold is 'bad' */
    { lock_detector_t d = mk(50, 1, 1); d.error_magnitude_acc = 16 * 50; for (int i = 0; i < 10; i++) lock_detector_update(&d, 50);
      CHECK(!d.locked, "avg == threshold counts as over threshold"); }

    /* ---- no floor bias.  bell-shaped loop error of true mean |e| ~ 9.7 against a threshold of 12 ---------------------------- */
    {
        lock_detector_t d = mk(12, 20, 5);
        double true_mean = 0; long cnt = 0, sum = 0; lcg_state = 606u;
        for (int i = 0; i < 20000; i++) {
            /* sum of 4 uniforms on [-10,10]: bell shaped, E|e| ~ 9.7 */
            int32_t e = (int32_t)(((int)(lcg_next() % 21) - 10) + ((int)(lcg_next() % 21) - 10) + ((int)(lcg_next() % 21) - 10) + ((int)(lcg_next() % 21) - 10));
            lock_detector_update(&d, e);
            if (i >= 2000) { true_mean += fabs((double)e); cnt++; sum += d.error_magnitude_avg; }
        }
        true_mean /= cnt;
        printf("  loop error with true mean |e| = %.2f, threshold 12: average reads %.2f\n", true_mean, (double)sum / cnt);
        CHECK(fabs((double)sum / cnt - true_mean) < 1.5, "average %.2f vs true mean |e| %.2f", (double)sum / cnt, true_mean);
    }
    /* constant |e| = 15 against a threshold of 8: the average reads 15 and the detector must not declare lock */
    { lock_detector_t d = mk(8, 5, 3);
      for (int i = 0; i < 200; i++) lock_detector_update(&d, 15);
      CHECK(d.error_magnitude_avg == 15 && d.locked == 0, "constant |e| = 15, threshold 8: avg %d locked %d", d.error_magnitude_avg, d.locked); }
    /* small steady errors are not lost: |e| = 5 reads 5 */
    { lock_detector_t d = mk(100, 5, 3); for (int i = 0; i < 300; i++) lock_detector_update(&d, 5);
      CHECK(d.error_magnitude_avg == 5, "constant |e| = 5 reads %d", d.error_magnitude_avg); }
    TEST_DONE("test_C_ch9_lock");
}
