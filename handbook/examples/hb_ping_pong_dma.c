/* Source: Embedded DSP Handbook, Chapter 4 (The double-buffer (ping-pong) DMA pattern, in code).
 * MIT License.
 * Host build (-DHB_HOST_TEST): ADC_HandleTypeDef and the CMSIS interrupt-mask intrinsics come from
 * tests/shims/hbA_stm32_hal_stub.h; on a target include your HAL header instead.  process_block() is
 * defined by Lab 0 (hb_lab0.c).
 * The host test is tests/test_A_dma.c, which includes this file and drives the two callbacks by hand. */
#include <stdint.h>
#include <stddef.h>
#ifdef HB_HOST_TEST   /* host build */
#include "hbA_stm32_hal_stub.h"   /* ADC_HandleTypeDef and PRIMASK stubs */
#endif
void process_block(const int16_t *block, int len);   /* defined in Lab 0 (hb_lab0.c) */

#define BLOCK_SIZE 128
static int16_t adc_dma_buffer[2 * BLOCK_SIZE];  // circular DMA target, two halves

volatile int16_t *ready_block = NULL;   // set by the ISR, consumed by the main loop
volatile uint32_t adc_overrun_count = 0;  // bumped whenever a block is dropped below

// DMA half-transfer callback: fires when the DMA has just finished filling the FIRST half,
// meaning the DMA is now writing into the second half and the first half is safe to read.
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    if (ready_block != NULL) adc_overrun_count++;  // main loop hasn't consumed the last block
    ready_block = &adc_dma_buffer[0];
}

// DMA transfer-complete callback: fires when the DMA has just finished filling the SECOND
// half (and, since the DMA is circular, has wrapped back to refilling the first half).
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (ready_block != NULL) adc_overrun_count++;  // same race, other half
    ready_block = &adc_dma_buffer[BLOCK_SIZE];
}

// Main loop: process whichever half was last marked ready, while the DMA silently fills
// the other half in the background. No copying, no gaps, no samples dropped between blocks.
void main_loop_process_audio(void) {
    if (ready_block != NULL) {
        // Read-and-clear with interrupts masked: a callback landing between
        // the two statements would have its block overwritten by NULL,
        // neither processed nor counted.
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        int16_t *block = (int16_t *)ready_block;
        ready_block = NULL;
        __set_PRIMASK(primask);
        process_block(block, BLOCK_SIZE);   // your filter/demodulator chain, Chapters 2-12
    }
}
