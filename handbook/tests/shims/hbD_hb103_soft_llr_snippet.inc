/* TEST-ONLY shim: HB[103] verbatim (Embedded DSP Handbook, Chapter 21).  The listing is two statements that use a free variable `i`; it is
 * #included inside a function body by tests/test_D_snippets.c.  MIT License. */
// Hard decision (as shown earlier): collapses to a bit, loses confidence information
uint8_t hard_bit = (i < 0) ? 1 : 0;

// Soft decision: an approximate LLR for BPSK is just proportional to the raw I value itself
// (scaled by noise variance) - large positive I = confident 0, large negative I = confident 1,
// small |I| = low confidence either way.
int32_t soft_llr = (i * LLR_SCALE_Q) >> 15;  // feed this to the FEC decoder instead of hard_bit
