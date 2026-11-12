/* Costas phase detector and loop.
 * Source: Field Manual, Chapter 6. */
#include "fm_costas.h"

// error ~ sign(I)*Q - sign(Q)*I - the standard QPSK-invariant phase detector.
int32_t costas_phase_error(int32_t i, int32_t q) {
    int32_t sign_i = (i >= 0) ? 1 : -1;
    int32_t sign_q = (q >= 0) ? 1 : -1;
    return (sign_i * q) - (sign_q * i);
}

void costas_update(costas_loop_t *c, int32_t phase_error) {
    // kp, ki are Q16 gains in NCO phase words per detector count (the macros
    // below), so each product is far past 32 bits: form it in 64.
    // Integrator: a standing rate, applied once below.
    c->freq  += (int32_t)(((int64_t)c->ki * phase_error) >> 16);
    // Proportional term ONLY - c->freq is applied separately; adding it here
    // too double-applies it.
    c->phase += (int32_t)(((int64_t)c->kp * phase_error) >> 16);
}

void costas_apply_correction(costas_loop_t *c, nco_t *nco) {
    nco->phase_inc = nco->base_inc + (uint32_t)c->freq;   // set the rate, don't accumulate into it
    nco->phase_acc += (uint32_t)c->phase;                  // one-time nudge
    c->phase = 0;                                          // applied - don't reapply next call
}
