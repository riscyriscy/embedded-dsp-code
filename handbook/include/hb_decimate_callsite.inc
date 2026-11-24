/* Source: Embedded DSP Handbook, Chapter 8 (Decimation: reducing the sample rate), call-site snippet.
 * Code from the Embedded DSP Handbook; MIT License.
 * SYNTAX-ONLY: statements, not a function. It needs f, adc_samples, DECIM_FACTOR, num_input_samples and output_buffer to be
 * defined by the surrounding code; tests/test_B_decimate.c includes it inside a harness function that supplies them. */

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
