/* Source: Embedded DSP Handbook, Chapter 6 (I/Q imbalance and DC offset: preventing them, not just diagnosing them).
 * Code from the Embedded DSP Handbook; MIT License.
 * DC blocker per rail (Chapter 1 dc_blocker_t) and a 2x2 gain/phase imbalance corrector. */
#ifndef HB_IQ_CORRECT_H
#define HB_IQ_CORRECT_H

#include <stdint.h>   /* host build */
#include "hb_filters.h"   /* host build */

// DC offset: reuse Chapter 1's DC blocker, applied independently to the I and Q rails right
// after the quadrature mixer, before anything else touches the samples.
typedef struct { dc_blocker_t i_blocker, q_blocker; } iq_dc_corrector_t;

// I/Q gain and phase imbalance correction: a fixed 2x2 correction matrix, estimated once
// during calibration (e.g. from a known test tone) and applied to every subsequent sample.
// Model: the receiver delivers I' = I and Q' = g*(Q*cos(phi) + I*sin(phi)), i.e. the Q
// path has gain g and sits phi away from 90 degrees. alpha = g*cos(phi) and
// beta = g*sin(phi) invert it exactly: beta is the phase term, alpha the gain g with
// the cos(phi) of the phase error folded in.
typedef struct { float alpha, beta; } iq_imbalance_corrector_t;

/* ---- function prototypes / shared state ---- */
void iq_dc_correct(iq_dc_corrector_t *c, int32_t *i, int32_t *q);
void iq_imbalance_correct(iq_imbalance_corrector_t *c, float *i, float *q);

#endif /* HB_IQ_CORRECT_H */
