# The Embedded DSP Field Manual: code

Host-buildable C for every listing in *The Embedded DSP Field Manual* (C. Paul Gordon, Embedded Think). Plain C11, no dependencies beyond a C compiler and `make`
(`python3` with `numpy` and `scipy` only for `make check-generated`).

```
make test            # build and run all 7 test programs, no hardware needed
make check-generated # regenerate coefficient/table headers from tools/*.py and diff
make sanitize        # same tests under AddressSanitizer + UBSan
```

| Directory | Contents |
|---|---|
| `include/`, `src/` | The book's library code: Q-format macros, ADC to Q15, FIR, biquad, simple filters, CIC, Goertzel, NCO and mixer, QPSK, AGC, timing, Costas, loops, lock, FEC, BER, RF helpers, the loopback chain. |
| `examples/` | `lab0.c` (Lab 0), `ping_pong_dma.c` (double-buffer DMA pattern) and `loopback_init.c` (the Chapter 10 reset values and Chapter 6 Costas gains). |
| `tests/` | Host tests with golden vectors. |
| `tools/` | The book's Python design scripts (FIR, RRC, NCO table). |
| `bench/` | Appendix A: instructions per call of every stage on a Cortex-M4F model (QEMU mps2-an386), the Chapter 12 shift and Q15 product check, and the seven tests built for the Cortex-M4F. Needs `arm-none-eabi-gcc` and `qemu-system-arm`; see `bench/README.md`. |

## Layout notes

* Each chapter's listings are split into a header (`include/fm_*.h`: types, macros, prototypes) and a source file (`src/fm_*.c`: function bodies), with the standard includes added.
* The coefficient and table headers in `include/generated/` are the output of `tools/design_filter.py`, `design_rrc.py` and `design_nco_table.py`; `make check-generated` regenerates them and diffs against the committed copies.
* `examples/ping_pong_dma.c` is reference code for a real board; on the host it is built against `tests/shims/stm32_hal_stub.h`. `tests/shims/arm_math.h` is a naive implementation of the few CMSIS-DSP calls Lab 0 uses, so `examples/lab0.c` runs on the host. Neither is CMSIS.
* The Chapter 9 encoder uses masks `0x4F` and `0x6D`: the register shifts the newest bit in at bit 0, so these are the 171/133 octal generators written bit-reversed.
* `LLR_SCALE_Q` (Chapter 9) is 16384, i.e. 0.5 in Q15.

## Test results (as last run)

| Program | Checks | Covers |
|---|---|---|
| `test_filters` | 93 | FIR incl. the Chapter 2 golden vector, Q-format, biquad, simple filters, ADC conversion (all 4096 codes), DC blocker (either sign of DC, zero-mean input, double-precision reference), Goertzel, CIC (bit-exact vs boxcar cubed; a DC input that runs past 2^32 with every integrator checked against its exact running sum), interpolation |
| `test_nco_qpsk` | 614 | NCO, mixer, QPSK map and slice, AGC (step units, closed-loop settling), timing, Costas detector, Q16 gain macros and a 64-bit loop pull-in against the linear model, lock detector |
| `test_lab0` | 5 | Lab 0 peak at the expected bin (against a naive-DFT shim) |
| `test_fec` | 11 | Convolutional encoder against the 171/133 octal definition, impulse response, golden vector; ACS step in a harness decoder |
| `test_dma` | 9 | Ping-pong buffer hand-off |
| `test_chain` | 25 | Full loopback receiver on a generated 300-symbol signal with a 45 Hz offset (plus a sweep), AGC and Costas gains |
| `test_vectors` | the book's own harness | "ALL VECTORS OK" |

All 7 pass with `-Wall -Wextra -Wpedantic` and zero warnings, and under ASan+UBSan. `make test` also builds `src/fm_cic.c` with `-Wconversion -Werror` (`make conversion`), so an unwritten narrowing in the CIC stops the build. "Pass" means the code does what **this repo's tests say**, not that it was validated on hardware.

## What is not tested

Vendor HAL material and CMSIS calls are reference-only. The stage costs in `bench/` are instruction counts on a QEMU model, not cycles measured on a board. The loopback fixture is ours (the book does not print its test signal), so `test_chain` is an algorithm check, not a bit-exact vector: on it the Costas loop pulls in the 45 Hz offset and the decoded stream matches the transmitted symbols with no bit errors from about symbol 180 on (the test scores from the receiver's 250th decision to the last transmitted symbol, which after the 9-symbol filter delay is transmitted symbols 241 to 299, 59 symbols: 0 errors in 118 bits).
