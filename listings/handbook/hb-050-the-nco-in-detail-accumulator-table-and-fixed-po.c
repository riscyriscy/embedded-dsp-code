/*
 * Embedded DSP Handbook, Chapter 6: Modulation / Demodulation and I/Q Signals / The NCO in detail: accumulator, table, and fixed-point limits
 * Listing 51 of 119 (index 50, counting from 0 in book order), exactly as it appears in the book.
 * Status: compiled | Built as: examples/nco_demo.c | Checked by: builds clean at TBL_BITS 6, 8, 10 and 12; measured by tools/nco_measure.py (the table in HB[51]); the listing is nco_cos and nco_sin from nco_demo.c with the comments shortened and tbl_init left out; not part of make test
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#define TBL_BITS 8                      // 256 entries; the demo was built for 6, 8, 10, 12
#define TBL_SIZE (1u << TBL_BITS)
static int16_t cos_tbl[TBL_SIZE + 1];   // one extra entry so idx + 1 never wraps

// phase: full circle = 2^32. interp = 0 uses the table alone; interp = 1 blends neighbours.
static inline int16_t nco_cos(uint32_t phase, int interp) {
    uint32_t idx = phase >> (32 - TBL_BITS);
    if (!interp) return cos_tbl[idx];
    int32_t frac = (int32_t)((phase >> (32 - TBL_BITS - 16)) & 0xFFFFu);   // Q16
    int32_t a = cos_tbl[idx], b = cos_tbl[idx + 1];
    return (int16_t)(a + (((b - a) * frac) >> 16));
}
static inline int16_t nco_sin(uint32_t phase, int interp) {
    return nco_cos(phase - 0x40000000u, interp);       // sin(x) = cos(x - 90 degrees)
}
