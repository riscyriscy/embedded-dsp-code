/* Source: Embedded DSP Handbook, Chapter 11 (3. Carrier recovery: a Costas loop).
 * costas_phase_error(), costas_loop_t, costas_update(), costas_apply_correction() (uses nco_t from hb_nco.h).
 * MIT License. */
#include "hb_costas.h"

// QPSK Costas loop phase detector: error ~ sign(I)*Q - sign(Q)*I
// This is the standard 4th-order-invariant detector for QPSK.
// The output is in raw sample units: for constellation points at |I| = |Q| =
// A, rotated by theta radians, it is about 2*A*sin(theta), so the detector
// gain is Kd = 2*A per radian.
int32_t costas_phase_error(int32_t i, int32_t q) {
    int32_t sign_i = (i >= 0) ? 1 : -1;
    int32_t sign_q = (q >= 0) ? 1 : -1;
    return (sign_i * q) - (sign_q * i);
}

void costas_update(costas_loop_t *c, int32_t phase_error) {
    // The gains turn detector units into NCO phase words (Chapter 12), so kp *
    // phase_error is far wider than 32 bits: form the products in 64 bits.
    // Adding half before the >> 16 rounds to nearest; a bare shift floors,
    // which biases the integrator negative on zero-mean error.
    int64_t df = (int64_t)c->ki * phase_error + (1 << 15);
    int64_t dp = (int64_t)c->kp * phase_error + (1 << 15);
    c->freq  += (int32_t)(df >> 16);   // frequency integrator: standing rate
    // Phase gets ONLY the proportional kick here. c->freq is applied
    // separately, once, as the NCO's standing rate below; adding it into
    // c->phase too would apply it a second time.
    c->phase += (int32_t)(dp >> 16);
}

// The piece that actually closes the loop: apply the Costas loop's corrected phase to the
// same NCO quadrature_mix() uses. c->freq is a standing rate estimate (already integrated in
// costas_update above); phase_inc = base_inc + freq, set fresh each call rather than
// accumulated into, keeps it anchored to the true carrier instead of ramping away from it.
// c->phase, in contrast, holds only the leftover proportional kick since the last correction,
// applied once as a one-time nudge to phase_acc and then zeroed. Adding c->freq into both
// phase_inc AND the phase_acc dump (an easy mistake, since costas_update also touches
// c->phase) would apply the frequency correction twice, roughly doubling the true offset and
// walking the NCO away from the carrier rather than locking onto it.
void costas_apply_correction(costas_loop_t *c, nco_t *nco) {
    nco->phase_inc = nco->base_inc + (uint32_t)c->freq;  // set the rate, don't accumulate into it
    nco->phase_acc += (uint32_t)c->phase;  // nudge the NCO's instantaneous phase by the phase estimate
    c->phase = 0;  // the correction has been applied; don't double-apply it next call
}
