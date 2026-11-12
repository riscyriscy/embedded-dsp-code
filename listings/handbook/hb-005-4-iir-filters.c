/*
 * Embedded DSP Handbook, Chapter 1: DSP Refresher for Embedded Systems / 4. IIR Filters
 * Listing 6 of 119 (index 5, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_filters.h src/hb_filters.c | Checked by: tests/test_A_filters.c (64 checks): DF2T biquad vs double DF1, DC gain, stability
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
typedef struct {
    float b0, b1, b2;  // feedforward coeffs
    float a1, a2;      // feedback coeffs (a0 normalized to 1)
    float z1, z2;      // state (Direct Form II Transposed)
} biquad_t;

float biquad_process(biquad_t *bq, float input) {
    float output = bq->b0 * input + bq->z1;
    bq->z1 = bq->b1 * input - bq->a1 * output + bq->z2;
    bq->z2 = bq->b2 * input - bq->a2 * output;
    return output;
}
