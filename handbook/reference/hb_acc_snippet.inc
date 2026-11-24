/* Source: Embedded DSP Handbook, Chapter 5 (Step 6: Choose accumulator width for MACs).
 * MIT License.
 * HB[19], call-site snippet: coeffs[] and history[] are not declared in the book, and "int64_t acc" is a statement sequence,
 * not a function.  Included verbatim inside a harness function by tests/test_A_fir_q15.c. */
// Q15 FIR with 32 taps: use int64_t accumulator, not int32_t
int64_t acc = 0;
for (int i = 0; i < 32; i++) {
    acc += (int64_t)coeffs[i] * (int64_t)history[i];
}
// Round and shift back to Q15 (clamp acc first if it can leave int16_t)
int16_t output = (int16_t)((acc + (1 << 14)) >> 15);
