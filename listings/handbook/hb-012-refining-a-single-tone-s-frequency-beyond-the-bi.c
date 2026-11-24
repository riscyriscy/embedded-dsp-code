/*
 * Embedded DSP Handbook, Chapter 3: DFT and FFT / Refining a single tone's frequency beyond the bin spacing
 * Listing 13 of 119 (index 12, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: reference/hb_peak_interp_snippet.inc | Checked by: call-site snippet (alpha/beta/gamma/k_peak undefined in the book); verbatim .inc included in a harness in tests/test_A_claims.c; book numbers delta 0.359 / 1044.8 Hz / 5.2 Hz reproduced
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// alpha, beta, gamma: FFT magnitude at the peak bin k_peak and its two neighbors, k_peak-1
// and k_peak+1. delta is a fractional bin offset in [-0.5, 0.5]; the refined estimate is
// (k_peak + delta) * fs / N - same capture, same FFT, no extra samples needed.
float delta = 0.5f * (alpha - gamma) / (alpha - 2.0f * beta + gamma);
float refined_hz = (k_peak + delta) * (fs / (float)N);
