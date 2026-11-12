/*
 * Embedded DSP Handbook, Chapter 8: Digital Demodulation In Depth / 5. Equalization (when the channel itself distorts the signal)
 * Listing 77 of 119 (index 76, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_lms_eq.h src/hb_lms_eq.c | Checked by: tests/test_C_ch8_lms.c (vs Wiener MMSE): convergence, step-size bound
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#define EQ_TAPS 8   // equalizer length in symbol-spaced taps

typedef struct {
    int32_t weights_i[EQ_TAPS], weights_q[EQ_TAPS];  // complex weights, real/imag parts
    int32_t history_i[EQ_TAPS], history_q[EQ_TAPS];
    int32_t mu;  // step size: an integer, the real step is mu / 2^31
} lms_equalizer_t;

// Complex LMS update: w[k] += mu * error * conj(history[k]).
// Splitting that complex multiply into real/imaginary parts (history's sign flips on
// the imaginary side because of the conjugate) is what actually equalizes both rails;
// updating weights_i from error_i alone, ignoring error_q and the q history entirely,
// silently drops the imaginary half of the channel and will not equalize a real
// quadrature link. The update adds mu * e * h / 2^16 to a Q15 weight
// (1.0 = 32768), so with e and h in raw sample units the effective step
// size is mu / 2^31.
void lms_update(lms_equalizer_t *eq, int32_t error_i, int32_t error_q) {
    for (int k = 0; k < EQ_TAPS; k++) {
        // Widen to int64_t before multiplying, same rule as the FIR accumulator in Chapter 2:
        // error and history values are each large enough that their product alone can already
        // approach 32-bit range, and this expression multiplies by mu on top of that sum, so
        // doing the arithmetic in plain int32_t risked signed overflow (undefined behavior),
        // not just a precision loss.
        int64_t cross_i = (int64_t)error_i * eq->history_i[k] + (int64_t)error_q * eq->history_q[k];
        int64_t cross_q = (int64_t)error_q * eq->history_i[k] - (int64_t)error_i * eq->history_q[k];
        eq->weights_i[k] += (int32_t)(((int64_t)eq->mu * cross_i) >> 16);
        eq->weights_q[k] += (int32_t)(((int64_t)eq->mu * cross_q) >> 16);
    }
}

// Forward path: apply the current weights to produce the equalized output. lms_update()
// above only adapts the weights; this is the piece that actually filters the signal with
// them, and the two need to run together every symbol for the equalizer to do anything.
void lms_filter(lms_equalizer_t *eq, int32_t input_i, int32_t input_q,
                 int32_t *out_i, int32_t *out_q) {
    for (int k = EQ_TAPS - 1; k > 0; k--) {
        eq->history_i[k] = eq->history_i[k - 1];
        eq->history_q[k] = eq->history_q[k - 1];
    }
    eq->history_i[0] = input_i;
    eq->history_q[0] = input_q;

    int64_t acc_i = 0, acc_q = 0;
    for (int k = 0; k < EQ_TAPS; k++) {
        // complex multiply: (w_i + j*w_q) * (h_i + j*h_q)
        acc_i += (int64_t)eq->weights_i[k] * eq->history_i[k] - (int64_t)eq->weights_q[k] * eq->history_q[k];
        acc_q += (int64_t)eq->weights_i[k] * eq->history_q[k] + (int64_t)eq->weights_q[k] * eq->history_i[k];
    }
    *out_i = (int32_t)(acc_i >> 15);
    *out_q = (int32_t)(acc_q >> 15);
}

// Decision-directed error: compare the equalized output against the nearest decided
// constellation point (Chapter 6's demodulate functions), the standard way to get an
// error signal once the loop is already roughly locked, without a separate known
// training sequence.
void lms_compute_error(int32_t out_i, int32_t out_q, int32_t decided_i, int32_t decided_q,
                        int32_t *error_i, int32_t *error_q) {
    *error_i = decided_i - out_i;
    *error_q = decided_q - out_q;
}
