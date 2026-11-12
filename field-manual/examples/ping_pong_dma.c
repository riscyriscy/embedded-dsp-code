/* Ping-pong (double-buffer) DMA pattern. Source: Field Manual, Chapter 1.
 * On a real STM32 target, replace stm32_hal_stub.h with your HAL header
 * and call process_block() from Lab 0 (lab0.c). The host test (tests/test_dma.c) drives the
 * two callbacks by hand to simulate half-complete / complete DMA events. */
#include <stdint.h>
#include <stddef.h>
#include "stm32_hal_stub.h"   /* host build: stub for ADC_HandleTypeDef; use your HAL's on target */
void process_block(const int16_t *block, int len);   /* defined in lab0.c */

#define BLOCK_SIZE 256                       // matches Lab 0's LAB0_N below
static int16_t adc_dma_buffer[2 * BLOCK_SIZE];  // circular DMA target, two halves
volatile int16_t *ready_block = NULL;            // set by the ISR, consumed by the main loop
volatile uint32_t adc_overrun_count = 0;         // bumped whenever a block is dropped below

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    if (ready_block != NULL) adc_overrun_count++;  // main loop hasn't consumed the last block - drop it
    ready_block = &adc_dma_buffer[0];             // DMA just finished the first half
}
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (ready_block != NULL) adc_overrun_count++;  // same race, other half
    ready_block = &adc_dma_buffer[BLOCK_SIZE];     // DMA just finished the second half
}

void main_loop_process_audio(void) {
    if (ready_block != NULL) {
        int16_t *block = (int16_t *)ready_block;
        ready_block = NULL;
        process_block(block, BLOCK_SIZE);          // your chain - the rest of this book
    }
}
