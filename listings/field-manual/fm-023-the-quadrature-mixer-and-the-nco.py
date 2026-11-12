# Embedded DSP Field Manual, Chapter 5: I/Q and the Demod Chain / The quadrature mixer and the NCO
# Listing 24 of 57 (index 23, counting from 0 in book order), exactly as it appears in the book.
# Status: not-code | Built as: tools/design_nco_table.py | Checked by: Python tool; regeneration checked by `make check-generated`
# MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
# design_nco_table.py - run once offline; NCO_TABLE_SIZE must match the #define below
import numpy as np

N = 256
angles = 2 * np.pi * np.arange(N) / N
cos_q15 = np.clip(np.round(32767 * np.cos(angles)), -32768, 32767).astype(np.int16)
sin_q15 = np.clip(np.round(32767 * np.sin(angles)), -32768, 32767).astype(np.int16)

with open("nco_table.h", "w") as f:
    f.write(f"#define NCO_TABLE_SIZE {N}\n")
    for name, table in [("cos_table", cos_q15), ("sin_table", sin_q15)]:
        f.write(f"static const int16_t {name}[NCO_TABLE_SIZE] = {{\n    ")
        f.write(", ".join(str(v) for v in table))
        f.write("\n};\n")
