/*
 * Embedded DSP Handbook, Chapter 8: Multirate DSP: Decimation, Interpolation, and CIC Filters / Decimation: reducing the sample rate
 * Listing 39 of 119 (index 38, counting from 0 in book order), exactly as it appears in the book.
 * Status: syntax-only | Built as: include/hb_decimate_callsite.inc | Checked by: statements, not a function; tests/test_B_decimate.c includes it in a harness (supplies f, adc_samples, DECIM_FACTOR, output_buffer) and checks its output against the reference
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Draining a block of ADC samples through the naive decimator above. fir_filter_t f is
// assumed already initialized with the anti-alias cutoff for the new, decimated rate.
int sample_count = 0;         // persists across calls - do not reset mid-stream
int out_count = 0;

for (int n = 0; n < num_input_samples; n++) {
    int32_t decimated;
    if (decimate_naive(&f, adc_samples[n], DECIM_FACTOR, &sample_count, &decimated)) {
        output_buffer[out_count++] = decimated;   // one new sample every DECIM_FACTOR inputs
    }
    // false case: nothing to do this cycle, decimated is unwritten and must not be read
}
