/* Chapter 9: K=7 convolutional encoder + Viterbi ACS step.
 * The book gives an encoder and a single add-compare-select step, but not a full decoder. This test
 * wires a minimal hard-decision decoder (branch metrics + traceback, written HERE, not the book's)
 * around viterbi_acs_step() and predecessor0/1() to prove that the pieces are mutually consistent:
 * encode -> inject errors -> decode must recover the message. The reference for the code itself is
 * the textbook definition: generators 171 and 133 octal, newest input bit at the top of a 7-bit window. */
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "fm_qformat.h"
#include "fm_fec.h"
#include "fm_qpsk.h"

#define MSG_BITS 400
#define TAIL     (K - 1)
#define NSTEPS   (MSG_BITS + TAIL)

static uint8_t g_win[NSTEPS][NUM_STATES];

/* Textbook output bit for generator g (octal 0171 / 0133, leftmost tap = current input) given the 7 most
 * recent inputs with the NEWEST at bit 6 of `window`. */
static uint8_t ref_out(uint32_t window, uint32_t g) { return (uint8_t)__builtin_parity(window & g); }
static uint32_t bit_reverse7(uint32_t v) { uint32_t r = 0; for (int k = 0; k < 7; k++) r |= ((v >> k) & 1u) << (6 - k); return r; }

static int branch_cost(int state, int which_pred, uint8_t r0, uint8_t r1) {
    int ps = which_pred ? predecessor1(state) : predecessor0(state);
    uint32_t reg = ((uint32_t)ps << 1) | (uint32_t)(state & 1);   /* newest bit at bit 0 */
    uint8_t e0 = ref_out(bit_reverse7(reg), 0171), e1 = ref_out(bit_reverse7(reg), 0133);
    return (e0 != r0) + (e1 != r1);
}

/* returns number of decoded-bit errors vs msg */
static int run_codec(const uint8_t *msg, const int *flip_positions, int nflips, int *tail_ok) {
    uint8_t c0[NSTEPS], c1[NSTEPS];
    shift_reg = 0;
    for (int t = 0; t < NSTEPS; t++) conv_encode_bit(t < MSG_BITS ? msg[t] : 0, &c0[t], &c1[t]);
    for (int f = 0; f < nflips; f++) { int pos = flip_positions[f]; if (pos & 1) c1[pos >> 1] ^= 1; else c0[pos >> 1] ^= 1; }

    int32_t pm[NUM_STATES]; for (int s = 0; s < NUM_STATES; s++) pm[s] = (s == 0) ? 0 : 1000000;
    for (int t = 0; t < NSTEPS; t++) {
        int32_t bm[NUM_STATES][2];
        for (int s = 0; s < NUM_STATES; s++) { bm[s][0] = branch_cost(s, 0, c0[t], c1[t]); bm[s][1] = branch_cost(s, 1, c0[t], c1[t]); }
        viterbi_acs_step(pm, bm, g_win[t]);
    }
    uint8_t dec[NSTEPS]; int state = 0;                 /* tail-terminated: ends in the all-zero state */
    for (int t = NSTEPS - 1; t >= 0; t--) {
        dec[t] = state & 1;
        state = g_win[t][state] ? predecessor1(state) : predecessor0(state);
    }
    int errs = 0; for (int t = 0; t < MSG_BITS; t++) errs += dec[t] != msg[t];
    *tail_ok = (state == 0);
    return errs;
}

