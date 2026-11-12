/*
 * Embedded DSP Field Manual, Chapter 11: FMAC and CMSIS-DSP Cheat Sheet / FMAC: the practical shape
 * Listing 56 of 57 (index 55, counting from 0 in book order), exactly as it appears in the book.
 * Status: reference-only | Built as: (not built) | Checked by: STM32 FMAC HAL configuration; needs the vendor HAL
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
hfmac.Instance = FMAC;
HAL_FMAC_Init(&hfmac);

FMAC_FilterConfigTypeDef cfg = {
    .pCoeffB = fir_coeffs_q15, .CoeffBSize = FILTER_TAPS, .pCoeffA = NULL, .CoeffASize = 0,
    .Filter = FMAC_FUNC_CONVO_FIR,           // or FMAC_FUNC_IIR_DIRECT_FORM_1
    .InputAccess = FMAC_BUFFER_ACCESS_DMA, .OutputAccess = FMAC_BUFFER_ACCESS_DMA,
    .Clip = FMAC_CLIP_ENABLED,               // saturate rather than wrap
    .P = FILTER_TAPS, .R = GAIN_SHIFT,
    /* Coeff/Input/Output BaseAddress and BufferSize: internal-RAM layout, see AN5305 */
};
HAL_FMAC_FilterConfig(&hfmac, &cfg);
HAL_FMAC_FilterPreload(&hfmac, NULL, 0, zero_output_preload, OUTPUT_PRELOAD_SIZE);
HAL_FMAC_FilterStart(&hfmac, output_buffer, &output_size);   // runs continuously via DMA
