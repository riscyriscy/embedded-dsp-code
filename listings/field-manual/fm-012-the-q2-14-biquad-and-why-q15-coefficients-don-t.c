/*
 * Embedded DSP Field Manual, Chapter 2: Filters You Ship / The Q2.14 biquad, and why Q15 coefficients don't work
 * Listing 13 of 61 (index 12, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_biquad.h src/fm_biquad.c | Checked by: tests/test_filters.c, tests/test_vectors.c (printed 8-sample vector)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#define Q2_14_SHIFT 14
#define FLOAT_TO_Q2_14(f) ((int16_t)lrintf(fmaxf(-32768.0f, fminf((f) * (1 << Q2_14_SHIFT), 32767.0f))))

// Q2.14 coeff x Q1.15 sample -> Q1.15-scaled product, ROUNDED not truncated: in a feedback
// path, a one-directional truncation bias gets divided by (1 + a1 + a2), small by construction
// for lowpass/bandpass, so even a 1-LSB bias becomes a real, measurable DC error. A truncating
// version of this function, tested against the float reference on the a1=-1.9556 example above,
// settles roughly 3% low in steady state.
static inline int32_t q2_14_mul_round(int32_t coeff_q2_14, int32_t sample_q15) {
    int64_t product = (int64_t)coeff_q2_14 * sample_q15;
    return (int32_t)((product + (1 << (Q2_14_SHIFT - 1))) >> Q2_14_SHIFT);
}

typedef struct {
    int16_t b0, b1, b2;    // feedforward, Q2.14
    int16_t a1, a2;        // feedback, Q2.14, a0 normalized to 1
    q15_wide_t z1, z2;     // state (Direct Form II Transposed), wide container
} biquad_q15_t;

int16_t biquad_process_q15(biquad_q15_t *bq, int16_t input) {
    int32_t out_wide = q15_wide_add_sat(q2_14_mul_round(bq->b0, input), bq->z1);
    int16_t output;
    if (out_wide > 32767) output = 32767;
    else if (out_wide < -32768) output = -32768;
    else output = (int16_t)out_wide;

    bq->z1 = q15_wide_add_sat(q2_14_mul_round(bq->b1, input) - q2_14_mul_round(bq->a1, output), bq->z2);
    bq->z2 = q2_14_mul_round(bq->b2, input) - q2_14_mul_round(bq->a2, output);
    return output;
}
