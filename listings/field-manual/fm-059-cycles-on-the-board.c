/*
 * Embedded DSP Field Manual, Appendix A: Reference Board and Stage Costs / Cycles on the board
 * Listing 60 of 61 (index 59, counting from 0 in book order), exactly as it appears in the book.
 * Status: reference-only | Built as: (not built) | Checked by: CMSIS core register names; needs a board. bench/bench.c with BENCH_DWT does the same with raw register addresses
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;   // trace on: the DWT is clocked
DWT->CYCCNT = 0;
DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;              // start the cycle counter

uint32_t t0 = DWT->CYCCNT;
/* the stage under test */
uint32_t cycles = DWT->CYCCNT - t0;               // unsigned: right across one wrap
