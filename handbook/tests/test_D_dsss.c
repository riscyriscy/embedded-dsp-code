/* Chapter 11 (Spread Spectrum), HB[91] 4-bit LFSR + spread/despread, HB[92] early-late DLL error (src/hb_dsss.c).
 * HB[90] (processing gain = 10 log10(chip_rate/bit_rate)) is a formula, checked in tests/test_D_formulas.c.
 * References: the LFSR recurrence derived on paper (right-shift register, new MSB = b0 ^ b3  =>  o[t+4] = o[t] ^ o[t+3],
 * i.e. x^4 + x^3 + 1, which is primitive, so the period must be 15); the m-sequence autocorrelation property (-1 at every
 * non-zero shift); a correlation computed from the definition. */
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "hb_dsss.h"

int main(void) {
    /* ---- the generator against the recurrence o[t+4] = o[t] ^ o[t+3] ---- */
    for (int seed = 1; seed < 16; seed++) {
        lfsr_t l = { (uint8_t)seed };
        uint8_t o[64];
        for (int t = 0; t < 64; t++) o[t] = lfsr_next_chip(&l);
        int bad = 0;
        for (int t = 0; t + 4 < 64; t++) if (o[t + 4] != (o[t] ^ o[t + 3])) bad++;
        CHECK(bad == 0, "seed %d: output violates o[t+4] = o[t]^o[t+3] at %d places", seed, bad);
        /* the first four output bits are the register bits b0, b1, b2, b3 of the seed */
        CHECK(o[0] == (seed & 1) && o[1] == ((seed >> 1) & 1) && o[2] == ((seed >> 2) & 1) && o[3] == ((seed >> 3) & 1), "seed %d: first outputs are the seed bits LSB first", seed);
        /* period exactly 15 and 8 ones / 7 zeros per period (maximal-length: prose says 2^n - 1 = 15) */
        int period = 0;
        for (int p = 1; p <= 30 && !period; p++) { int same = 1; for (int t = 0; t + p < 64; t++) if (o[t] != o[t + p]) same = 0; if (same) period = p; }
        CHECK(period == 15, "seed %d: period %d, want 15 (m-sequence of a 4-bit register)", seed, period);
        int ones = 0; for (int t = 0; t < 15; t++) ones += o[t];
        CHECK(ones == 8, "seed %d: %d ones per period, want 8", seed, ones);
        /* periodic autocorrelation of the +-1 chips: 15 at lag 0, -1 at every other lag */
        int ok = 1;
        for (int lag = 0; lag < 15; lag++) {
            int r = 0; for (int t = 0; t < 15; t++) r += (o[t] ? -1 : 1) * (o[t + lag] ? -1 : 1);
            if (r != (lag == 0 ? 15 : -1)) ok = 0;
        }
        CHECK(ok, "seed %d: autocorrelation is 15 / -1 / -1 ... (m-sequence two-level property)", seed);
    }
    /* the all-zero state is a lock-up state (not a sequence): seed 0 never leaves it */
    { lfsr_t l = { 0 }; int ones = 0; for (int t = 0; t < 100; t++) ones += lfsr_next_chip(&l); CHECK(ones == 0 && l.state == 0, "seed 0 locks up: all-zero output forever"); }
    /* the register stays a 4-bit value */
    { lfsr_t l = { 1 }; int ok = 1; for (int t = 0; t < 100; t++) { lfsr_next_chip(&l); if (l.state > 15) ok = 0; } CHECK(ok, "state stays within 4 bits"); }

    /* ---- spread / despread ---- */
    for (int ratio = 15; ratio <= 255; ratio += 120 /* 15, 135, 255 */) {
        for (int bit = 0; bit < 2; bit++) {
            lfsr_t tx = { 0x9 }, rx = { 0x9 }, ref = { 0x9 };
            int8_t chips[255];
            dsss_spread_bit(&tx, (uint8_t)bit, ratio, chips);
            int ok = 1;
            for (int i = 0; i < ratio; i++) {
                uint8_t pn = lfsr_next_chip(&ref);
                int8_t want = (bit ^ pn) ? -1 : 1;
                if (chips[i] != want) ok = 0;
            }
            CHECK(ok, "ratio %d bit %d: chip = +1 when data == pn, -1 otherwise (BPSK-style)", ratio, bit);
            int32_t c = dsss_despread_bit(&rx, chips, ratio);
            CHECK(c == (bit ? -ratio : ratio), "ratio %d bit %d: synchronized correlation %d, want %d", ratio, bit, (int)c, bit ? -ratio : ratio);
            CHECK(tx.state == rx.state, "TX and RX registers advance identically");
        }
    }
    /* unsynchronized: one full period of the m-sequence correlates at -1 against any rotated copy => |corr| = 1 */
    {
        lfsr_t tx = { 0x9 }, rx = { 0x9 }; int8_t chips[15];
        dsss_spread_bit(&tx, 0, 15, chips);
        for (int shift = 1; shift < 15; shift++) {
            lfsr_t r = { 0x9 }; for (int s = 0; s < shift; s++) lfsr_next_chip(&r);
            int32_t c = dsss_despread_bit(&r, chips, 15);
            CHECK(c == -1, "code phase off by %d chips: correlation %d, want -1 (near zero = not synchronized)", shift, (int)c);
        }
        (void)rx;
    }
    /* processing gain in action: a continuous tone 3x stronger than the signal (a "jammer") is suppressed by the despreader */
    {
        int ratio = 255, wrong = 0, trials = 0;
        for (int t = 0; t < 60; t++) {
            int bit = t & 1; lfsr_t tx = { (uint8_t)(1 + (t % 15)) }, rx = tx; int8_t chips[255]; int8_t rcv[255];
            dsss_spread_bit(&tx, (uint8_t)bit, ratio, chips);
            double f = 0.1234 + 0.0007 * t, ph = 0.37 * t;
            for (int i = 0; i < ratio; i++) {
                double jam = 30.0 * cos(6.283185307179586 * f * i + ph);          /* signal chips are +-10, jammer amplitude 30 */
                int v = chips[i] * 10 + (int)lround(jam);
                rcv[i] = (int8_t)(v > 127 ? 127 : v < -127 ? -127 : v);
            }
            int32_t c = dsss_despread_bit(&rx, rcv, ratio);
            int dec = c < 0;
            trials++; if (dec != bit) wrong++;
        }
        CHECK(wrong == 0, "tone jammer 3x the signal amplitude, processing gain %d: %d of %d bits wrong", ratio, wrong, trials);
    }

    /* ---- DLL early-late error (HB[92]) ---- */
    {
        /* PN code at 2 samples per chip (so "half a chip" is one sample), one 15-chip period = 30 samples; use 4 periods */
        enum { SPC = 2, PER = 15 * SPC, LEN = 4 * PER };
        int8_t pn[LEN]; lfsr_t l = { 0x6 };
        for (int c = 0; c < LEN / SPC; c++) { int8_t v = lfsr_next_chip(&l) ? -1 : 1; for (int s = 0; s < SPC; s++) pn[c * SPC + s] = v; }
        /* analytic autocorrelation of the periodic 2 sps code from the definition */
        #define PN(i) pn[(((i) % PER) + PER) % PER]
        int R[9]; for (int tau = 0; tau <= 8; tau++) { R[tau] = 0; for (int i = 0; i < PER; i++) R[tau] += PN(i) * PN(i + tau); }
        /* derivation: lag 1 = 15 same-chip pairs (+1 each) + the -1 chip-to-chip autocorrelation = 14; lag 2 = 2 x (-1) = -2 */
        CHECK(R[0] == PER && R[1] == 14 && R[2] == -2, "autocorrelation shape of the 2 sps code: R(0)=%d R(1)=%d R(2)=%d, want 30 / 14 / -2", R[0], R[1], R[2]);
        for (int d = -2; d <= 2; d++) {
            /* received code delayed by d samples relative to the local replica: rx[i] = pn[i - d].
             * "Early" correlator = local replica advanced by one sample (half a chip): sum rx[i] pn[i+1] = sum rx[i-1] pn[i];
             * "late" = replica delayed: sum rx[i+1] pn[i]. */
            int8_t rx_e[LEN], rx_l[LEN];
            for (int i = 0; i < LEN; i++) { int ie = i - 1, il = i + 1; rx_e[i] = PN(ie - d); rx_l[i] = PN(il - d); }
            int32_t err = dll_chip_error(rx_e, rx_l, pn, LEN);
            /* closed form from R: early = R(d+1) (shift of the replica by +1 vs the delayed input), late = R(d-1) */
            int ae = d + 1 < 0 ? -(d + 1) : d + 1, al = d - 1 < 0 ? -(d - 1) : d - 1;
            int want = (R[ae] - R[al]) * (LEN / PER);
            CHECK(err == want, "delay %+d samples: early-late error %d, closed form %d", d, (int)err, want);
            if (d > 0) CHECK(err < 0, "received code late by %d: error must be negative (comment: negative means shift the local code later), got %d", d, (int)err);
            if (d < 0) CHECK(err > 0, "received code early by %d: error must be positive (shift earlier), got %d", -d, (int)err);
            if (d == 0) CHECK(err == 0, "aligned: error is zero, got %d", (int)err);
            /* the listing never says how the two arrays are produced: if "early_corr_chips" means the received stream
             * sampled early (rx advanced) instead of the replica advanced, the sign convention in its comment flips. */
            if (d == 1) { int32_t flipped = dll_chip_error(rx_l, rx_e, pn, LEN); CHECK(flipped == -err && flipped > 0, "swapping the early/late arrays flips the sign (%d vs %d): the comment's sign convention depends on the caller", (int)flipped, (int)err); }
        }
        #undef PN
    }
    TEST_DONE("test_D_dsss");
}
