/* Source: Embedded DSP Handbook, Chapter 5 (Interpolation: increasing the sample rate).
 * Code from the Embedded DSP Handbook; MIT License.
 * Zero-stuff by L and filter with Chapter 2's fir_q15_t (FIR_TAPS = 19). Never link together with hb_fir_float / hb_decimate. */
#include "hb_interpolate.h"

// Zero-stuff by L, then lowpass filter, using Chapter 2's fir_q15_t/fir_process_q15 since
// this operates on genuine int16_t samples, not fir_filter_t's float design-time version.
//
// Zero-stuffing alone leaves the output about L times too quiet: only 1 sample in L
// actually carries the real signal, and an ordinary unity-gain lowpass (what Chapter 1's
// design tools produce by default) does nothing on its own to restore that. Scaling the
// filtered result by L here undoes it, saturating rather than silently wrapping if that
// would exceed int16_t's range.
void interpolate_by_L(fir_q15_t *f, int16_t input, int L, int16_t *outputs) {
    for (int k = 0; k < L; k++) {
        int16_t stuffed = (k == 0) ? input : 0;   // only the first slot carries the real sample
        int32_t scaled = (int32_t)fir_process_q15(f, stuffed) * L;
        if (scaled > INT16_MAX) scaled = INT16_MAX;
        else if (scaled < INT16_MIN) scaled = INT16_MIN;
        outputs[k] = (int16_t)scaled;
    }
}
