/* Source: Embedded DSP Handbook, Chapter 1 (The float sibling: denormals in a decaying IIR or AGC state).
 * MIT License.
 * Listing HB[7]; reference only (needs CMSIS core_cm4.h: __get_FPSCR/__set_FPSCR).  NOT built. */
// Flush-to-zero (FZ, bit 24 of FPSCR) makes any denormal RESULT flush to zero instead of
// trapping to the software FP assist. Set it once at startup, before any filter runs:
#include "core_cm4.h"   // or core_cm7.h; arm_math.h pulls in the right one transitively

void enable_fpu_flush_to_zero(void) {
    uint32_t fpscr = __get_FPSCR();
    fpscr |= (1u << 24);      // FZ
    __set_FPSCR(fpscr);
}
