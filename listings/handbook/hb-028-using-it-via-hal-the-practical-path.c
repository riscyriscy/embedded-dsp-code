/*
 * Embedded DSP Handbook, Chapter 3: STM32 FMAC (Filter Math Accelerator) / Using it via HAL (the practical path)
 * Listing 29 of 119 (index 28, counting from 0 in book order), exactly as it appears in the book.
 * Status: reference-only | Built as: reference/hb_fmac_hal_config.c | Checked by: STM32 HAL FMAC configuration (placeholders left unfilled in the book, HAL types not available)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Shape of the FMAC HAL workflow, not verified letter-for-line against a specific STM32
// family's header (see the note below the code for exactly which parts are shaky).
// 1. Enable the FMAC clock and init the handle
hfmac.Instance = FMAC;
HAL_FMAC_Init(&hfmac);

// 2. Fill in a filter configuration struct
FMAC_FilterConfigTypeDef sFmacConfig;
sFmacConfig.CoeffBaseAddress = 0;                  // where coefficients live in FMAC internal RAM
sFmacConfig.CoeffBufferSize  = FILTER_TAPS;        // number of coefficients
sFmacConfig.InputBaseAddress = FILTER_TAPS;        // input buffer starts right after coeffs
sFmacConfig.InputBufferSize  = FILTER_TAPS + 1;
sFmacConfig.InputThreshold   = FMAC_THRESHOLD_1;   // watermark that triggers DMA/IRQ (0 if using DMA continuously)
sFmacConfig.OutputBaseAddress = /* next free address */;
sFmacConfig.OutputBufferSize  = /* output buffer size */;
sFmacConfig.OutputThreshold   = FMAC_THRESHOLD_1;
sFmacConfig.pCoeffB   = fir_coeffs_q15;            // your quantized Q1.15 coefficients
sFmacConfig.CoeffBSize = FILTER_TAPS;
sFmacConfig.pCoeffA   = NULL;                       // NULL/0 for FIR; feedback coeffs for IIR
sFmacConfig.CoeffASize = 0;
sFmacConfig.Filter    = FMAC_FUNC_CONVO_FIR;        // or FMAC_FUNC_IIR_DIRECT_FORM_1 for IIR
sFmacConfig.InputAccess  = FMAC_BUFFER_ACCESS_DMA;
sFmacConfig.OutputAccess = FMAC_BUFFER_ACCESS_DMA;
sFmacConfig.Clip = FMAC_CLIP_ENABLED;               // saturate rather than wrap on overflow
sFmacConfig.P = FILTER_TAPS;                        // filter order parameter
sFmacConfig.Q = 0;                                  // unused for pure FIR
sFmacConfig.R = GAIN_SHIFT;                         // output gain scaling exponent

HAL_FMAC_FilterConfig(&hfmac, &sFmacConfig);

// 3. Preload the Y buffer with zeros to avoid garbage transients at startup
HAL_FMAC_FilterPreload(&hfmac, NULL, 0, zero_output_preload, OUTPUT_PRELOAD_SIZE);

// 4. Start the filter - it now runs continuously via DMA
uint16_t output_size = OUTPUT_BUFFER_SIZE;
HAL_FMAC_FilterStart(&hfmac, output_buffer, &output_size);

// 5. Feed new samples as they arrive (if not fully DMA-driven end to end)
HAL_FMAC_AppendFilterData(&hfmac, new_input_samples, &input_size);
