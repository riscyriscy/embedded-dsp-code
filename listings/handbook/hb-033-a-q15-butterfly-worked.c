/*
 * Embedded DSP Handbook, Chapter 7: FFT Butterfly: What It Is / A Q15 butterfly, worked
 * Listing 34 of 119 (index 33, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_fft_q15.h | Checked by: tests/test_B_fft_q15.c: sat/cmul/butterfly vs double complex; golden N=8 (4097 / 0,-1) reproduced via a harness driver; |z| <= 1 precondition
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Same clamping idea as q15_add_sat above (Chapter 1), just narrowed to return an int16_t
// from a 32-bit intermediate instead of clamping two already-32-bit values.
static inline int16_t q15_sat32(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

// Q15 complex multiply: two Q15 x Q15 products, each needing a wider (32-bit) intermediate
// before shifting back down to Q15, then rounded (add half an LSB before the shift) and
// saturated (Chapter 1's saturating-add pattern, applied here to a saturating shift-down).
static inline void cmul_q15(int16_t ar, int16_t ai, int16_t br, int16_t bi,
                             int16_t *rr, int16_t *ri) {
    int32_t re = ((int32_t)ar * br - (int32_t)ai * bi + (1 << 14)) >> 15;
    int32_t im = ((int32_t)ar * bi + (int32_t)ai * br + (1 << 14)) >> 15;
    *rr = q15_sat32(re);
    *ri = q15_sat32(im);
}

// Q15 radix-2 butterfly: same X = A + W*B, Y = A - W*B as the float version, plus the
// block-floating-point convention above - shift both outputs right by 1 (rounding, then
// saturating) after every stage, trading a little SNR for a guarantee that the sums
// cannot wrap. The guarantee covers the add/subtract; the product W*B is only accurate
// while |B| (complex magnitude) is at most full scale, because cmul_q15 saturates.
static inline void butterfly_q15(int16_t *Ar, int16_t *Ai, int16_t *Br, int16_t *Bi,
                                  int16_t Wr, int16_t Wi) {
    int16_t wbr, wbi;
    cmul_q15(Wr, Wi, *Br, *Bi, &wbr, &wbi);
    int32_t xr = (int32_t)*Ar + wbr, xi = (int32_t)*Ai + wbi;
    int32_t yr = (int32_t)*Ar - wbr, yi = (int32_t)*Ai - wbi;
    *Ar = q15_sat32((xr + (xr >= 0 ? 1 : 0)) >> 1);
    *Ai = q15_sat32((xi + (xi >= 0 ? 1 : 0)) >> 1);
    *Br = q15_sat32((yr + (yr >= 0 ? 1 : 0)) >> 1);
    *Bi = q15_sat32((yi + (yi >= 0 ? 1 : 0)) >> 1);
}
