# Field Manual stage costs and target tests on a Cortex-M4F model (QEMU mps2-an386).
# Needs arm-none-eabi-gcc with newlib (rdimon.specs) and qemu-system-arm.
#   make bench    instructions per call of each stage -> bench.txt
#   make shift    the two Q15 edge cases of Chapter 12 -> shift_check.txt
#   make tests    the seven tests of ../tests built for the Cortex-M4F -> tests_m4.txt
#   make BENCH_DWT=1 bench.elf   the same bench reading DWT->CYCCNT, for a board (link.ld is for QEMU)
CC     := arm-none-eabi-gcc
ARCH   := -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS := $(ARCH) -O2 -std=c11 -Wall -Wextra -Wno-unused-parameter -D_DEFAULT_SOURCE \
          -I../include -I../include/generated -I../tests -I../tests/shims $(if $(BENCH_DWT),-DBENCH_DWT)
LDFLAGS:= -T link.ld -nostartfiles --specs=rdimon.specs -Wl,--gc-sections
LDLIBS := -lm
QEMU   := qemu-system-arm -M mps2-an386 -nographic -semihosting-config enable=on,target=native
LIB    := $(wildcard ../src/*.c)
TESTS  := $(patsubst ../tests/%.c,%.elf,$(wildcard ../tests/test_*.c))

.PHONY: bench shift tests clean
bench: bench.elf
	$(QEMU) -icount shift=0 -kernel $< | tee bench.txt
shift: shift_check.elf
	$(QEMU) -icount shift=0 -kernel $< | tee shift_check.txt
tests: $(TESTS)
	@rm -f tests_m4.txt; for t in $(TESTS); do $(QEMU) -icount shift=auto -kernel $$t | grep -E "checks|VECTORS" | tee -a tests_m4.txt; done

bench.elf: bench.c startup.c ../examples/loopback_init.c $(LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@
shift_check.elf: shift_check.c startup.c shinit.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@
test_%.elf: ../tests/test_%.c startup.c shinit.c $(LIB)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@
clean:
	rm -f *.elf
