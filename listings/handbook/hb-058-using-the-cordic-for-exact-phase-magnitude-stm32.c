/*
 * Embedded DSP Handbook, Chapter 6: Modulation / Demodulation and I/Q Signals / Using the CORDIC for exact phase/magnitude (STM32 “Phase” function)
 * Listing 59 of 119 (index 58, counting from 0 in book order), exactly as it appears in the book.
 * Status: reference-only | Built as: src/hb_cordic_hal_ref.c.txt | Checked by: ST HAL calls on a hardware peripheral; archived verbatim, not built
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// STM32 CORDIC, HAL-based: exact phase and magnitude from an I/Q pair.
// (Field names/config shape shown here match ST's HAL cordic driver pattern -
//  check stm32xxxx_hal_cordic.h for your exact family.)

CORDIC_ConfigTypeDef sCordicConfig;
sCordicConfig.Function  = CORDIC_FUNCTION_PHASE;    // "Phase" mode returns BOTH phase and magnitude
sCordicConfig.Precision = CORDIC_PRECISION_6CYCLES; // more cycles = more precision, more latency
sCordicConfig.Scale     = CORDIC_SCALE_0;
sCordicConfig.NbWrite   = CORDIC_NBWRITE_2;         // write I then Q (two arguments in)
sCordicConfig.NbRead    = CORDIC_NBREAD_2;          // read back phase then magnitude
sCordicConfig.InSize    = CORDIC_INSIZE_32BITS;     // q1.31 fixed point
sCordicConfig.OutSize   = CORDIC_OUTSIZE_32BITS;

HAL_CORDIC_Configure(&hcordic, &sCordicConfig);

int32_t io_buf[2] = { i_sample_q31, q_sample_q31 };  // write I, then Q
int32_t result[2];                                    // reads back phase, then magnitude
HAL_CORDIC_Calculate(&hcordic, io_buf, result, 1, HAL_MAX_DELAY);

int32_t phase_q31     = result[0];  // angle / pi, in q1.31 - multiply by pi to get radians
int32_t magnitude_q31 = result[1];
