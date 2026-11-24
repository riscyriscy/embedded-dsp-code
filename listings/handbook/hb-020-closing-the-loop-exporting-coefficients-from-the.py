# Embedded DSP Handbook, Chapter 5: Fixed-Point Design Workflow / Closing the loop: exporting coefficients from the design tool to C
# Listing 21 of 119 (index 20, counting from 0 in book order), exactly as it appears in the book.
# Status: not-code | Built as: tools/design_filter.py | Checked by: Python tool (verbatim, foundation); regeneration verified by tools/hbA_check_coeffs.py: output identical to include/generated/coeffs.h and to the values in HB[21]
# MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
# design_filter.py - run once offline, whenever the filter spec changes
import scipy.signal as sig
import numpy as np

taps = sig.remez(19, [0, 1000, 2000, 4000], [1, 0], fs=8000)  # this chapter's own 1kHz/2kHz/8kHz spec
q15 = np.clip(np.round(taps * 32768), -32768, 32767).astype(np.int16)  # Chapter 1's FLOAT_TO_Q15

with open("coeffs.h", "w") as f:
    f.write(f"#define FIR_TAPS {len(q15)}\n")
    f.write("static const int16_t fir_coeffs_q15[FIR_TAPS] = {\n    ")
    f.write(", ".join(str(c) for c in q15))
    f.write("\n};\n")
