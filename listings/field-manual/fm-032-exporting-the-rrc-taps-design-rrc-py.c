/*
 * Embedded DSP Field Manual, Chapter 5: I/Q and the Demod Chain / Exporting the RRC taps: design_rrc.py
 * Listing 33 of 61 (index 32, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_timing.h src/fm_timing.c | Checked by: tests/test_nco_qpsk.c (Gardner sign, interpolator, PI loop)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
typedef struct {
    int32_t mu;            // fractional timing offset, Q-format
    int32_t prev_sample;
} gardner_ted_t;

// mid, curr from a linear (or better, Farrow) interpolator at mu and mu+0.5 symbols.
int32_t gardner_error(gardner_ted_t *t, int32_t mid, int32_t curr) {
    int64_t err = (int64_t)(curr - t->prev_sample) * mid;   // classic Gardner TED
    t->prev_sample = curr;
    return (int32_t)(err >> 16);
}

// Applies fractional offset mu (Q15) between two adjacent oversampled buffer entries.
int32_t timing_interpolate_linear(int32_t sample_a, int32_t sample_b, int32_t mu_q15) {
    int64_t diff = (int64_t)(sample_b - sample_a) * mu_q15;
    return sample_a + (int32_t)(diff >> 15);
}

typedef struct { int32_t integrator; int32_t kp, ki; } timing_loop_t;
int32_t timing_loop_update(timing_loop_t *l, int32_t error) {
    l->integrator += (l->ki * error) >> 16;
    return ((l->kp * error) >> 16) + l->integrator;   // adjustment added to t->mu
}
