/* Source: Embedded DSP Handbook, Chapter 8 (Decimation: reducing the sample rate).
 * Code from the Embedded DSP Handbook; MIT License.
 * Naive decimate-by-M on the float fir_filter_t (FIR_TAPS = 32). */
#include "hb_decimate.h"

// Naive (but easy to reason about) decimate-by-M: filter every sample, keep every Mth output.
// Wasteful - computes M-1 outputs for every 1 you keep - but a correct starting point.
// Returns a bool plus an out-parameter rather than a sentinel value: a sentinel return (an
// INT32_MIN "no output this cycle" marker, for instance) is easy for a careless caller to
// mistake for real data, injecting a huge, silent spike into whatever consumes the output.
bool decimate_naive(fir_filter_t *f, int16_t input, int decim_factor, int *sample_count,
                      int32_t *output) {
    int32_t filtered = fir_process(f, input);
    (*sample_count)++;
    if (*sample_count % decim_factor == 0) {
        *output = filtered;
        return true;    // *output is a valid, real sample this cycle
    }
    return false;       // *output was not written; nothing to do this cycle
}
