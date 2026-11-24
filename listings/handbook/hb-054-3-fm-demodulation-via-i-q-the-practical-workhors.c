/*
 * Embedded DSP Handbook, Chapter 9: Modulation / Demodulation and I/Q Signals / 3. FM demodulation via I/Q (the practical workhorse)
 * Listing 55 of 119 (index 54, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_fm_demod.h src/hb_fm_demod.c | Checked by: tests/test_B_fm_demod.c: output = A^2 sin(dphi)/2^16, sign, proportional to deviation, small-angle error 1-sin(x)/x quantified, int64 range
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Complex sample: I(t), Q(t) at time n and n-1
// FM demod output proportional to d(phase)/dt, computed without an atan2 call:
//   output[n] = I[n-1]*Q[n] - Q[n-1]*I[n]   (approximates sin(delta_phase), good for small delta_phase)
// This is the "quadrature FM demodulator" used in essentially every analog FM SDR receiver.

typedef struct {
    int32_t i_prev, q_prev;
} fm_demod_t;

int32_t fm_demod_process(fm_demod_t *d, int32_t i, int32_t q) {
    // cross product approximates sin(phase delta) for small angles between samples
    int64_t cross = (int64_t)d->i_prev * q - (int64_t)d->q_prev * i;
    d->i_prev = i;
    d->q_prev = q;
    return (int32_t)(cross >> 16);  // scale back down, adjust shift to your Q-format
}
