/* GROUP A: ping-pong DMA handoff (HB[15]).  The book's listing (examples/hb_ping_pong_dma.c) is #included into this translation unit
 * and driven by a small DMA MODEL (test code, not the book's): a circular write pointer over adc_dma_buffer that fires the two HAL
 * callbacks exactly where a circular STM32 DMA would (half-transfer after BLOCK_SIZE samples, transfer-complete after 2*BLOCK_SIZE, then wrap).
 * Claims tested: first half -> block 0, second half -> block 1, no copying (the pointer IS the DMA buffer), no gaps/duplicates when the
 * consumer keeps up, overrun counted and the stale block replaced when it does not, and a callback that fires while the main loop is
 * taking the block is neither lost nor miscounted. */
#define HB_HOST_TEST
#include "../examples/hb_ping_pong_dma.c"
#include "check.h"

/* ---- consumer side: process_block as the book's main loop calls it ----------------------------------------------------------------- */
#define MAXSEEN 64
static const int16_t *seen_ptr[MAXSEEN]; static int16_t seen_first[MAXSEEN]; static int seen_len[MAXSEEN]; static int seen_ramp_ok[MAXSEEN]; static int nseen;
void process_block(const int16_t *block, int len) {
    if (nseen < MAXSEEN) { seen_ptr[nseen] = block; seen_len[nseen] = len; seen_first[nseen] = block[0];
        int ok = 1; for (int i = 1; i < len; i++) if ((int16_t)(block[i] - block[i - 1]) != 1) ok = 0; seen_ramp_ok[nseen] = ok; nseen++; }
}

/* ---- DMA model ---------------------------------------------------------------------------------------------------------------------- */
static ADC_HandleTypeDef hadc;
static int dma_pos = 0; static int16_t sample_counter = 0;
static void dma_write(int count, int (*after_event)(void)) {
    for (int i = 0; i < count; i++) {
        adc_dma_buffer[dma_pos++] = sample_counter++;
        if (dma_pos == BLOCK_SIZE) { HAL_ADC_ConvHalfCpltCallback(&hadc); if (after_event) after_event(); }
        if (dma_pos == 2 * BLOCK_SIZE) { dma_pos = 0; HAL_ADC_ConvCpltCallback(&hadc); if (after_event) after_event(); }
    }
}
static int run_main_loop(void) { main_loop_process_audio(); return 0; }
static void reset_model(void) { dma_pos = 0; sample_counter = 0; nseen = 0; ready_block = NULL; adc_overrun_count = 0; for (int i = 0; i < 2 * BLOCK_SIZE; i++) adc_dma_buffer[i] = 0; }

static int pending_complete, delivered;
static void raise_complete_event(void) { if (hbA_primask) pending_complete = 1; else HAL_ADC_ConvCpltCallback(&hadc); }
static void deliver_pending(void) { if (pending_complete) { pending_complete = 0; delivered++; HAL_ADC_ConvCpltCallback(&hadc); } }

