/* HB[33]: q15_sat32, cmul_q15, butterfly_q15 (include/hb_fft_q15.h) and the prose's Q15 N=8 golden vector.
 * The listing is the butterfly only; the bit-reverse-then-log2(N)-stages driver is TEST HARNESS code (q15_fft_harness), not part of it: bit reversal + Chapter 1's FLOAT_TO_Q15 twiddles + butterfly_q15 per stage.
 * References: double-precision complex arithmetic, and the DFT of the same input. */
#include <stdint.h>
#include <complex.h>
#include "check.h"
#include "hb_qformat.h"
#include "hb_fft_q15.h"

static int rev_bits(int i, int bits) { int r = 0; for (int b = 0; b < bits; b++) if ((i >> b) & 1) r |= 1 << (bits - 1 - b); return r; }

/* --- harness: in-place Q15 radix-2 DIT, every butterfly shifts right by 1 => output = DFT / N --- */
static void q15_fft_harness(int16_t *re, int16_t *im, int n) {
    int bits = 0; while ((1 << bits) < n) bits++;
    for (int i = 0; i < n; i++) { int r = rev_bits(i, bits); if (r > i) { int16_t t = re[i]; re[i] = re[r]; re[r] = t; t = im[i]; im[i] = im[r]; im[r] = t; } }
    for (int size = 2; size <= n; size *= 2) {
        int half = size / 2;
        for (int g = 0; g < n; g += size)
            for (int k = 0; k < half; k++) {
                double ang = -2.0 * M_PI * k / size;
                int16_t wr = (int16_t)FLOAT_TO_Q15(cos(ang)), wi = (int16_t)FLOAT_TO_Q15(sin(ang));
                butterfly_q15(&re[g + k], &im[g + k], &re[g + k + half], &im[g + k + half], wr, wi);
            }
    }
}

