/*
 * Embedded DSP Handbook, Chapter 8: Multirate DSP: Decimation, Interpolation, and CIC Filters / Interpolation: increasing the sample rate
 * Listing 41 of 119 (index 40, counting from 0 in book order), as printed in the 1.0 edition of the book.
 * Status: tested | Built as: src/hb_interpolate.c include/hb_interpolate.h | Checked by: tests/test_B_interpolate.c: bit-exact vs int64 reference L=1..8 incl. saturation, restored gain (DC), line amplitudes of fundamental + images vs |H(f)| of the 19 taps
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 * No longer printed in the book (November 2026): this listing is now a paragraph that points to Field Manual listing fm-017 (interpolate_by_L). The text below is the former Handbook listing, kept so the compiled module still matches it.
 */
// Zero-stuff by L, then lowpass filter, using Chapter 5's fir_q15_t/fir_process_q15 since
// this operates on genuine int16_t samples, not fir_filter_t's float design-time version.
//
// Zero-stuffing alone leaves the output about L times too quiet: only 1 sample in L
// actually carries the real signal, and an ordinary unity-gain lowpass (what Chapter 2's
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
