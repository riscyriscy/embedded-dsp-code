# Embedded DSP Field Manual, Chapter 5: I/Q and the Demod Chain / Exporting the RRC taps: design_rrc.py
# Listing 30 of 61 (index 29, counting from 0 in book order), exactly as it appears in the book.
# Status: not-code | Built as: tools/design_rrc.py | Checked by: Python tool; regeneration checked by `make check-generated`
# MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
# design_rrc.py - run once offline, whenever alpha or sps changes
import numpy as np

def rrc_taps(alpha, sps, span_symbols):
    n = np.arange(-span_symbols * sps, span_symbols * sps + 1)
    t = n / sps                                    # time in symbol periods
    h = np.zeros_like(t, dtype=float)
    for i, ti in enumerate(t):
        if abs(ti) < 1e-8:                                       # t = 0
            h[i] = 1 - alpha + 4 * alpha / np.pi
        elif abs(abs(4 * alpha * ti) - 1) < 1e-8:                 # t = +-Ts/(4*alpha)
            h[i] = (alpha / np.sqrt(2)) * (
                (1 + 2 / np.pi) * np.sin(np.pi / (4 * alpha)) +
                (1 - 2 / np.pi) * np.cos(np.pi / (4 * alpha)))
        else:
            num = np.sin(np.pi * ti * (1 - alpha)) + 4 * alpha * ti * np.cos(np.pi * ti * (1 + alpha))
            den = np.pi * ti * (1 - (4 * alpha * ti) ** 2)
            h[i] = num / den
    return h / np.sqrt(np.sum(h ** 2))              # unit energy - keeps the Q15 export below simple

# Chapter 10's loopback runs the matched filter directly at 2 sps (see above) - deploy taps built
# for that rate, not the 8-sps design/eye-diagram check below.
taps = rrc_taps(alpha=0.35, sps=2, span_symbols=4.5)   # 19 taps, matching this book's shared FIR_TAPS
q15 = np.clip(np.round(taps * 32768), -32768, 32767).astype(np.int16)

with open("rrc_coeffs.h", "w") as f:
    f.write("static const int16_t rrc_coeffs_q15[FIR_TAPS] = {\n    ")
    f.write(", ".join(str(c) for c in q15))
    f.write("\n};\n")
