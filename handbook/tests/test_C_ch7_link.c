/* Chapter 7: Friis cascade, noise floor, FSPL, link_budget_margin_db(), IM3.
 * HB[63] is the only C listing; HB[60],[61],[62],[65] are formulas and HB[64] is the worked call-site.  Every number in the
 * chapter is re-derived here from independent physics (kTB from Boltzmann's constant, FSPL from (4*pi*d*f/c)^2,
 * a noise-temperature cascade, a two-tone cubic-nonlinearity simulation for IM3). */
#include <stdint.h>
#include "check.h"
#include "hb_link_budget.h"

static int g_snippet_result;
#define link_budget_margin_db(...) (g_snippet_result = (link_budget_margin_db)(__VA_ARGS__))   /* capture the worked call's value */

static double lin(double db) { return pow(10.0, db / 10.0); }
static double dB(double x)   { return 10.0 * log10(x); }

int main(void) {
    /* ---- HB[64]: the worked call, executed verbatim, must return +33 ------------------------- */
#include "hb_C_snippet_64.inc"
#undef link_budget_margin_db
    CHECK(g_snippet_result == 33, "HB[64] worked call returned %d, expected +33", g_snippet_result);

    /* ---- HB[63]: hand-computed cases, every argument distinct so a swapped/mis-signed term shows up --------------- */
    /* rx = 17 + 5 - 101 + 3 - 7 = -83 ; snr = -83 - (-113) = 30 ; margin = 30 - 11 = 19 */
    CHECK(link_budget_margin_db(17, 5, 101, 3, 7, 11, -113) == 19, "case A");
    /* rx = 20 + 3 - 100 + 3 - 2 = -76 ; snr = 34 ; margin = 22 */
    CHECK(link_budget_margin_db(20, 3, 100, 3, 2, 12, -110) == 22, "case B");
    /* link that does not close: rx = -120, snr = -20, margin = -30 */
    CHECK(link_budget_margin_db(0, 0, 120, 0, 0, 10, -100) == -30, "negative margin");
    /* zero-margin boundary: rx = -90, snr = 10, required 10 -> 0 */
    CHECK(link_budget_margin_db(10, 0, 100, 0, 0, 10, -100) == 0, "zero margin");
    /* sign of each input (finite differences around case A): +tx, +txg, -fspl, +rxg, -cable, -required, -noise_floor */
    const int m0 = link_budget_margin_db(17, 5, 101, 3, 7, 11, -113);
    CHECK(link_budget_margin_db(18, 5, 101, 3, 7, 11, -113) - m0 == +1, "d/d tx_power");
    CHECK(link_budget_margin_db(17, 6, 101, 3, 7, 11, -113) - m0 == +1, "d/d tx_gain");
    CHECK(link_budget_margin_db(17, 5, 102, 3, 7, 11, -113) - m0 == -1, "d/d fspl");
    CHECK(link_budget_margin_db(17, 5, 101, 4, 7, 11, -113) - m0 == +1, "d/d rx_gain");
    CHECK(link_budget_margin_db(17, 5, 101, 3, 8, 11, -113) - m0 == -1, "d/d cable_loss");
    CHECK(link_budget_margin_db(17, 5, 101, 3, 7, 12, -113) - m0 == -1, "d/d required_snr");
    CHECK(link_budget_margin_db(17, 5, 101, 3, 7, 11, -112) - m0 == -1, "d/d noise_floor");

    /* ---- HB[60]: Friis worked example (G, F as linear ratios) ---------------------------------------------------- */
    const double F1 = lin(1.5), F2 = lin(8.0), F3 = lin(6.0), G1 = lin(20.0), G2 = lin(-6.0);
    CHECK_NEAR(F1, 1.413, 5e-4, "F1 linear (prose 1.413)");
    CHECK_NEAR(F2, 6.310, 5e-4, "F2 linear (prose 6.310)");
    CHECK_NEAR(F3, 3.981, 5e-4, "F3 linear (prose 3.981)");
    CHECK_NEAR(G1, 100.0, 1e-9, "G1 linear (prose 100)");
    CHECK_NEAR(G2, 0.251, 5e-4, "G2 linear (prose 0.251)");
    const double t2 = (F2 - 1.0) / G1, t3 = (F3 - 1.0) / (G1 * G2);
    CHECK_NEAR(t2, 0.053, 5e-4, "second Friis term (prose 0.053)");
    CHECK_NEAR(t3, 0.119, 5e-4, "third Friis term (prose 0.119)");
    const double Ftot = F1 + t2 + t3;
    CHECK_NEAR(Ftot, 1.585, 2e-3, "F_total (prose 1.585)");
    CHECK_NEAR(dB(Ftot), 2.0, 0.01, "NF_total in dB (prose 2.0)");
    /* independent route: noise-temperature cascade, T = 290 (F - 1), NF = 1 + T_sys/290 */
    const double T1 = 290.0 * (F1 - 1), T2 = 290.0 * (F2 - 1), T3 = 290.0 * (F3 - 1);
    const double Tsys = T1 + T2 / G1 + T3 / (G1 * G2);
    CHECK_NEAR(1.0 + Tsys / 290.0, Ftot, 1e-12, "noise-temperature cascade == Friis");
    CHECK_NEAR(dB(Ftot) - 1.5, 0.5, 0.01, "'only 0.5 dB worse than the LNA's own 1.5 dB'");
    /* the ordering claim: putting the lossy mixer first must be much worse */
    const double Fbad = F2 + (F1 - 1.0) / G2 + (F3 - 1.0) / (G2 * G1);
    CHECK(dB(Fbad) > 8.0, "mixer-first ordering NF %.2f dB should be worse than 8 dB", dB(Fbad));

    /* ---- HB[61]: noise floor ------------------------------------------------------------------------------------- */
    const double kB = 1.380649e-23;
    CHECK_NEAR(10.0 * log10(kB * 290.0 * 1000.0), -174.0, 0.03, "kT at 290 K in dBm/Hz is -174 (independent: Boltzmann)");
    const double noise_floor = -174.0 + dB(125000.0) + dB(Ftot);
    CHECK_NEAR(dB(125000.0), 50.97, 0.005, "10log10(125 kHz) (prose 50.97)");
    CHECK_NEAR(noise_floor, -121.0, 0.05, "noise floor (prose -121.0 dBm)");

    /* ---- HB[62]: FSPL, rx power, SNR, margin --------------------------------------------------------------------- */
    const double c = 299792458.0;
    const double fspl = 20.0 * log10(1.0) + 20.0 * log10(915.0) + 32.44;
    CHECK_NEAR(20.0 * log10(915.0), 59.23, 0.005, "20log10(915) (prose 59.23)");
    CHECK_NEAR(fspl, 91.7, 0.05, "FSPL (prose 91.7 dB)");
    CHECK_NEAR(fspl, 20.0 * log10(4.0 * M_PI * 1000.0 * 915e6 / c), 0.01, "32.44 constant vs the physical (4 pi d f / c)^2");
    const double rx = 14.0 - fspl;
    CHECK_NEAR(rx, -77.7, 0.05, "received power (prose -77.7 dBm)");
    CHECK_NEAR(rx - noise_floor, 43.3, 0.1, "achieved SNR (prose 43.3 dB; exact 43.36)");
    CHECK_NEAR(rx - noise_floor - 10.0, 33.0, 0.5, "margin (prose 33 dB)");
    /* the integer function, fed the prose's rounded integers, must agree with the real-valued margin to within rounding */
    CHECK(abs(link_budget_margin_db(14, 0, (int)lrint(fspl), 0, 0, 10, (int)lrint(noise_floor)) - (int)lrint(rx - noise_floor - 10.0)) <= 1,
          "integer-dB function vs real-valued margin");

    /* ---- HB[65]: IM3 formula, checked by simulation: y = a1 x + a3 x^3, two equal tones --------------------------- */
    {
        const double R = 50.0, Pin_dbm = -30.0, IIP3_dbm = -8.0;
        const double A   = sqrt(2.0 * R * 1e-3 * pow(10.0, Pin_dbm / 10.0));        /* tone amplitude (V) */
        const double Aip = sqrt(2.0 * R * 1e-3 * pow(10.0, IIP3_dbm / 10.0));       /* amplitude at the input intercept */
        const double a1 = 1.0, a3 = -4.0 * a1 / (3.0 * Aip * Aip);                  /* defines IIP3 for a cubic nonlinearity */
        enum { N = 4096, K1 = 300, K2 = 310 };
        double re = 0, im = 0;                                                       /* DFT bin 2*K1 - K2 (the in-band IM3 product) */
        for (int n = 0; n < N; n++) {
            double x = A * (cos(2 * M_PI * K1 * n / N) + cos(2 * M_PI * K2 * n / N));
            double y = a1 * x + a3 * x * x * x;
            re += y * cos(2 * M_PI * (2 * K1 - K2) * n / N); im -= y * sin(2 * M_PI * (2 * K1 - K2) * n / N);
        }
        double amp_out = 2.0 * sqrt(re * re + im * im) / N;
        double im3_in_dbm = 10.0 * log10((amp_out / a1) * (amp_out / a1) / (2.0 * R) / 1e-3);   /* referred to the input */
        CHECK_NEAR(im3_in_dbm, 3.0 * Pin_dbm - 2.0 * IIP3_dbm, 0.05, "simulated IM3 (input-referred) vs 3*Pin - 2*IIP3");
        CHECK_NEAR(3.0 * Pin_dbm - 2.0 * IIP3_dbm, -74.0, 1e-12, "prose -74 dBm");
        CHECK_NEAR(-74.0 - noise_floor, 47.0, 0.05, "'47 dB above thermal noise'");
        /* the chapter's closing numbers: SNR -3.7 dB, margin -13.7 dB */
        CHECK_NEAR(rx - (-74.0), -3.7, 0.05, "SNR against the IM3 floor (prose -3.7 dB)");
        CHECK_NEAR(rx - (-74.0) - 10.0, -13.7, 0.05, "margin against the IM3 floor (prose -13.7 dB)");
    }
    TEST_DONE("test_C_ch7_link");
}
