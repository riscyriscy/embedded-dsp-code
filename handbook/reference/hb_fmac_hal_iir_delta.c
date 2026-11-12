/* Source: Embedded DSP Handbook, Chapter 3 (The IIR case).
 * MIT License.
 * Listing HB[29]; reference only (STM32 HAL FMAC delta for IIR).  NOT built. */
// Delta from the FIR sFmacConfig above - same struct, these five fields change for IIR:
sFmacConfig.pCoeffA    = iir_coeffs_a_q15;    // feedback (A) coefficients, Q1.15 - real now, not NULL
sFmacConfig.CoeffASize = IIR_ORDER;           // 3 for a 3-pole-3-zero compensator
sFmacConfig.Filter     = FMAC_FUNC_IIR_DIRECT_FORM_1;
sFmacConfig.P = IIR_ORDER;   // feedforward (B) order - was FILTER_TAPS in the FIR config above
sFmacConfig.Q = IIR_ORDER;   // feedback (A) order - was 0 (unused) in the FIR config above
// X1/X2/Y buffer setup, R gain shift, Clip, preload, and start are all unchanged from the FIR
// workflow above - IIR only changes which coefficients are real and how P/Q describe the order.
