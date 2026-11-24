# Handbook host tests.  `make test` builds and runs every tests/test_*.c.
# Each test lists the library sources it links in tests/<name>.deps (one line, space separated, paths
# relative to this directory).  Units are separate translation units on purpose: the Handbook reuses
# names with different types in different chapters (FIR_TAPS is 32 float in Chapter 2 but 19 Q15 in
# Chapter 5), so they must never be linked into one image.
CC      ?= cc
CFLAGS  ?= -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wno-unused-parameter -D_DEFAULT_SOURCE
INC     := -Iinclude -Iinclude/generated -Itests -Itests/shims
LDLIBS  := -lm
BUILD   := build

TESTS := $(patsubst tests/%.c,$(BUILD)/%,$(wildcard tests/test_*.c))

.PHONY: all test clean check-generated
all: $(TESTS)

.SECONDEXPANSION:
$(BUILD)/%: tests/%.c $$(shell cat tests/$$*.deps 2>/dev/null) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $< $$(cat tests/$*.deps 2>/dev/null) -o $@ $(LDLIBS)

$(BUILD):
	mkdir -p $(BUILD)

test: $(TESTS)
	@fail=0; for t in $(TESTS); do ./$$t || fail=1; done; \
	  if [ $$fail -ne 0 ]; then echo "SOME TESTS FAILED"; exit 1; else echo "ALL TESTS PASSED"; fi

clean:
	rm -rf $(BUILD)

# Regenerate the coefficient/table headers into a scratch dir and diff against the committed copies.
# Needs python3 with numpy and scipy.  (hilbert_coeffs.h and farrow_weights.h are generated; the book elides these tables.)
check-generated:
	@d=$$(mktemp -d); abs=$$(pwd); \
	(cd $$d && python3 $$abs/tools/design_filter.py) || exit 1; \
	python3 tools/design_hilbert.py $$d/hilbert_coeffs.h || exit 1; \
	python3 tools/gen_farrow_weights.py > $$d/farrow_weights.h || exit 1; \
	for h in coeffs hilbert_coeffs farrow_weights; do diff -u include/generated/$$h.h $$d/$$h.h || { echo "MISMATCH: $$h.h"; exit 1; }; done; \
	python3 tools/hbA_check_coeffs.py || exit 1; \
	echo "generated headers match tools/*.py output"; rm -rf $$d

# Same tests under AddressSanitizer + UBSan (shift-base and signed-integer-overflow included).
.PHONY: sanitize
sanitize:
	$(MAKE) clean
	$(MAKE) test CFLAGS="-std=c11 -O1 -g -D_DEFAULT_SOURCE -fsanitize=address,undefined -fno-sanitize-recover=undefined"
	$(MAKE) clean
