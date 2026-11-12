/* Source: Embedded DSP Handbook, Chapter 2 (Step 3: Map the "unity" reference).
 * MIT License. */
#ifndef HB_ADC_Q15_H
#define HB_ADC_Q15_H

#include <stdint.h>

/* ---- function prototypes / shared state ---- */
int16_t adc_to_q15(uint16_t adc_raw);

#endif /* HB_ADC_Q15_H */
