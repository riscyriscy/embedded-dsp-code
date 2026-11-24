/* Source: Embedded DSP Handbook, Chapter 11 (3. Carrier recovery: a Costas loop).
 * costas_phase_error(), costas_loop_t, costas_update(), costas_apply_correction() (uses nco_t from hb_nco.h).
 * MIT License. */
#ifndef HB_COSTAS_H
#define HB_COSTAS_H

#include <stdint.h>
#include "hb_nco.h"

typedef struct {
    int32_t phase;      // current NCO phase correction estimate. Same full-circle-equals-2^32
                         // convention as nco_t's phase_acc/phase_inc (not radians, not a
                         // separate Q-format scale): this is what makes directly adding
                         // c->phase into nco->phase_acc in costas_apply_correction below
                         // numerically valid rather than a unit mismatch.
    int32_t freq;       // current frequency estimate (integrator output), same convention:
                         // a per-sample phase-step correction in the identical units as
                         // nco->phase_inc, added to nco->base_inc, not radians/sec or Hz.
    int32_t kp, ki;      // loop gains: Q16, NCO words per detector unit
} costas_loop_t;

/* ---- function prototypes / shared state ---- */
int32_t costas_phase_error(int32_t i, int32_t q);
void costas_update(costas_loop_t *c, int32_t phase_error);
void costas_apply_correction(costas_loop_t *c, nco_t *nco);

#endif /* HB_COSTAS_H */
