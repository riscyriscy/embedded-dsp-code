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
