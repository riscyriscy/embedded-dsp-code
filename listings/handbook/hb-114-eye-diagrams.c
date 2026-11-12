/*
 * Embedded DSP Handbook, Chapter 19: Test and Validation Methodology / Eye diagrams
 * Listing 115 of 119 (index 114, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_testkit.c include/hb_testkit.h | Checked by: tests/test_D_testkit.c: eye dump line count/time index/amplitudes, partial symbol dropped, clean pulse train overlays exactly
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Building an eye diagram from a captured, oversampled waveform buffer: wrap every
// symbol period into the same fixed-width trace and let a host plot overlay them.
// samples_per_symbol should match the RX oversampling rate (e.g. 4 or 8 samples/symbol).
void dump_eye_diagram_uart(int32_t *waveform, int total_samples, int samples_per_symbol) {
    for (int start = 0; start + samples_per_symbol <= total_samples; start += samples_per_symbol) {
        for (int k = 0; k < samples_per_symbol; k++) {
            printf("%d,%ld\n", k, (long)waveform[start + k]);  // (time-within-symbol, amplitude)
        }
    }
    // Plotting every (k, amplitude) pair as a scatter, or connecting each symbol's k=0..N-1
    // trace as its own line, produces the classic overlaid "eye" shape on the host.
}
