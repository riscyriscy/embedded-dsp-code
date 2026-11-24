/* Source: Embedded DSP Handbook, Chapter 9 (I/Q imbalance and DC offset: preventing them, not just diagnosing them).
 * Code from the Embedded DSP Handbook; MIT License.
 * DC blocker per rail (Chapter 2 dc_blocker_t) and a 2x2 gain/phase imbalance corrector. */
#include "hb_iq_correct.h"

void iq_dc_correct(iq_dc_corrector_t *c, int32_t *i, int32_t *q) {
    *i = dc_blocker_process(&c->i_blocker, *i);
    *q = dc_blocker_process(&c->q_blocker, *q);
}

void iq_imbalance_correct(iq_imbalance_corrector_t *c, float *i, float *q) {
    float i_in = *i, q_in = *q;
    *i = i_in;
    *q = (q_in - c->beta * i_in) / c->alpha;
}
