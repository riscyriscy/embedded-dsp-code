/*
 * Embedded DSP Handbook, Chapter 8: Multirate DSP: Decimation, Interpolation, and CIC Filters / Decimation: reducing the sample rate
 * Listing 40 of 119 (index 39, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_polyphase.c include/hb_polyphase.h | Checked by: tests/test_B_decimate.c: output == direct FIR decimated and == decimate_naive with the commutator-correct coefficient split
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Polyphase decimate-by-M: split an N-tap prototype filter's coefficients into M sub-filters,
// phase p holding h[M-1-p], h[M-1-p+M], h[M-1-p+2M], ... Each of the M sub-filters gets its
// OWN delay line, fed by a commutator that routes each incoming sample to exactly one phase in
// round-robin order; every M inputs, summing all M phases' partial outputs together produces one
// decimated output sample. This is what actually gives the 1/M multiply-accumulate saving over
// filtering every sample at the full N-tap cost and then discarding M-1 out of every M outputs,
// on average: most calls (the history-shift-only ones) cost nothing, one call in every M pays
// the full N-multiply combine, averaging to L = N/M multiplies per input over the whole cycle.
#define POLY_M 4          // decimation factor
#define POLY_TAPS_PER_PHASE 8   // L = N/M taps per phase, N = POLY_M * POLY_TAPS_PER_PHASE total

typedef struct {
    int32_t coeffs[POLY_M][POLY_TAPS_PER_PHASE];   // coeffs[phase][tap], precomputed once at init
    int32_t history[POLY_M][POLY_TAPS_PER_PHASE];  // one independent delay line PER PHASE
    int phase;                                      // which phase the next input commutates to
} polyphase_decim_t;

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
