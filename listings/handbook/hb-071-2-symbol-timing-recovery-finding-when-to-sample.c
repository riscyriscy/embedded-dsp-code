/*
 * Embedded DSP Handbook, Chapter 8: Digital Demodulation In Depth / 2. Symbol timing recovery: finding when to sample
 * Listing 72 of 119 (index 71, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_farrow.h src/hb_farrow.c | Checked by: tests/test_C_ch8_interp.c (exact on cubics, vs sine, used as interpolator in the closed-loop timing test)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Cubic Farrow-style interpolator (four-point Lagrange interpolation): uses one sample on
// either side of the linear version's two, for a meaningfully better fractional-delay fit.
// x_m1, x_0, x_1, x_2 are four consecutive samples; mu in [0,1) is the fractional offset
// between x_0 and x_1 (mu=0 returns x_0, mu approaching 1 approaches x_1).
int32_t timing_interpolate_farrow_cubic(int32_t x_m1, int32_t x_0, int32_t x_1, int32_t x_2,
                                          int32_t mu_q15) {
    // Work in float internally for the polynomial evaluation; a fixed-point production version
    // would precompute these Lagrange weights as Q15 constants per mu step instead.
    float mu = mu_q15 / 32768.0f;
    float p_m1 = -mu * (mu - 1.0f) * (mu - 2.0f) / 6.0f;
    float p_0  =  (mu + 1.0f) * (mu - 1.0f) * (mu - 2.0f) / 2.0f;
    float p_1  = -(mu + 1.0f) * mu * (mu - 2.0f) / 2.0f;
    float p_2  =  (mu + 1.0f) * mu * (mu - 1.0f) / 6.0f;
    float y = x_m1 * p_m1 + x_0 * p_0 + x_1 * p_1 + x_2 * p_2;
    return (int32_t)y;
}
