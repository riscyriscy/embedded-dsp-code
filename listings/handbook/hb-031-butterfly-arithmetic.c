/*
 * Embedded DSP Handbook, Chapter 7: FFT Butterfly: What It Is / Butterfly arithmetic
 * Listing 32 of 119 (index 31, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_complex.h | Checked by: tests/test_B_complex.c: cmul vs double _Complex, butterfly invariants, exact W=1 / W=-j cases
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
typedef struct { float re, im; } complex_t;

static inline complex_t cmul(complex_t a, complex_t b) {
    complex_t r;
    r.re = a.re * b.re - a.im * b.im;
    r.im = a.re * b.im + a.im * b.re;
    return r;
}

static inline void butterfly(complex_t *A, complex_t *B, complex_t W) {
    complex_t wb = cmul(W, *B);
    complex_t a  = *A;
    A->re = a.re + wb.re;   // X = A + W*B
    A->im = a.im + wb.im;
    B->re = a.re - wb.re;   // Y = A - W*B
    B->im = a.im - wb.im;
}
