/* complex_t, cmul, butterfly.
 * Source: Embedded DSP Handbook, Chapter 4 (What one butterfly computes).
 * Code from the Embedded DSP Handbook; header-only; MIT License. */
#ifndef HB_COMPLEX_H
#define HB_COMPLEX_H


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

#endif /* HB_COMPLEX_H */
