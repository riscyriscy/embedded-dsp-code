# The Embedded DSP Handbook: code

Host-buildable C for the listings of *The Embedded DSP Handbook* (C. Paul Gordon, Embedded Think). C11; needs a C compiler and `make`
(`python3` with `numpy` and `scipy` only for `make check-generated`).

```
make test            # 49 test programs, no hardware needed
make check-generated # regenerate coefficient headers from tools/*.py and diff
make sanitize        # same tests under AddressSanitizer + UBSan
```

## Layout

| Directory | Contents |
|---|---|
| `include/`, `src/` | One module per topic: Q-format, FIR (float and Q15), biquad, NCO, FFT (float and Q15), Goertzel, SDFT, CIC, polyphase, Hilbert, QPSK/QAM/DQPSK, AGC, timing, Costas, LMS, OFDM, DSSS, OOK, BLE, FEC, test kit, loopback. |
| `examples/` | `hb_lab0.c`, `hb_ping_pong_dma.c`, `nco_fixed_vs_float.c` (one hour of a 1 kHz tone through uint32, float32 and float64 phase accumulators, the Chapter 9 fixed-point versus floating-point comparison), `nco_demo.c` (the Chapter 9 NCO-in-detail example: 32-bit accumulator, one Q15 cosine table, optional linear interpolation; build with `-DTBL_BITS=6/8/10/12`; the listing is HB[50] in `listings/INDEX.md`) |
| `tests/` | Host tests, grouped A to E by chapter range (`test_A_*` Ch 1-3, `test_B_*` Ch 4-6, `test_C_*` Ch 7-9, `test_D_*` Ch 10-13 and 15-19, `test_E_chain` Ch 19 loopback). Each lists its source files in `tests/<name>.deps`. `test_C_costas_macros_match_fm` compiles the Field Manual's `fm_loops.h` in its own translation unit and checks that its `COSTAS_KP_Q16`/`COSTAS_KI_Q16` give the same integers as the Handbook's for the same arguments, so the repository must keep `handbook/` and `field-manual/` side by side. |
| `reference/` | Vendor HAL listings and snippet includes: archived, not built on their own. |
| `tools/` | Python design scripts (FIR, Hilbert, RRC, Farrow weights), and `nco_measure.py`, which measures the NCO's SFDR from builds of `examples/nco_demo.c` (needs numpy; not part of `make test`). `fir_vs_iir.py` designs one lowpass spec as FIR and as IIR and prints the Chapter 2 FIR-or-IIR comparison (needs numpy and scipy; about a minute; not part of `make test`). `iq_waveforms.py <outdir>` regenerates the four constellation and I/Q waveform figures and the PAPR table in Chapter 9 (needs numpy and matplotlib; not part of `make test`). |
| `../listings/INDEX.md` | Per-listing status and the test that checks it. |

## Why units are separate translation units

The book reuses names with different types in different chapters (`FIR_TAPS` is 32 for the float filter in Chapter 2 and 19 for the Q15 filter in Chapter 5),
and `hb_nco.h` holds file-static tables. Each test therefore links only the sources it needs (`tests/*.deps`) and the modules are not meant to be linked into one image together.

The test files `tests/test_C_ch7_*`, `test_C_ch8_*` and `test_C_ch9_*` keep the chapter numbers in their names from before the book split its Chapter 1 into four chapters (the book now has 22 chapters). The comments inside them, and every other chapter reference in this repository, use the current numbers.

## Status

49 test programs, all passing, no compiler warnings, clean under ASan+UBSan with every check enabled. See `../listings/INDEX.md` for the status of each of the 119 listings
(79 tested, 1 compiled, 5 syntax-only, 8 reference-only, 26 not-code). "Tested" means a host test that was run and passed; it does not mean hardware-validated.

## What the tests do and do not show

Where a listing has a closed form or a standard definition (a DFT, a CRC, the BLE whitening register, the 171/133 convolutional code, CIC bit growth), the test compares against an independent
implementation of that definition. The receiver-loop tests (AGC, Gardner, Costas, the Chapter 22 loopback) run on signals generated inside the tests, because the book does not list its test
signals; they show the loops behave as the text describes on those signals, not on hardware or on any other waveform. In the Chapter 22 sweep the +60 Hz row is reported but its bit-error
count is not asserted: with the true decision amplitude the loop is still pulling in at symbol 300 (it reaches 2 errors in a 400-symbol run).
