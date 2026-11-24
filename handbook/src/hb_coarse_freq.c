/* Source: Embedded DSP Handbook, Chapter 12 (Pull-in range vs. pull-in time).
 * coarse_freq_estimate_hz(): delay-and-conjugate-multiply frequency estimate over a known preamble.
 * MIT License. */
#include "hb_coarse_freq.h"
#include <math.h>

// Coarse frequency estimate from the same preamble already used for coarse timing above:
// correlate preamble_iq against a copy of itself delayed by 'lag' samples. A residual carrier
// offset rotates phase steadily across that lag; this is the same delay-and-conjugate-multiply
// mechanism Chapter 13's CFO estimator applies to the OFDM cyclic prefix, applied here to a
// known preamble instead.
// The modulation cancels in that product only if the preamble repeats with a
// period that divides lag (an unmodulated tone, or +1,+1,-1,-1,... with lag
// 4); for any other known preamble, first multiply each sample by the
// conjugate of its known symbol.
// Returns the Hz by which the received carrier sits ABOVE the NCO that made
// preamble_iq: with quadrature_mix()'s convention the baseband then rotates
// by +2*pi*f/fs per sample. Add the result to the NCO frequency to correct
// it. Valid for |f| < fs / (2 * lag); beyond that the phase wraps.
float coarse_freq_estimate_hz(const complex_t *preamble_iq, int lag, float fs_hz) {
    float sum_re = 0.0f, sum_im = 0.0f;
    for (int n = 0; n < PREAMBLE_LEN - lag; n++) {
        // conj(preamble_iq[n]) * preamble_iq[n + lag]
        const complex_t *a = &preamble_iq[n], *b = &preamble_iq[n + lag];
        sum_re += a->re * b->re + a->im * b->im;
        sum_im += a->re * b->im - a->im * b->re;
    }
    float phase = atan2f(sum_im, sum_re);            // phase rotation accumulated over 'lag' samples
    return phase * fs_hz / (2.0f * 3.14159265f * (float)lag);
}
