# bench: stage costs and target tests on a Cortex-M4F model

The material behind Appendix A and the Chapter 12 shift example of The Embedded DSP Field Manual.

| File | What it is |
|---|---|
| `bench.c` | Instructions per call of each stage of the chain. Each stage runs 4096 times on a 1 kHz tone at 8 kHz; the count of an empty loop over the same input is subtracted. |
| `shift_check.c` | `q15_mul_trunc(-32768, -32768)`, a saturating version, and `x >> n` against `x / 2^n` for negative `x`. |
| `startup.c`, `shinit.c`, `link.ld` | Vector table, reset handler with the FPU enable, semihosting set-up and the memory map of the QEMU machine mps2-an386. |
| `bench.txt`, `shift_check.txt`, `tests_m4.txt` | The output of the three runs below. |

    make bench    # -> bench.txt
    make shift    # -> shift_check.txt
    make tests    # the seven tests of ../tests on the Cortex-M4F model -> tests_m4.txt

Tools used for the committed output: arm-none-eabi-gcc 13.2.1 20231009 (Arm GNU Toolchain 13.2.rel1, newlib), QEMU 8.2.2.

## What the numbers are

QEMU runs mps2-an386 (a Cortex-M4 with FPU) with `-icount shift=0`, so every instruction advances virtual time by
1 ns. SysTick counts the 25 MHz system clock, so one SysTick count is 40 instructions. The calibration line checks
this: a loop of 2,000,000 instructions counts as 2,000,000. The numbers are instructions, not cycles. On a real
Cortex-M4 every instruction takes at least one cycle, loads and taken branches take more, and flash wait states add
more again, so an instruction count is a lower bound on the cycle count.

## On a board

Build with `make BENCH_DWT=1 bench.elf` and `bench_now()` reads `DWT->CYCCNT`, so the same table gives cycles.
`link.ld` and `startup.c` are for QEMU: on a board use the vendor's linker script and startup file, and print over
the board's UART or SWO in place of semihosting. These numbers have not been measured on a board.
