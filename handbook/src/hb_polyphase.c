/* Source: Embedded DSP Handbook, Chapter 8 (Decimation: reducing the sample rate), polyphase.
 * Code from the Embedded DSP Handbook; MIT License.
 * Polyphase decimate-by-M (POLY_M = 4, 8 taps per phase) and the coefficient loader polyphase_init(). */
#include "hb_polyphase.h"

// Loads the N-tap prototype h[] (h[0] multiplies the newest input) and clears the state.
// An output is produced right after phase POLY_M-1 receives its sample, so that phase holds
// the newest input and phase p holds the one POLY_M-1-p inputs older: h[M-1-p], h[M-1-p+M], ...
void polyphase_init(polyphase_decim_t *f, const int32_t *h) {
    for (int p = 0; p < POLY_M; p++) {
        for (int i = 0; i < POLY_TAPS_PER_PHASE; i++) {
            f->coeffs[p][i] = h[(POLY_M - 1 - p) + i * POLY_M];
            f->history[p][i] = 0;
        }
    }
    f->phase = 0;
}

// Called once per INPUT sample (at the high rate). Returns a valid output only once every
// POLY_M calls, when the commutator completes one full cycle across all phases.
int32_t polyphase_decimate(polyphase_decim_t *f, int32_t input, int *got_output) {
    int p = f->phase;
    for (int i = POLY_TAPS_PER_PHASE - 1; i > 0; i--) f->history[p][i] = f->history[p][i - 1];
    f->history[p][0] = input;

    *got_output = 0;
    f->phase++;
    if (f->phase < POLY_M) {
        return 0;  // commutator cycle not yet complete: no output sample from this input
    }
    f->phase = 0;

    // Commutator cycle complete: sum every phase's own partial output. All M phases and all
    // M coefficient banks actually contribute here, unlike a version that only ever reads phase 0.
    int64_t acc = 0;
    for (int ph = 0; ph < POLY_M; ph++) {
        for (int i = 0; i < POLY_TAPS_PER_PHASE; i++) {
            acc += (int64_t)f->coeffs[ph][i] * f->history[ph][i];
        }
    }
    *got_output = 1;
    return (int32_t)(acc >> 15);
}
