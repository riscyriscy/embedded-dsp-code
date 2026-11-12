/*
 * Embedded DSP Field Manual, Chapter 1: Sampling, Q-Format, and DMA / The double-buffer (ping-pong) DMA pattern
 * Listing 4 of 57 (index 3, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: examples/ping_pong_dma.c | Checked by: tests/test_dma.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
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
