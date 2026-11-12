/* Source: Embedded DSP Handbook, Chapter 4 (RFFT packing, worked in code).
 * Code from the Embedded DSP Handbook; MIT License.
 * Pack two real N-point signals into one complex buffer; unpack the two spectra after fft(). */
#include "hb_rfft_pack.h"

// Pack two real N-point signals into one complex N-point buffer: a[n] becomes the real part
// and b[n] the imaginary part of packed[n]. packed must be memory of its own, separate from
// a and b: the copy would overwrite inputs it has not read yet if the arrays overlapped.
void rfft_pack(const float *a, const float *b, complex_t *packed, int N) {
    for (int n = 0; n < N; n++) {
        packed[n].re = a[n];
        packed[n].im = b[n];
    }
}

// After running the ordinary complex FFT (Chapter 4's fft()) on 'packed', unpack the two real
// spectra A[k] and B[k] from the combined result Z[k], using conjugate symmetry:
//   A[k] = (Z[k] + conj(Z[N-k])) / 2
//   B[k] = (Z[k] - conj(Z[N-k])) / (2j)
// k=0 and k=N/2 are their own mirrors (Z[N-0] wraps to Z[0]) and need the special case shown.
void rfft_unpack(const complex_t *Z, complex_t *A, complex_t *B, int N) {
    for (int k = 0; k <= N / 2; k++) {
        int mirror = (k == 0) ? 0 : (N - k);
        complex_t z_mirror_conj = { Z[mirror].re, -Z[mirror].im };

        A[k].re = 0.5f * (Z[k].re + z_mirror_conj.re);
        A[k].im = 0.5f * (Z[k].im + z_mirror_conj.im);

        // dividing by 2j is the same as: swap re/im, negate the new im, then halve
        float diff_re = Z[k].re - z_mirror_conj.re;
        float diff_im = Z[k].im - z_mirror_conj.im;
        B[k].re =  0.5f * diff_im;
        B[k].im = -0.5f * diff_re;
    }
}
