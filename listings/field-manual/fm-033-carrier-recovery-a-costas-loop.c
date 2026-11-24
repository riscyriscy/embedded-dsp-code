/*
 * Embedded DSP Field Manual, Chapter 6: Loop Tuning and Bring-Up / Carrier recovery: a Costas loop
 * Listing 34 of 61 (index 33, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_costas.h src/fm_costas.c | Checked by: tests/test_nco_qpsk.c (detector, update split); closed loop: tests/test_chain.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// error ~ sign(I)*Q - sign(Q)*I - the standard QPSK-invariant phase detector.
int32_t costas_phase_error(int32_t i, int32_t q) {
    int32_t sign_i = (i >= 0) ? 1 : -1;
    int32_t sign_q = (q >= 0) ? 1 : -1;
    return (sign_i * q) - (sign_q * i);
}

typedef struct {
    int32_t phase;   // leftover proportional kick since the last correction; same
                      // full-circle-is-2^32 convention as nco_t's phase_acc
    int32_t freq;     // standing rate correction, same convention as nco_t's phase_inc
    int32_t kp, ki;
} costas_loop_t;

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