int main(void) {
    /* ---- q15_sat32 ---- */
    CHECK(q15_sat32(0) == 0 && q15_sat32(32767) == 32767 && q15_sat32(32768) == 32767 && q15_sat32(-32768) == -32768 && q15_sat32(-32769) == -32768, "q15_sat32 edges");
    CHECK(q15_sat32(INT32_MAX) == 32767 && q15_sat32(INT32_MIN) == -32768, "q15_sat32 extremes");
    for (int32_t v = -70000; v <= 70000; v += 37) { int32_t w = v > 32767 ? 32767 : v < -32768 ? -32768 : v; CHECK(q15_sat32(v) == w, "q15_sat32(%d)", v); }

    /* ---- cmul_q15 against double complex, rounding to nearest, saturating ---- */
    for (int t = 0; t < 20000; t++) {
        int16_t ar = (int16_t)(lcg_next() & 0xFFFF), ai = (int16_t)(lcg_next() & 0xFFFF), br = (int16_t)(lcg_next() & 0xFFFF), bi = (int16_t)(lcg_next() & 0xFFFF);
        int16_t rr, ri; cmul_q15(ar, ai, br, bi, &rr, &ri);
        double complex p = ((double)ar + I * (double)ai) * ((double)br + I * (double)bi) / 32768.0;
        double wr = creal(p), wi = cimag(p);
        if (wr > 32767) { wr = 32767; }
        if (wr < -32768) { wr = -32768; }
        if (wi > 32767) { wi = 32767; }
        if (wi < -32768) { wi = -32768; }
        CHECK_NEAR(rr, wr, 0.5 + 1e-9, "cmul_q15 re within half an LSB (round to nearest, ties up)");
        CHECK_NEAR(ri, wi, 0.5 + 1e-9, "cmul_q15 im within half an LSB");
    }
    { int16_t rr, ri;   /* -1 * -1 in Q15: (-32768)(-32768) - 0 = +2^15 after >>15 -> must saturate to 32767, not wrap */
      cmul_q15(-32768, 0, -32768, 0, &rr, &ri); CHECK(rr == 32767 && ri == 0, "(-1)*(-1) saturates to 32767, got %d", rr);
      cmul_q15(-32768, -32768, -32768, 32767, &rr, &ri);   /* (-1-1j)(-1+1j) = 2 + 0j -> re clips at 32767, im ~ 0 */
      CHECK(rr == 32767 && abs(ri) <= 1, "(-1-j)(-1+j) = 2: re saturates to 32767 (got %d), im ~ 0 (got %d)", rr, ri);
      cmul_q15(32767, 0, 1234, -4321, &rr, &ri); CHECK(rr == 1234 && ri == -4321, "x 32767 (= 'one') is the identity to within 1 LSB: %d %d", rr, ri); }
    /* Q15 'one' is 32767, not 32768 (prose): 32767 * x loses the low bit of x */
    { int16_t rr, ri; cmul_q15(32767, 0, 20000, 0, &rr, &ri); CHECK(rr == 19999 || rr == 20000, "32767 * 20000 -> %d", rr); }

    /* ---- butterfly_q15 vs the float butterfly, including the >>1 per stage ---- */
    for (int t = 0; t < 20000; t++) {
        /* inputs inside the unit circle (|z| <= 30000): that is the domain in which W*B cannot clip (see the |B| > 1 case below) */
        int16_t Ar, Ai, Br, Bi;
        do { Ar = (int16_t)(lcg_next() & 0xFFFF); Ai = (int16_t)(lcg_next() & 0xFFFF); } while (hypot(Ar, Ai) > 30000.0);
        do { Br = (int16_t)(lcg_next() & 0xFFFF); Bi = (int16_t)(lcg_next() & 0xFFFF); } while (hypot(Br, Bi) > 30000.0);
        double ang = (double)(lcg_next() & 0xFFFF) / 65536.0 * 2.0 * M_PI;
        int16_t Wr = (int16_t)FLOAT_TO_Q15(cos(ang)), Wi = (int16_t)FLOAT_TO_Q15(sin(ang));
        double complex a = (double)Ar + I * (double)Ai, b = (double)Br + I * (double)Bi, w = (Wr + I * Wi) / 32768.0;
        double complex X = (a + w * b) / 2.0, Y = (a - w * b) / 2.0;
        butterfly_q15(&Ar, &Ai, &Br, &Bi, Wr, Wi);
        /* error budget: W*B is rounded to Q15 (<= 0.5 LSB) then the sum is halved and rounded (<= 0.5 LSB of the output): 0.25 + 0.5 */
        CHECK_NEAR(Ar, creal(X), 0.76, "X re"); CHECK_NEAR(Ai, cimag(X), 0.76, "X im");
        CHECK_NEAR(Br, creal(Y), 0.76, "Y re"); CHECK_NEAR(Bi, cimag(Y), 0.76, "Y im");
    }
    /* Documented precondition: the no-wrap guarantee holds for any input, accuracy needs |B| <= full scale as a complex magnitude.
     * With both rails near full scale |B| reaches 1.41, W*B's components exceed the Q15 range, cmul_q15 saturates, and the butterfly
     * result is the clipped one (0+16384j) rather than the ideal 0+23170j. */
    { int16_t Ar = 0, Ai = 0, Br = 32767, Bi = 32767;
      butterfly_q15(&Ar, &Ai, &Br, &Bi, 23170, 23170);   /* W = e^{j pi/4}: W*B = j*1.414*32767 = j*46340, true X = Y*-1 = 23170j */
      CHECK(Ar == 0 && Ai == 16384, "|B| = 1.41 (outside the precondition): clipped W*B gives X = 0+16384j (ideal 0+23170j), got %d%+dj", Ar, Ai); }
    /* guaranteed no WRAP: full-scale inputs, any twiddle */
    { int16_t Ar = 32767, Ai = 32767, Br = 32767, Bi = 32767; butterfly_q15(&Ar, &Ai, &Br, &Bi, 32767, 0);
      /* W = 32767 is "one" minus 1 LSB: W*B = (32767*32767 + 2^14)>>15 = 32766, so X = (32767+32766+1)>>1 = 32767, Y = (32767-32766+1)>>1 = 1 (not 0): the prose's k=0 twiddle point */
      CHECK(Ar == 32767 && Ai == 32767 && Br == 1 && Bi == 1, "full-scale: X = 32767, Y = 1 (twiddle 32767 is not exactly 1.0): %d %d %d %d", Ar, Ai, Br, Bi);
      Ar = -32768; Ai = -32768; Br = -32768; Bi = -32768; butterfly_q15(&Ar, &Ai, &Br, &Bi, 32767, 0);
      /* W*B = (32767*-32768 + 2^14)>>15 = -32767 -> X = (-32768-32767)>>1 = -32768 (floor of -32767.5), Y = (-32768+32767)>>1 = -1 */
      CHECK(Ar == -32768 && Br == -1, "negative full scale: X = -32768, Y = -1, got %d %d", Ar, Br); }
    /* rounding is symmetric (half away from zero) in the >>1: +3 -> 2, -3 -> -2 */
    { int16_t Ar = 3, Ai = -3, Br = 0, Bi = 0; butterfly_q15(&Ar, &Ai, &Br, &Bi, 32767, 0);
      CHECK(Ar == 2 && Ai == -2, "round-half-away: 3/2 -> 2, -3/2 -> -2, got %d %d", Ar, Ai); }

    /* ---- the prose's Q15 N=8 golden vector (via the harness driver) ----
     * cosine at bin 1 at 0.25 of full scale, "X[1] and X[7] land at 4097 ... every other bin lands at 0 or -1" */
    { int16_t re[8], im[8];
      for (int n = 0; n < 8; n++) { re[n] = (int16_t)lrint(0.25 * 32768.0 * cos(2.0 * M_PI * n / 8.0)); im[n] = 0; }
      q15_fft_harness(re, im, 8);
      printf("  [info] Q15 N=8 harness result: re = {");
      for (int k = 0; k < 8; k++) { printf("%d%s", re[k], k < 7 ? ", " : ""); }
      printf("}  im = {");
      for (int k = 0; k < 8; k++) { printf("%d%s", im[k], k < 7 ? ", " : ""); }
      printf("}\n");
      /* ideal: DFT/8 = 4096 at bins 1,7; prose: 4097 */
      CHECK_NEAR(re[1], 4096.0, 3.0, "X[1] within a few LSB of the ideal 4096"); CHECK_NEAR(re[7], 4096.0, 3.0, "X[7] ~ 4096");
      CHECK(re[1] == 4097 && re[7] == 4097, "prose golden value: X[1] = X[7] = 4097, got %d, %d", re[1], re[7]);
      for (int k = 0; k < 8; k++) if (k != 1 && k != 7) CHECK(re[k] == 0 || re[k] == -1, "prose: bin %d lands at 0 or -1, got %d", k, re[k]);
      for (int k = 0; k < 8; k++) CHECK(im[k] == 0 || im[k] == -1 || im[k] == 1, "imag bin %d small, got %d", k, im[k]); }
    /* harness vs DFT/N for random input at several sizes: confirms butterfly_q15 composes into a correct scaled FFT */
    for (int n = 8; n <= 256; n *= 2) {
        int16_t re[256], im[256]; double complex x[256];
        for (int i = 0; i < n; i++) { re[i] = (int16_t)((int)(lcg_next() & 0x3FFF) - 8192); im[i] = (int16_t)((int)(lcg_next() & 0x3FFF) - 8192); x[i] = re[i] + I * im[i]; }
        q15_fft_harness(re, im, n);
        double worst = 0;
        for (int k = 0; k < n; k++) {
            double complex X = 0; for (int m = 0; m < n; m++) X += x[m] * cexp(-2.0 * M_PI * I * (double)((long long)k * m % n) / n);
            X /= n;   /* log2(N) halvings */
            double e = cabs(X - ((double)re[k] + I * (double)im[k])); if (e > worst) worst = e;
        }
        /* per stage: <=0.76 LSB, error halves each later stage: total < ~1.5 LSB; twiddle quantisation adds ~|x|*2^-15*few */
        printf("  [info] n=%3d Q15 FFT/N vs DFT/N: worst error %.2f LSB\n", n, worst);
        CHECK(worst < 6.0, "n=%d: worst error %g LSB", n, worst);
    }
    TEST_DONE("test_B_fft_q15");
}
