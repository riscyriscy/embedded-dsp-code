/*
 * Embedded DSP Handbook, Chapter 1: DSP Refresher for Embedded Systems / The float sibling: denormals in a decaying IIR or AGC state
 * Listing 8 of 119 (index 7, counting from 0 in book order), exactly as it appears in the book.
 * Status: reference-only | Built as: reference/hb_fpu_flush_to_zero.c | Checked by: CMSIS core_cm4.h (__get_FPSCR/__set_FPSCR); the premise (denormals in a decaying biquad, FZ flushes them) is exercised on the host FPU (x86 MXCSR.FTZ analogue) in tests/test_A_filters.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Flush-to-zero (FZ, bit 24 of FPSCR) makes any denormal RESULT flush to zero instead of
// trapping to the software FP assist. Set it once at startup, before any filter runs:
#include "core_cm4.h"   // or core_cm7.h; arm_math.h pulls in the right one transitively

void enable_fpu_flush_to_zero(void) {
    uint32_t fpscr = __get_FPSCR();
    fpscr |= (1u << 24);      // FZ
    __set_FPSCR(fpscr);
}
