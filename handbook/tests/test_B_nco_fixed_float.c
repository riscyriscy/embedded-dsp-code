/* Chapter 6, "Fixed-point or floating-point NCO": one hour of a 1 kHz tone at 48 kHz (172.8 M samples) through four phase
 * representations. Asserts the numbers the book quotes (examples/nco_fixed_vs_float.c prints them). The float32 results
 * assume IEEE-754 single precision with no extended-precision evaluation (true for SSE and for Cortex-M4F). */
#include <stdint.h>
#include "check.h"

int main(void) {
    const long N = 48000L * 3600L;
    const double ideal = 1000.0 * 3600.0;
    uint32_t inc = (uint32_t)llround(1000.0 / 48000.0 * 4294967296.0), acc = 0;
    long wraps = 0;
    for (long i = 0; i < N; i++) { uint32_t p = acc; acc += inc; if (acc < p) wraps++; }
    CHECK_NEAR((wraps + acc / 4294967296.0 - ideal) * 360.0, -4.83, 0.01, "uint32 accumulator drift, deg");

    volatile float finc = 1000.0f / 48000.0f; float facc = 0.0f; wraps = 0;
    for (long i = 0; i < N; i++) { facc += finc; if (facc >= 1.0f) { facc -= 1.0f; wraps++; } }
    CHECK_NEAR((wraps + (double)facc - ideal) * 360.0, -463.49, 0.5, "float32 turns drift, deg");

    double dinc = 1000.0 / 48000.0, dacc = 0.0; wraps = 0;
    for (long i = 0; i < N; i++) { dacc += dinc; if (dacc >= 1.0) { dacc -= 1.0; wraps++; } }
    CHECK(fabs((wraps + dacc - ideal) * 360.0) < 0.001, "float64 turns drift %.6f deg", (wraps + dacc - ideal) * 360.0);

    volatile float rinc = (float)(2.0 * M_PI * 1000.0 / 48000.0); float racc = 0.0f; long stuck = -1;
    for (long i = 0; i < N; i++) { float before = racc; racc += rinc; if (stuck < 0 && racc == before) stuck = i; }
    CHECK(stuck > 0 && fabs(stuck / 48000.0 - 523.0) < 1.0, "float32 radian phase froze after %.1f s", stuck / 48000.0);
    CHECK(racc == 4194304.0f, "frozen at 2^22 rad, got %.1f", (double)racc);
    TEST_DONE("test_B_nco_fixed_float");
}
