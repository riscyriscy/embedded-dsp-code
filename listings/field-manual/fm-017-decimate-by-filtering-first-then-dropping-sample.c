/*
 * Embedded DSP Field Manual, Chapter 4: Multirate and CIC / Decimate by filtering first, then dropping samples
 * Listing 18 of 57 (index 17, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_fir.h src/fm_fir.c | Checked by: tests/test_filters.c (interpolate_by_L)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
void interpolate_by_L(fir_q15_t *f, int16_t input, int L, int16_t *outputs) {
    for (int k = 0; k < L; k++) {
        int16_t stuffed = (k == 0) ? input : 0;      // only the first slot carries the real sample
        int32_t scaled = (int32_t)fir_process_q15(f, stuffed) * L;
        if (scaled > INT16_MAX) scaled = INT16_MAX;
        else if (scaled < INT16_MIN) scaled = INT16_MIN;
        outputs[k] = (int16_t)scaled;
    }
}
