/* Ping-pong DMA handoff (Chapter 1): drives the two HAL callbacks by hand, as DMA would. */
#include "../examples/ping_pong_dma.c"
#include "check.h"

static const int16_t *g_seen[8]; static int g_nseen; static int g_lastlen;
void process_block(const int16_t *block, int len) { if (g_nseen < 8) g_seen[g_nseen++] = block; g_lastlen = len; }

int main(void) {
    ADC_HandleTypeDef h = { 0 };
    main_loop_process_audio();
    CHECK(g_nseen == 0, "nothing ready: process_block must not run");
    HAL_ADC_ConvHalfCpltCallback(&h);
    CHECK(ready_block == &adc_dma_buffer[0], "half-complete -> first half");
    main_loop_process_audio();
    CHECK(g_nseen == 1 && g_seen[0] == &adc_dma_buffer[0] && g_lastlen == BLOCK_SIZE, "first half consumed with BLOCK_SIZE samples");
    CHECK(ready_block == NULL, "ready_block cleared after consume");
    HAL_ADC_ConvCpltCallback(&h);
    main_loop_process_audio();
    CHECK(g_nseen == 2 && g_seen[1] == &adc_dma_buffer[BLOCK_SIZE], "complete -> second half");
    CHECK(adc_overrun_count == 0, "no overruns so far");
    /* Overrun: two events with no consume in between -> counted, and the newer block wins. */
    HAL_ADC_ConvHalfCpltCallback(&h);
    HAL_ADC_ConvCpltCallback(&h);
    CHECK(adc_overrun_count == 1, "dropped block counted: %u", (unsigned)adc_overrun_count);
    CHECK(ready_block == &adc_dma_buffer[BLOCK_SIZE], "newest block kept");
    CHECK(sizeof adc_dma_buffer / sizeof adc_dma_buffer[0] == 2 * BLOCK_SIZE, "buffer holds two halves");
    TEST_DONE("test_dma");
}
