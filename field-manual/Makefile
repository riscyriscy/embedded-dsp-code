# Field Manual host tests.  `make test` builds and runs everything that is host-testable.
CC      ?= cc
CFLAGS  ?= -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wno-unused-parameter -D_DEFAULT_SOURCE
INC     := -Iinclude -Iinclude/generated -Itests -Itests/shims
LDLIBS  := -lm
BUILD   := build

LIB_SRCS := $(wildcard src/*.c)
TESTS    := $(patsubst tests/%.c,$(BUILD)/%,$(wildcard tests/test_*.c))

.PHONY: all test clean
all: $(TESTS)

$(BUILD)/%: tests/%.c $(LIB_SRCS) | $(BUILD)
	$(CC) $(CFLAGS) $(INC) $< $(LIB_SRCS) -o $@ $(LDLIBS)

$(BUILD):
	mkdir -p $(BUILD)

test: $(TESTS)
	@fail=0; for t in $(TESTS); do ./$$t || fail=1; done; \
	  if [ $$fail -ne 0 ]; then echo "SOME TESTS FAILED"; exit 1; else echo "ALL TESTS PASSED"; fi

clean:
	rm -rf $(BUILD)

# Regenerate the coefficient/table headers into a scratch dir and diff against the committed copies.
# Needs python3 with numpy and scipy.
.PHONY: check-generated
check-generated:
	@d=$$(mktemp -d); abs=$$(pwd); \
	for t in design_filter design_nco_table design_rrc; do (cd $$d && python3 $$abs/tools/$$t.py) || exit 1; done; \
	for h in coeffs nco_table rrc_coeffs; do diff -u include/generated/$$h.h $$d/$$h.h || { echo "MISMATCH: $$h.h"; exit 1; }; done; \
	echo "generated headers match tools/*.py output"; rm -rf $$d

# Same tests under AddressSanitizer + UBSan (shift-base included).
.PHONY: sanitize
sanitize:
	$(MAKE) clean
	$(MAKE) test CFLAGS="-std=c11 -O1 -g -D_DEFAULT_SOURCE -fsanitize=address,undefined -fno-sanitize-recover=undefined"
	$(MAKE) clean
