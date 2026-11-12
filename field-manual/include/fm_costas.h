/* Costas phase detector and loop.
 * Source: Field Manual, Chapter 6. */
#ifndef FM_COSTAS_H
#define FM_COSTAS_H

#include "fm_qformat.h"
#include "fm_nco.h"

typedef struct {
    int32_t phase;   // leftover proportional kick since the last correction; same
                      // full-circle-is-2^32 convention as nco_t's phase_acc
    int32_t freq;     // standing rate correction, same convention as nco_t's phase_inc
    int32_t kp, ki;
} costas_loop_t;

/* ---- function prototypes / shared state ---- */
int32_t costas_phase_error(int32_t i, int32_t q);
void costas_update(costas_loop_t *c, int32_t phase_error);
void costas_apply_correction(costas_loop_t *c, nco_t *nco);

#endif /* FM_COSTAS_H */
