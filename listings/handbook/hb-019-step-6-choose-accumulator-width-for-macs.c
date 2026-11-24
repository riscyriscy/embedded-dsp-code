/*
 * Embedded DSP Handbook, Chapter 5: Fixed-Point Design Workflow / Step 6: Choose accumulator width for MACs
 * Listing 20 of 119 (index 19, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: reference/hb_acc_snippet.inc | Checked by: call-site snippet (coeffs/history undeclared); verbatim .inc included in a harness in tests/test_A_fir_q15.c; an int32 accumulator suffices for the 19-tap design (sum|h| = 1.48)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Q15 FIR with 32 taps: use int64_t accumulator, not int32_t
int64_t acc = 0;
for (int i = 0; i < 32; i++) {
    acc += (int64_t)coeffs[i] * (int64_t)history[i];
}
// Round and shift back to Q15 (clamp acc first if it can leave int16_t)
int16_t output = (int16_t)((acc + (1 << 14)) >> 15);