int main(void) {
    /* Encoder vs the textbook definition: window with the newest bit at the top, masks 0171 / 0133. */
    shift_reg = 0; uint32_t win = 0; lcg_state = 4242u;
    int bad = 0;
    for (int n = 0; n < 5000; n++) {
        uint8_t b = (lcg_next() >> 7) & 1, o0, o1; conv_encode_bit(b, &o0, &o1);
        win = (win >> 1) | ((uint32_t)b << 6);
        if (o0 != ref_out(win, 0171) || o1 != ref_out(win, 0133)) bad++;
    }
    CHECK(bad == 0, "encoder disagrees with generators 171/133 octal on %d of 5000 bits", bad);

    /* Impulse response: a lone 1 followed by zeros reads out the generators MSB-first, i.e. 1111001 and
     * 1011011, which are 0171 and 0133. */
    shift_reg = 0; uint8_t a, b; int g0 = 0, g1 = 0;
    for (int n = 0; n < 7; n++) { conv_encode_bit(n == 0, &a, &b); g0 = (g0 << 1) | a; g1 = (g1 << 1) | b; }
    CHECK(g0 == 0171 && g1 == 0133, "impulse response (first output = MSB): %o/%o octal", g0, g1);

    /* The Chapter 9 golden vector: 10 data bits + 6 flush zeros -> 16 (out0,out1) pairs. */
    {
        static const uint8_t in[16] = { 1,0,1,1,0,1,0,0,1,1, 0,0,0,0,0,0 };
        static const uint8_t want[16][2] = { {1,1},{1,0},{0,0},{1,0},{0,1},{1,0},{1,0},{1,0},{1,0},{1,0},{0,0},{1,1},{1,1},{0,1},{1,0},{1,1} };
        shift_reg = 0; int mism = 0;
        for (int n = 0; n < 16; n++) { uint8_t o0, o1; conv_encode_bit(in[n], &o0, &o1); mism += (o0 != want[n][0]) + (o1 != want[n][1]); }
        CHECK(mism == 0, "encoder golden vector: %d mismatching bits", mism);
    }

    /* A lone 1 followed by K-1 zeros has output weight 5 + 5 = 10, the free distance of the K=7 171/133 code. */
    shift_reg = 0; int wt = 0;
    for (int n = 0; n < 7; n++) { conv_encode_bit(n == 0, &a, &b); wt += a + b; }
    CHECK(wt == 10, "weight of the single-input-1 error event: %d (free distance 10)", wt);

    CHECK(NUM_STATES == 64 && predecessor0(5) == 2 && predecessor1(5) == 34, "predecessor helpers");

    uint8_t msg[MSG_BITS]; for (int t = 0; t < MSG_BITS; t++) msg[t] = (lcg_next() >> 9) & 1;
    int tail_ok;
    CHECK(run_codec(msg, NULL, 0, &tail_ok) == 0 && tail_ok, "clean channel must decode exactly");
    /* Scattered errors (>= 20 channel bits apart; free distance is 10, so each is correctable on its own). */
    int flips[] = { 30, 111, 210, 333, 480, 602, 721 };
    CHECK(run_codec(msg, flips, 7, &tail_ok) == 0, "7 isolated channel-bit errors must all be corrected");
    /* The classic limit: a dense burst beats a K=7 code. Documented in the book (Chapter 9); asserted so
     * the claim "burst errors poorly" stays honest. */
    int burst[16]; for (int i = 0; i < 16; i++) burst[i] = 300 + i;
    CHECK(run_codec(msg, burst, 16, &tail_ok) > 0, "a 16-bit channel burst is expected to defeat hard-decision Viterbi");
    /* Random sparse errors at ~1% channel BER: decoded BER must be far below channel BER. */
    int tot = 0, nsets = 20;
    for (int set = 0; set < nsets; set++) {
        int fl[8]; for (int i = 0; i < 8; i++) fl[i] = (int)((lcg_next() >> 3) % (2 * NSTEPS));
        tot += run_codec(msg, fl, 8, &tail_ok);
    }
    CHECK(tot <= 4, "random ~1%% channel errors: %d residual bit errors in %d runs", tot, nsets);

    /* Soft LLR signs and bit order (I = bit1, Q = bit0). */
    int32_t l1, l0; qpsk_soft_llr(20000, -20000, &l1, &l0);
    CHECK(l1 > 0 && l0 < 0, "LLR signs follow the axis signs: %d %d", l1, l0);
    qpsk_soft_llr(0, 0, &l1, &l0); CHECK(l1 == 0 && l0 == 0, "zero in, zero LLR");
    TEST_DONE("test_fec");
}
