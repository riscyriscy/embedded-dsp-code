/* Formula-only listings in my range (status "not-code"): HB[90] DSSS processing gain, HB[101] reflection coefficient / VSWR / return loss,
 * HB[102] CPU budget (pseudo-code with no worked numbers: nothing numeric to check).  The formulas are transcribed below
 * and checked against (a) the worked numbers the prose prints and (b) INDEPENDENT derivations:
 * VSWR is recomputed from the standing-wave voltage pattern |1 + Gamma e^{j theta}| (max/min), not from the closed form. */
#include <stdint.h>
#include <complex.h>
#include "check.h"

#define PI_D 3.14159265358979323846
static double gamma_mag(double complex z, double z0) { return cabs((z - z0) / (z + z0)); }
static double vswr_of(double g) { return (1.0 + g) / (1.0 - g); }               /* HB[101] */
static double rl_db(double g) { return -20.0 * log10(g); }                       /* HB[101] */

int main(void) {
    /* ---- HB[90]: Processing_gain_dB = 10 log10(chip_rate / bit_rate) ---- */
    CHECK_NEAR(10.0 * log10(100.0), 20.0, 1e-12, "prose: 100 chips per bit -> 20 dB processing gain");
    CHECK_NEAR(10.0 * log10(15.0), 11.7609, 1e-3, "the toy 15-chip LFSR code gives 11.76 dB");

    /* ---- HB[101]: Gamma, VSWR, return loss ---- */
    CHECK_NEAR(gamma_mag(50.0, 50.0), 0.0, 1e-15, "matched load: Gamma = 0");
    CHECK_NEAR(gamma_mag(100.0, 50.0), 1.0 / 3.0, 1e-12, "100 ohm on 50 ohm: |Gamma| = 1/3");
    CHECK_NEAR(gamma_mag(25.0, 50.0), 1.0 / 3.0, 1e-12, "25 ohm on 50 ohm: |Gamma| = 1/3");
    CHECK_NEAR(vswr_of(1.0 / 3.0), 2.0, 1e-12, "|Gamma| = 1/3 -> VSWR 2:1");
    CHECK_NEAR(rl_db(1.0 / 3.0), 9.5424, 1e-3, "VSWR 2:1 -> return loss 9.54 dB (prose: 'around 9.5 dB')");
    double refl = (1.0 / 3.0) * (1.0 / 3.0);
    CHECK_NEAR(refl, 0.1111, 1e-3, "reflected power fraction |Gamma|^2 = 11.1%% (prose: 'roughly 11%%')");
    CHECK_NEAR(-10.0 * log10(1.0 - refl), 0.5115, 1e-3, "mismatch loss -10 log10(1 - 0.111) = 0.51 dB (prose: about 0.5 dB)");
    /* independent derivation of VSWR from the standing wave: |V(theta)| = |1 + Gamma e^{j theta}| */
    {
        static const double complex loads[] = { 100.0, 25.0, 50.0 + 50.0 * I, 30.0 - 40.0 * I, 200.0 + 10.0 * I };
        for (unsigned i = 0; i < sizeof loads / sizeof *loads; i++) {
            double complex g = (loads[i] - 50.0) / (loads[i] + 50.0);
            double vmax = 0, vmin = 1e9;
            for (int k = 0; k < 200000; k++) { double th = 2 * PI_D * k / 200000.0; double v = cabs(1.0 + g * cexp(I * th)); if (v > vmax) vmax = v; if (v < vmin) vmin = v; }
            CHECK_NEAR(vmax / vmin, vswr_of(gamma_mag(loads[i], 50.0)), 1e-4, "VSWR from the sampled standing wave == (1+|G|)/(1-|G|)");
            /* return loss: reflected power in dB relative to incident, from the same wave: |G|^2 */
            CHECK_NEAR(10.0 * log10(cabs(g) * cabs(g)), -rl_db(cabs(g)), 1e-9, "RL = -20 log10|G| equals reflected power in dB");
        }
    }
    CHECK_NEAR(vswr_of(gamma_mag(50.0 + 50.0 * I, 50.0)), 2.618034, 1e-5, "50+j50 ohm: VSWR 2.618 (golden ratio squared, closed form)");
    /* ---- prose in the same chapter: quarter-wave monopole lengths ---- */
    CHECK_NEAR(299792458.0 / (4.0 * 2.4e9) * 1000.0, 31.2, 0.1, "quarter wave at 2.4 GHz is ~31 mm (Chapter 19 Key Lessons)");
    CHECK_NEAR(299792458.0 / (4.0 * 915e6) * 1000.0, 81.9, 0.1, "quarter wave at 915 MHz is ~82 mm");
    TEST_DONE("test_D_formulas");
}
