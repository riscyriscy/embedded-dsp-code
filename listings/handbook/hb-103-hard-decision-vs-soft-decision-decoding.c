/*
 * Embedded DSP Handbook, Chapter 18: Forward Error Correction (FEC) / Hard-decision vs. soft-decision decoding
 * Listing 104 of 119 (index 103, counting from 0 in book order), exactly as it appears in the book.
 * Status: syntax-only | Built as: tests/shims/hbD_hb103_soft_llr_snippet.inc | Checked by: two statements with a free variable `i`; executed verbatim inside a harness function in tests/test_D_fec.c (llr = raw value at LLR_SCALE_Q, sign agrees with hard bit); LLR_SCALE_Q is defined in hb_fec.h
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Hard decision (as shown earlier): collapses to a bit, loses confidence information
uint8_t hard_bit = (i < 0) ? 1 : 0;

// Soft decision: an approximate LLR for BPSK is just proportional to the raw I value itself
// (scaled by noise variance) - large positive I = confident 0, large negative I = confident 1,
// small |I| = low confidence either way.
int32_t soft_llr = (i * LLR_SCALE_Q) >> 15;  // feed this to the FEC decoder instead of hard_bit
