/*
 * Embedded DSP Handbook, Chapter 11: Digital Demodulation In Depth / 3. Carrier recovery: a Costas loop
 * Listing 75 of 119 (index 74, counting from 0 in book order), as printed in the 1.0 edition of the book.
 * Status: tested | Built as: include/hb_costas.h src/hb_costas.c (uses foundation hb_nco) | Checked by: tests/test_C_ch8_costas.c (closed loop on the real NCO/mixer): pull-in, tracking, gain units, 64-bit products
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 * In the book (November 2026), costas_phase_error is replaced by a comment that points to Field Manual listing fm-033. The text below is the full listing, kept so the compiled module still matches it.
 */
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
