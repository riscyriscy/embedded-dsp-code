/* Chapter 9, "The NCO in detail": the one-cosine-table NCO with optional linear interpolation (examples/nco_demo.c).
 * Checks the numbers the book quotes: worst error against double-precision sin/cos over 10^6 phases (256 entries,
 * interpolated: 4.05 LSB), quarter-turn wrap, sin(x) = cos(x - 90 deg), and the frequency-quantization figures
 * for 1000 Hz at 48 kHz. The SFDR figures come from tools/nco_measure.py (needs numpy), not from this test. */
#include <stdint.h>
#include "check.h"
#define main demo_main
#include "../examples/nco_demo.c"
#undef main

int main(void) {
    tbl_init();
    /* one full turn is the whole 32-bit word: a step of 2^30 visits the four quadrant points exactly */
    uint32_t acc = 0;
    const int16_t quad[4] = { 32767, 0, -32768, 0 };
    for (int i = 0; i < 8; i++) { CHECK(nco_cos(acc, 0) == quad[i % 4], "quadrant %d: got %d", i, nco_cos(acc, 0)); acc += 0x40000000u; }
    CHECK(acc == 0u, "eight quarter turns wrap to 0, got %u", acc);

    double worst_s = 0, worst_c = 0;
    for (uint32_t i = 0; i < 1000000; i++) {
        uint32_t ph = (uint32_t)((uint64_t)i * 4294967296ull / 1000000ull);
        double ang = 2.0 * M_PI * (double)ph / 4294967296.0;
        double es = fabs(nco_sin(ph, 1) - 32768.0 * sin(ang));
        double ec = fabs(nco_cos(ph, 1) - 32768.0 * cos(ang));
        if (es > worst_s) worst_s = es;
        if (ec > worst_c) worst_c = ec;
    }
    CHECK(TBL_BITS != 8 || (worst_s < 4.1 && worst_c < 4.1), "256 entries + interpolation: sin %.2f, cos %.2f LSB (book: 4.05)", worst_s, worst_c);
    printf("  max error with interpolation, %u entries: sin %.2f LSB, cos %.2f LSB\n", TBL_SIZE, worst_s, worst_c);

    /* frequency quantization: 1000 Hz at 48 kHz */
    double exact = 1000.0 / 48000.0 * 4294967296.0;
    uint32_t inc = (uint32_t)(exact + 0.5);
    CHECK(inc == 89478485u, "phase_inc for 1000 Hz at 48 kHz = %u", inc);
    CHECK_NEAR(inc * 48000.0 / 4294967296.0, 999.999996275, 1e-9, "actual frequency");
    CHECK_NEAR(48000.0 / 4294967296.0 * 1e6, 11.176, 0.001, "step in uHz");
    CHECK_NEAR(((int)(1000.0 / 48000.0 * 65536.0 + 0.5)) * 48000.0 / 65536.0, 999.7559, 0.0001, "16-bit accumulator result");
    TEST_DONE("test_B_nco_table");
}
