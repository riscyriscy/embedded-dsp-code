// A 32-bit phase-accumulator NCO with a Q15 cosine table, with and without linear interpolation.
// Build: cc -O2 -DTBL_BITS=8 -o nco_demo nco_demo.c -lm
// Run:   ./nco_demo <phase_inc> <interp 0|1> <count> > samples.i16   (raw int16, little endian)
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef TBL_BITS
#define TBL_BITS 8
#endif
#define TBL_SIZE (1u << TBL_BITS)

static int16_t cos_tbl[TBL_SIZE + 1];  // one extra entry, so index + 1 never wraps

static int16_t float_to_q15(double x) {
    long v = lround(x * 32768.0);      // rounded, then clamped to the Q15 maximum
    return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
}

static void tbl_init(void) {
    for (uint32_t i = 0; i <= TBL_SIZE; i++)
        cos_tbl[i] = float_to_q15(cos(2.0 * M_PI * (double)i / TBL_SIZE));
}

// The top TBL_BITS bits of the phase pick the entry; the next 16 bits are the fraction.
static inline int16_t nco_cos(uint32_t phase, int interp) {
    uint32_t idx = phase >> (32 - TBL_BITS);
    if (!interp) return cos_tbl[idx];
    int32_t frac = (int32_t)((phase >> (32 - TBL_BITS - 16)) & 0xFFFFu);  // Q16, 0..65535
    int32_t a = cos_tbl[idx], b = cos_tbl[idx + 1];
    return (int16_t)(a + (((b - a) * frac) >> 16));
}

// sin(x) = cos(x - 90 degrees): subtracting a quarter turn is subtracting 2^30 from the phase.
static inline int16_t nco_sin(uint32_t phase, int interp) {
    return nco_cos(phase - 0x40000000u, interp);
}

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    uint32_t inc = (uint32_t)strtoul(argv[1], NULL, 10);
    int interp = atoi(argv[2]);
    long n = atol(argv[3]);
    tbl_init();
    uint32_t acc = 0;
    for (long i = 0; i < n; i++) {
        int16_t s = nco_cos(acc, interp);
        fwrite(&s, sizeof s, 1, stdout);
        acc += inc;                    // wraps modulo 2^32: one full turn is the whole word
    }
    (void)nco_sin;
    return 0;
}
