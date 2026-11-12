/* Source: Embedded DSP Handbook, Chapter 1 (Refining a single tone's frequency beyond the bin spacing).
 * MIT License.
 * HB[12], call-site snippet: alpha, beta, gamma, k_peak, fs, N are not declared in the book.
 * Included verbatim inside a harness function by tests/test_A_claims.c. */
// alpha, beta, gamma: FFT magnitude at the peak bin k_peak and its two neighbors, k_peak-1
// and k_peak+1. delta is a fractional bin offset in [-0.5, 0.5]; the refined estimate is
// (k_peak + delta) * fs / N - same capture, same FFT, no extra samples needed.
float delta = 0.5f * (alpha - gamma) / (alpha - 2.0f * beta + gamma);
float refined_hz = (k_peak + delta) * (fs / (float)N);