int main(void) {
    CHECK(BLOCK_SIZE == 128, "BLOCK_SIZE = %d (the book's DMA example; Lab 0 uses 256)", BLOCK_SIZE);
    CHECK(sizeof adc_dma_buffer / sizeof adc_dma_buffer[0] == 2 * BLOCK_SIZE, "buffer holds two halves");

    /* --- hand-driven callbacks ----------------------------------------------------------------------------------------------------------- */
    reset_model();
    main_loop_process_audio();
    CHECK(nseen == 0, "nothing ready: process_block must not run");
    HAL_ADC_ConvHalfCpltCallback(&hadc);
    CHECK(ready_block == &adc_dma_buffer[0], "half-complete -> first half");
    main_loop_process_audio();
    CHECK(nseen == 1 && seen_ptr[0] == &adc_dma_buffer[0] && seen_len[0] == BLOCK_SIZE, "first half consumed, BLOCK_SIZE samples, no copy");
    CHECK(ready_block == NULL, "ready_block cleared after consume");
    HAL_ADC_ConvCpltCallback(&hadc);
    CHECK(ready_block == &adc_dma_buffer[BLOCK_SIZE], "complete -> second half");
    main_loop_process_audio();
    CHECK(nseen == 2 && seen_ptr[1] == &adc_dma_buffer[BLOCK_SIZE], "second half consumed");
    CHECK(adc_overrun_count == 0, "no overruns so far");
    main_loop_process_audio();
    CHECK(nseen == 2, "main loop with nothing pending does nothing (no double processing)");

    /* --- overrun policy: 'drop and count': two events, no consume between them ------------------------------------------------------------ */
    HAL_ADC_ConvHalfCpltCallback(&hadc);
    HAL_ADC_ConvCpltCallback(&hadc);
    CHECK(adc_overrun_count == 1, "dropped block counted once: %u", (unsigned)adc_overrun_count);
    CHECK(ready_block == &adc_dma_buffer[BLOCK_SIZE], "stale block silently replaced by the newest");
    main_loop_process_audio();
    CHECK(nseen == 3 && seen_ptr[2] == &adc_dma_buffer[BLOCK_SIZE], "only the newest block is processed (older one dropped, not queued)");
    CHECK(ready_block == NULL, "...and the slot is free again");

    /* --- DMA model, consumer keeps up (main loop runs after every DMA event): contiguous, gap-free, duplicate-free stream ------------------------ */
    reset_model();
    dma_write(10 * BLOCK_SIZE, run_main_loop);
    { int ok = (nseen == 10), gaps = 0, rampbad = 0;
      for (int i = 0; i < nseen; i++) { if (seen_first[i] != (int16_t)(i * BLOCK_SIZE)) gaps++; if (!seen_ramp_ok[i]) rampbad++; if (seen_ptr[i] != &adc_dma_buffer[(i % 2) * BLOCK_SIZE]) ok = 0; }
      CHECK(nseen == 10, "10 half-buffer fills -> %d blocks processed", nseen);
      CHECK(gaps == 0 && rampbad == 0, "stream is contiguous: %d blocks start at the wrong sample, %d blocks contain a discontinuity", gaps, rampbad);
      CHECK(ok, "blocks alternate between the two halves of the DMA buffer");
      CHECK(adc_overrun_count == 0, "no overruns when the consumer keeps up (%u)", (unsigned)adc_overrun_count); }

    /* --- DMA model, consumer stalls for 4 half-buffers: overruns counted, and only the newest block survives ------------------------------------- */
    reset_model();
    dma_write(4 * BLOCK_SIZE, NULL);        /* four events with the main loop not running at all */
    CHECK(adc_overrun_count == 3, "4 events, nothing consumed: %u overruns (3 dropped blocks)", (unsigned)adc_overrun_count);
    main_loop_process_audio();
    CHECK(nseen == 1 && seen_first[0] == (int16_t)(3 * BLOCK_SIZE), "the surviving block is the newest one (samples %d..): %d", 3 * BLOCK_SIZE, nseen ? seen_first[0] : -1);

    /* --- a DMA event arriving while the main loop takes the block ---------------------------------------------------------------------------- */
    /* Model of the interrupt controller: while PRIMASK is set an event stays pending and is delivered when the mask is released.  The
     * event is raised at the instant the main loop masks interrupts, i.e. exactly inside its read-and-clear of ready_block. */
    reset_model();
    HAL_ADC_ConvHalfCpltCallback(&hadc);                 /* first half is ready */
    hbA_primask = 0; hbA_irq_mask_calls = 0;
    hbA_on_irq_masked = raise_complete_event; hbA_on_irq_unmasked = deliver_pending;
    pending_complete = 0; delivered = 0;
    main_loop_process_audio();
    hbA_on_irq_masked = NULL; hbA_on_irq_unmasked = NULL;
    CHECK(hbA_irq_mask_calls == 1, "the read-and-clear runs inside one interrupt-masked section (%d)", hbA_irq_mask_calls);
    CHECK(hbA_primask == 0, "interrupts are unmasked again afterwards");
    CHECK(nseen == 1 && seen_ptr[0] == &adc_dma_buffer[0], "the block that was ready is processed");
    CHECK(delivered == 1 && ready_block == &adc_dma_buffer[BLOCK_SIZE], "the event raised inside the section is delivered afterwards and its block is kept");
    CHECK(adc_overrun_count == 0, "...without a spurious overrun (%u)", (unsigned)adc_overrun_count);
    main_loop_process_audio();
    CHECK(nseen == 2 && seen_ptr[1] == &adc_dma_buffer[BLOCK_SIZE], "the second block is processed on the next pass: nothing was lost");
    /* The previous interrupt state is restored, not forced on: with PRIMASK already set the section must leave it set. */
    reset_model(); HAL_ADC_ConvHalfCpltCallback(&hadc);
    hbA_primask = 1;
    main_loop_process_audio();
    CHECK(hbA_primask == 1, "PRIMASK that was already set before the call is left set");
    hbA_primask = 0;
    TEST_DONE("test_A_dma");
}
