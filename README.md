# embedded-dsp-code

Companion code for two books by C. Paul Gordon (Embedded Think):

- **The Embedded DSP Handbook**: `handbook/`
- **The Embedded DSP Field Manual**: `field-manual/`

Repository: `https://github.com/<your-github-user>/embedded-dsp-code` (replace `<your-github-user>` with your account name in this file and wherever the books refer to it).

## What is here

Every code listing in both books, extracted verbatim, split into headers and sources, built on a host machine, and tested against golden vectors
and independent references where that is possible. Nothing needs hardware.

```
git clone https://github.com/<your-github-user>/embedded-dsp-code
cd embedded-dsp-code
make -C field-manual test     # 7 test programs
make -C handbook test         # 49 test programs
```

Needs a C11 compiler and `make`. `make check-generated` (coefficient tables regenerated from the Python tools and diffed) also needs `python3` with `numpy` and `scipy`.
`make sanitize` runs the same tests under AddressSanitizer and UBSan. All three targets are run by CI (`.github/workflows/ci.yml`).

| Path | Contents |
|---|---|
| `field-manual/`, `handbook/` | Library code (`include/`, `src/`), `examples/`, `tests/`, `tools/` (Python design scripts), and a per-book README |
| `listings/` | The complete archive of the code listings, verbatim, with provenance headers; `listings/INDEX.md` gives the status of each one |

## How to read the status of a listing

| Status | Meaning |
|---|---|
| tested | Built and checked by a host test that was run and passed |
| compiled | Builds clean; no behavioural test |
| syntax-only | Compiles only inside a harness, or relies on definitions the book never prints |
| reference-only | Vendor HAL, CMSIS or AT-command material: archived, not built |
| not-code | Formula, worksheet, procedure, or a Python design tool |

Counts: Field Manual, 57 listings (34 tested, 1 compiled, 21 not-code, 1 reference-only). Handbook, 119 listings (79 tested, 1 compiled, 5 syntax-only, 8 reference-only, 26 not-code).
"Tested" means passed on a host with the tests in this repository; it does not mean validated on hardware or against a real over-the-air signal.

## What the tests show, and what they do not

The code in `src/` and `include/` is the code printed in the books. Where a listing has a closed form or a standard definition (a DFT, a CRC, the BLE whitening register, the 171/133
convolutional code, CIC bit growth), the tests compare against an independent implementation of that definition. The receiver-loop tests (AGC, Gardner, Costas, the loopback chains) run on
signals generated inside the tests, because the books do not list their test signals; they show that the loops behave as the text describes on those signals, not on hardware or on any other
waveform. Verify gains and loop behaviour on your own signals before relying on them.

## License

MIT, see `LICENSE`. Copyright (c) 2026 C. Paul Gordon.
