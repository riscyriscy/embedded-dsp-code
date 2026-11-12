/*
 * Embedded DSP Handbook, Chapter 2: Fixed-Point Design Workflow / A fixed-point biquad: the problem with Q1.15 coefficients
 * Listing 25 of 119 (index 24, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_biquad_q15.h src/hb_biquad_q15.c tests/shims/hbA_biquad_q15_trunc.h | Checked by: tests/test_A_biquad_q15.c (33 checks): quantised coefficients and golden impulse response; vs double DF1; level-dependent error of the 1 + a1 + a2 term
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#define Q2_14_SHIFT 14
// Q2.14: same idea as Chapter 1's Q15_SHIFT/FLOAT_TO_Q15, one bit of integer range traded for
// one bit of fractional precision, specifically so a1 (which can reach magnitude just under
// 2.0 for a stable biquad) doesn't silently clamp the way FLOAT_TO_Q15 above would.
#define FLOAT_TO_Q2_14(f) ((int16_t)lrintf(fmaxf(-32768.0f, fminf((f) * (1 << Q2_14_SHIFT), 32767.0f))))

// Q2.14 coefficient x Q1.15 sample -> Q1.15-scaled product, ROUNDED rather than truncated.
// Chapter 1 already establishes why (q15_mul_trunc vs. q15_mul_round, in the limit-cycle
// discussion): a one-directional truncation bias doesn't average out, and in a feedback path
// specifically, that bias gets divided by (1 + a1 + a2), which is small by construction for
// any lowpass or bandpass design. For a narrowband section that denominator can be very small,
// so even a half-LSB error in the feedback products turns into a real,
// measurable DC error rather than harmless rounding noise. On the -1.9556
// example above, a1 and a2 quantize to -32040 and 15672, so 1 + a1 + a2 is
// only 16 LSB wide and that error is amplified about 1000x at DC: rounding
// removes the bias, but the settled output still depends on the signal level
// (measurements below the listing).
static inline int32_t q2_14_mul_round(int32_t coeff_q2_14, int32_t sample_q15) {
    int64_t product = (int64_t)coeff_q2_14 * sample_q15;
    return (int32_t)((product + (1 << (Q2_14_SHIFT - 1))) >> Q2_14_SHIFT);
}

typedef struct {
    int16_t b0, b1, b2;   // feedforward coeffs, Q2.14 (not Q1.15 - see above)
    int16_t a1, a2;       // feedback coeffs, Q2.14, a0 normalized to 1 (same convention as the
                           // float biquad_t)
    q15_wide_t z1, z2;    // state (Direct Form II Transposed), kept in the SAME wide container
                           // Chapter 1 defines for exactly this reason: a resonant section's
                           // internal state can transiently run past the nominal +-1.0 signal
                           // range even when the final output never does, and narrowing at every
                           // intermediate step (rather than once, at the true output) would clip
                           // headroom Direct Form II Transposed exists to provide.
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
