/* HB[94] (Chapter 12): tag_backscatter_bit() -- a conceptual, HAL-dependent listing.  It cannot run on a host without the
 * antenna-load driver that is not part of the listing, so this is status "syntax-only": the listing is compiled verbatim (via the
 * TEST-ONLY shim tests/shims/hbD_hb094_tag_backscatter.inc) against a recording stub for set_antenna_load / LOAD_STATE_*, and the
 * test checks only the control flow the listing itself describes (bit 1 -> HIGH load, bit 0 -> LOW load, exactly one call).
 * Nothing about RF behaviour is (or can be) tested here.
 * The 847 kHz subcarrier arithmetic in the listing's comment is checked too: 13.56 MHz / 16 = 847.5 kHz. */
#include <stdint.h>
#include "check.h"

/* ---- TEST-ONLY HAL stub (names taken from the listing) ---- */
enum { LOAD_STATE_LOW = 100, LOAD_STATE_HIGH = 200 };
static int last_load = -1, load_calls = 0;
static void set_antenna_load(int state) { last_load = state; load_calls++; }

#include "shims/hbD_hb094_tag_backscatter.inc"

int main(void) {
    load_calls = 0; tag_backscatter_bit(1);
    CHECK(load_calls == 1 && last_load == LOAD_STATE_HIGH, "bit 1 -> one call, LOAD_STATE_HIGH (got %d calls, state %d)", load_calls, last_load);
    load_calls = 0; tag_backscatter_bit(0);
    CHECK(load_calls == 1 && last_load == LOAD_STATE_LOW, "bit 0 -> one call, LOAD_STATE_LOW");
    load_calls = 0; tag_backscatter_bit(0x80);
    CHECK(load_calls == 1 && last_load == LOAD_STATE_HIGH, "any non-zero value counts as 1 (listing uses 'if (bit)')");
    CHECK_NEAR(13.56e6 / 16.0, 847.5e3, 1e-6, "ISO 14443 subcarrier: carrier / 16 = 847.5 kHz (prose, and the comment's '847 kHz')");
    TEST_DONE("test_D_snippets");
}
