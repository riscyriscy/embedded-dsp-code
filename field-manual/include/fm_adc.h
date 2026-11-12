/* Raw 12-bit ADC counts -> Q15.
 * Source: Field Manual, Chapter 1 ("From raw ADC counts to Q15"). */
#ifndef FM_ADC_H
#define FM_ADC_H

#include <stdint.h>

/* ---- function prototypes / shared state ---- */
int16_t adc_to_q15(uint16_t raw12);

#endif /* FM_ADC_H */
