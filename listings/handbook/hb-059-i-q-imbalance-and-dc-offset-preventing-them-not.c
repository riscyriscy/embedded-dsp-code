/*
 * Embedded DSP Handbook, Chapter 9: Modulation / Demodulation and I/Q Signals / I/Q imbalance and DC offset: preventing them, not just diagnosing them
 * Listing 60 of 119 (index 59, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_iq_correct.c include/hb_iq_correct.h | Checked by: tests/test_B_iq_correct.c: DC blocker per rail vs closed form, imbalance matrix vs explicit impairment model, image rejection before/after
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// DC offset: reuse Chapter 2's DC blocker, applied independently to the I and Q rails right
// after the quadrature mixer, before anything else touches the samples.
typedef struct { dc_blocker_t i_blocker, q_blocker; } iq_dc_corrector_t;

void iq_dc_correct(iq_dc_corrector_t *c, int32_t *i, int32_t *q) {
    *i = dc_blocker_process(&c->i_blocker, *i);
    *q = dc_blocker_process(&c->q_blocker, *q);
}

// I/Q gain and phase imbalance correction: a fixed 2x2 correction matrix, estimated once
// during calibration (e.g. from a known test tone) and applied to every subsequent sample.
// Model: the receiver delivers I' = I and Q' = g*(Q*cos(phi) + I*sin(phi)), i.e. the Q
// path has gain g and sits phi away from 90 degrees. alpha = g*cos(phi) and
// beta = g*sin(phi) invert it exactly: beta is the phase term, alpha the gain g with
// the cos(phi) of the phase error folded in.
typedef struct { float alpha, beta; } iq_imbalance_corrector_t;

void iq_imbalance_correct(iq_imbalance_corrector_t *c, float *i, float *q) {
    float i_in = *i, q_in = *q;
    *i = i_in;
    *q = (q_in - c->beta * i_in) / c->alpha;
}
