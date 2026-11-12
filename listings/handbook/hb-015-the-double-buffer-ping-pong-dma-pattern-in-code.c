/*
 * Embedded DSP Handbook, Chapter 1: DSP Refresher for Embedded Systems / The double-buffer (ping-pong) DMA pattern, in code
 * Listing 16 of 119 (index 15, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: examples/hb_ping_pong_dma.c tests/shims/hbA_stm32_hal_stub.h | Checked by: tests/test_A_dma.c (20 checks): callbacks, no-copy pointers, gap-free stream via a DMA model, overrun drop-and-count, hand-off inside a masked section
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
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
        process_block(block, BLOCK_SIZE);   // your filter/demodulator chain, Chapters 1-9
    }
}
