/* HB[31]: complex_t, cmul, butterfly (include/hb_complex.h).  Reference: C99 double _Complex arithmetic and the
 * butterfly's algebraic invariants (X+Y = 2A, X-Y = 2WB, parallelogram energy law), all independent of the code under test. */
#include <stdint.h>
#include <complex.h>
#include "check.h"
#include "hb_complex.h"

static double frand(void) { return ((int)(lcg_next() & 0xFFFF) - 32768) / 8192.0; }   /* about [-4, 4) */

int main(void) {
    /* exact small cases */
    complex_t one = { 1.0f, 0.0f }, j = { 0.0f, 1.0f };
    complex_t r = cmul(j, j);
    CHECK(r.re == -1.0f && r.im == 0.0f, "j*j = -1, got %g%+gj", r.re, r.im);
    r = cmul(one, j); CHECK(r.re == 0.0f && r.im == 1.0f, "1*j = j");
    complex_t a = { 3.0f, 4.0f }, b = { -2.0f, 5.0f };
    r = cmul(a, b);   /* (3+4j)(-2+5j) = -6+15j-8j+20j^2 = -26+7j */
    CHECK(r.re == -26.0f && r.im == 7.0f, "(3+4j)(-2+5j) = -26+7j, got %g%+gj", r.re, r.im);

    /* random cmul against double _Complex */
    for (int t = 0; t < 2000; t++) {
        complex_t x = { (float)frand(), (float)frand() }, y = { (float)frand(), (float)frand() };
        double complex ref = ((double)x.re + I * (double)x.im) * ((double)y.re + I * (double)y.im);
        complex_t z = cmul(x, y);
        double tol = 4e-7 * (cabs(ref) + 1.0);   /* a few float ulps of the largest partial product */
        CHECK_NEAR(z.re, creal(ref), tol, "cmul re");
        CHECK_NEAR(z.im, cimag(ref), tol, "cmul im");
    }
    /* commutativity and |a*b| = |a||b| */
    for (int t = 0; t < 500; t++) {
        complex_t x = { (float)frand(), (float)frand() }, y = { (float)frand(), (float)frand() };
        complex_t p = cmul(x, y), q = cmul(y, x);
        CHECK(p.re == q.re && p.im == q.im, "cmul commutative (bit-exact: same products)");
        CHECK_NEAR(hypot(p.re, p.im), hypot(x.re, x.im) * hypot(y.re, y.im), 1e-5 * (1 + hypot(p.re, p.im)), "|ab|=|a||b|");
    }

    /* butterfly: X = A + W*B, Y = A - W*B, in place */
    for (int t = 0; t < 2000; t++) {
        complex_t A = { (float)frand(), (float)frand() }, B = { (float)frand(), (float)frand() };
        double ang = (double)(lcg_next() & 0xFFFF) / 65536.0 * 2.0 * M_PI;
        complex_t W = { (float)cos(ang), (float)sin(ang) };
        double complex a0 = (double)A.re + I * (double)A.im, b0 = (double)B.re + I * (double)B.im;
        double complex w0 = (double)W.re + I * (double)W.im;
        double complex X = a0 + w0 * b0, Y = a0 - w0 * b0;
        butterfly(&A, &B, W);
        double tol = 4e-6;
        CHECK_NEAR(A.re, creal(X), tol, "X re"); CHECK_NEAR(A.im, cimag(X), tol, "X im");
        CHECK_NEAR(B.re, creal(Y), tol, "Y re"); CHECK_NEAR(B.im, cimag(Y), tol, "Y im");
        /* invariants stated in terms of the two OUTPUTS and the original inputs */
        CHECK_NEAR((double)A.re + B.re, 2.0 * creal(a0), tol, "X+Y = 2A (re)");
        CHECK_NEAR((double)A.im + B.im, 2.0 * cimag(a0), tol, "X+Y = 2A (im)");
        double e_out = (double)A.re * A.re + (double)A.im * A.im + (double)B.re * B.re + (double)B.im * B.im;
        double e_in  = 2.0 * (creal(a0) * creal(a0) + cimag(a0) * cimag(a0) + creal(b0) * creal(b0) + cimag(b0) * cimag(b0));
        CHECK_NEAR(e_out, e_in, 1e-5 * (1.0 + e_in), "parallelogram law: |X|^2+|Y|^2 = 2(|A|^2+|B|^2) when |W|=1");
    }
    /* W = 1 (the k=0 twiddle): plain sum / difference */
    complex_t A = { 1.5f, -2.0f }, B = { 0.25f, 4.0f }, W = { 1.0f, 0.0f };
    butterfly(&A, &B, W);
    CHECK(A.re == 1.75f && A.im == 2.0f && B.re == 1.25f && B.im == -6.0f, "W=1 butterfly exact");
    /* W = -j (the N=4, k=1 twiddle) */
    complex_t A2 = { 1.0f, 1.0f }, B2 = { 2.0f, 0.0f }, W2 = { 0.0f, -1.0f };   /* W*B = -2j */
    butterfly(&A2, &B2, W2);
    CHECK(A2.re == 1.0f && A2.im == -1.0f && B2.re == 1.0f && B2.im == 3.0f, "W=-j butterfly exact: X=1-1j, Y=1+3j");
    TEST_DONE("test_B_complex");
}
