/*
 * Embedded DSP Field Manual, Chapter 12: Bugs and Golden Vectors / Running these without a project template
 * Listing 57 of 57 (index 56, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: tests/test_vectors.c | Checked by: the harness the book lists, run as `make test` (main() is book_vectors() so extra rows can share the process)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#include <stdio.h>
#include <string.h>

static int all_ok = 1;
static void check_i16(const char *name, int idx, int16_t got, int16_t want) {
    if (got != want) { printf("FAIL %s[%d]: got %d, want %d\n", name, idx, got, want); all_ok = 0; }
}

int main(void) {
    // Rows that print a coefficient array (the 19-tap FIR, the 19-tap RRC) need no filtering at
    // all to check - that array IS what design_filter.py/design_rrc.py wrote to the header, so
    // just diff it against the printed row directly.
    static const int16_t fir_vector[FIR_TAPS] = {
        -212, 5, 560, 678, -474, -1977, -1180, 3418, 9498,
        12339, 9498, 3418, -1180, -1977, -474, 678, 560, 5, -212
    };
    for (int n = 0; n < FIR_TAPS; n++) check_i16("fir_coeffs", n, fir_coeffs_q15[n], fir_vector[n]);

    // Rows that print a RESPONSE (the Q2.14 biquad impulse, Goertzel N=8) need the actual state
    // struct run through the printed input. Chapter 2's biquad vector: 0.5 impulse, Q2.14
    // coefficients - 0.5 is exactly representable in Q15 (16384), so no rounding slack to budget
    // for the way a saturated 1.0 impulse would need.
    static const int16_t biquad_vector[8] = { 329, 1172, 1948, 2289, 2324, 2160, 1881, 1551 };
    // b0=b2=329, b1=658, a1=-25576, a2=10508: this chapter's Q2.14 Butterworth example, quantized.
    biquad_q15_t bq = { .b0 = 329, .b1 = 658, .b2 = 329, .a1 = -25576, .a2 = 10508, .z1 = 0, .z2 = 0 };
    for (int n = 0; n < 8; n++) {
        int16_t in = (n == 0) ? FLOAT_TO_Q15(0.5f) : 0;
        check_i16("biquad", n, biquad_process_q15(&bq, in), biquad_vector[n]);
    }

    printf(all_ok ? "ALL VECTORS OK\n" : "SOME VECTORS FAILED\n");
    return all_ok ? 0 : 1;
}
