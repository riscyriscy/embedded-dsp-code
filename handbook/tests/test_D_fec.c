/* Chapter 21 (FEC), HB[103]..HB[110]: soft LLRs, the rate-1/2 K=7 convolutional encoder, Viterbi ACS / branch metrics / traceback,
 * rate-3/4 puncturing, block interleaver, GF(2^8) tables and multiply (src/hb_fec.c).
 *
 * The chapter lists the pieces of a Viterbi decoder but no driver.  The driver below (HARNESS, written here) is the
 * minimum glue: build_branch_metrics -> viterbi_acs_step -> viterbi_traceback, emitting decoded_bits_out[0] once per symbol
 * exactly as the prose says ("a caller emits just that one bit per call"), with path metrics zero-initialised as the prose says.
 * Independent references: the polynomial-tap definition of a convolutional code (output_n = XOR_k g[k] u[n-k]); an exhaustive
 * trellis check; Monte-Carlo AWGN with a Box-Muller noise source; shift-and-add carry-less GF multiplication with polynomial
 * long division; closed-form index maps for the interleaver; IEEE 802.11's published rate-3/4 puncturing ("A0 B0 A1 B2"). */
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "hb_fec.h"

/* ---------- local PRNG for noise (xorshift64*) ---------- */
static uint64_t rs = 88172645463325252ULL;
static uint64_t xr(void) { rs ^= rs >> 12; rs ^= rs << 25; rs ^= rs >> 27; return rs * 2685821657736338717ULL; }
static double u01(void) { return ((xr() >> 11) + 0.5) / 9007199254740992.0; }
static double gauss(void) { double a = u01(), b = u01(); return sqrt(-2.0 * log(a)) * cos(6.283185307179586 * b); }

/* ---------- code definitions straight from the polynomial taps ---------- */
/* tap list read left to right: tap[0] multiplies the CURRENT input bit u[n], tap[k] multiplies u[n-k] (the usual MSB-first octal reading) */
static const int G171[7] = { 1,1,1,1,0,0,1 }, G133[7] = { 1,0,1,1,0,1,1 };
static void encode_taps(const int *g0, const int *g1, const uint8_t *u, int n, uint8_t *c) {
    for (int t = 0; t < n; t++) {
        int a = 0, b = 0;
        for (int k = 0; k < 7; k++) if (t - k >= 0) { a ^= g0[k] & u[t - k]; b ^= g1[k] & u[t - k]; }
        c[2 * t] = (uint8_t)a; c[2 * t + 1] = (uint8_t)b;
    }
}

typedef void (*enc_fn)(uint8_t, uint8_t *, uint8_t *);
typedef void (*bm_fn)(int32_t[NUM_STATES][2], int32_t, int32_t);
/* hypothesis outputs from the tap lists: bit k of a 7-bit register is the input k steps ago (bit 0 = newest) */
static int taps_out(const int *g, unsigned reg) { int a = 0; for (int k = 0; k < 7; k++) a ^= g[k] & (int)((reg >> k) & 1u); return a; }
/* a K=7 encoder with the generator masks applied the other way round (taps mirrored); used only as a negative control */
static uint32_t mirror_reg;
static void enc_mirrored(uint8_t b, uint8_t *o0, uint8_t *o1) {
    mirror_reg = ((mirror_reg << 1) | (b & 1u)) & 0x7Fu;
    *o0 = __builtin_parity(mirror_reg & 0x79u); *o1 = __builtin_parity(mirror_reg & 0x5Bu);
}

static viterbi_decoder_t V;
/* HARNESS: streaming decode.  l0/l1: per-step LLRs (positive = 0).  Emits bit (t - (D-1)) after step t.  Returns bits emitted. */
static int decode_stream(bm_fn bm_f, const int32_t *l0, const int32_t *l1, int nsteps, uint8_t *dec) {
    const int D = MAX_TRACEBACK_DEPTH; int emitted = 0; uint8_t win[MAX_TRACEBACK_DEPTH];
    memset(&V, 0, sizeof V);
    for (int t = 0; t < nsteps; t++) {
        int32_t bm[NUM_STATES][2];
        bm_f(bm, l0[t], l1[t]);
        viterbi_acs_step(&V, bm);
        if (t >= D - 1) { int r = viterbi_traceback(&V, win); if (r != MAX_TRACEBACK_DEPTH) return -1; dec[emitted++] = win[0]; }
    }
    return emitted;
}

#define MSG 499
#define TAILB 6
#define NSTEPS (MSG + TAILB + MAX_TRACEBACK_DEPTH)         /* message, flush bits, then D zero bits so every message bit gets its full traceback depth */
static uint8_t g_msg[NSTEPS], g_c[2 * NSTEPS], g_rx[2 * NSTEPS], g_dec[NSTEPS];
static int32_t g_l0[NSTEPS], g_l1[NSTEPS];

static void make_message(void) { for (int t = 0; t < NSTEPS; t++) g_msg[t] = (t < MSG) ? (uint8_t)((lcg_next() >> 9) & 1) : 0; }
static void encode_with(enc_fn e) { shift_reg = 0; mirror_reg = 0; for (int t = 0; t < NSTEPS; t++) e(g_msg[t], &g_c[2 * t], &g_c[2 * t + 1]); }
static void hard_llrs(const uint8_t *rx) { for (int t = 0; t < NSTEPS; t++) { g_l0[t] = rx[2 * t] ? -1 : 1; g_l1[t] = rx[2 * t + 1] ? -1 : 1; } }
static int msg_errors(bm_fn bm_f) {
    int n = decode_stream(bm_f, g_l0, g_l1, NSTEPS, g_dec);
    if (n != NSTEPS - MAX_TRACEBACK_DEPTH + 1) return 100000;
    int e = 0; for (int t = 0; t < MSG; t++) e += g_dec[t] != g_msg[t];
    return e;
}

/* ---------- AWGN BER at one Eb/N0 for the rate-1/2 code ---------- */
static double awgn_ber(int nbits, double ebn0_db, int soft, uint64_t seed) {
    static uint8_t msg[600000];
    rs = seed;
    double sigma = sqrt(1.0 / (2.0 * 0.5 * pow(10.0, ebn0_db / 10.0)));        /* unit-energy BPSK coded bits, R = 1/2 */
    const int D = MAX_TRACEBACK_DEPTH; int nst = nbits + TAILB + D, errs = 0, counted = 0;
    memset(&V, 0, sizeof V); shift_reg = 0; uint8_t win[MAX_TRACEBACK_DEPTH];
    for (int t = 0; t < nst; t++) {
        uint8_t b = (t < nbits) ? (uint8_t)((xr() >> 20) & 1) : 0; if (t < nbits) msg[t] = b;
        uint8_t o0, o1; conv_encode_bit(b, &o0, &o1);
        double r0 = (o0 ? -1.0 : 1.0) + sigma * gauss(), r1 = (o1 ? -1.0 : 1.0) + sigma * gauss();
        int32_t a, c;
        if (soft) { a = (int32_t)lround(r0 * 16.0); c = (int32_t)lround(r1 * 16.0); if (a > 127) a = 127; if (a < -127) a = -127; if (c > 127) c = 127; if (c < -127) c = -127; }
        else { a = r0 > 0 ? 1 : -1; c = r1 > 0 ? 1 : -1; }
        int32_t bm[NUM_STATES][2]; build_branch_metrics(bm, a, c); viterbi_acs_step(&V, bm);
        if (t >= D - 1) { viterbi_traceback(&V, win); int ti = t - (D - 1); if (ti < nbits) { counted++; errs += win[0] != msg[ti]; } }
    }
    return (double)errs / counted;
}
static double interp_ebn0(const double *eb, const double *ber, int n, double target) {
    for (int i = 0; i < n - 1; i++) if (ber[i] >= target && ber[i + 1] < target && ber[i + 1] > 0) {
        double a = log10(ber[i]), b = log10(ber[i + 1]), t = log10(target); return eb[i] + (eb[i + 1] - eb[i]) * (a - t) / (a - b); }
    return -1000;
}

/* ---------- GF(2^8) reference: shift-and-add carry-less product, then reduction by polynomial long division ---------- */
static unsigned gf_ref(unsigned a, unsigned b, unsigned poly) {
    unsigned r = 0; for (int i = 0; i < 8; i++) if ((b >> i) & 1) r ^= a << i;
    for (int bit = 14; bit >= 8; bit--) if (r & (1u << bit)) r ^= poly << (bit - 8);
    return r;
}

/* ---------- HB[103] statements, verbatim, wrapped in a harness function ---------- */
static void snippet_hb103(int32_t i, uint8_t *hard_out, int32_t *soft_out) {
#include "shims/hbD_hb103_soft_llr_snippet.inc"
    *hard_out = hard_bit; *soft_out = soft_llr;
}

int main(void) {
    /* ===================== HB[103]/[104]: LLR convention ===================== */
    {
        int bad = 0;
        for (int32_t i = -32768; i <= 32767; i += 7) {
            uint8_t hb; int32_t sl; snippet_hb103(i, &hb, &sl);
            int32_t q0, q1; qpsk_soft_llr(i, -i / 2, &q0, &q1);
            if (sl != i) bad++;                                  /* LLR_SCALE_Q = 1.0 (Q15): llr == raw value */
            if (q0 != i || q1 != -i / 2) bad++;
            if ((sl > 0 && hb != 0) || (sl < 0 && hb != 1)) bad++;   /* positive LLR = confident 0, negative = confident 1 */
        }
        CHECK(bad == 0, "soft LLR (gain 1.0): llr = raw value, sign agrees with the hard decision (%d violations)", bad);
        uint8_t hb; int32_t sl; snippet_hb103(0, &hb, &sl); CHECK(hb == 0 && sl == 0, "i = 0 -> hard bit 0, LLR 0");
        /* i * LLR_SCALE_Q is an int32_t product: at the default gain of 1.0 (Q15 32768) a full-scale 16-bit input fits, a gain of 8.0 would not */
        long long fits = 32768LL * 32768LL, wide = 32767LL * (8LL * 32768LL);
        CHECK(fits <= (long long)INT32_MAX && wide > (long long)INT32_MAX, "full-scale 16-bit I at gain 1.0 gives a product of %lld (fits int32_t); gain 8.0 would reach %lld", fits, wide);
    }

    /* ===================== HB[105]: the encoder ===================== */
    {
        /* trellis consistency, exhaustive: every (previous 7-bit register, input bit) */
        int bad = 0;
        for (uint32_t r = 0; r < 128; r++) for (int b = 0; b < 2; b++) {
            shift_reg = r; uint8_t o0, o1; conv_encode_bit((uint8_t)b, &o0, &o1);
            uint8_t e0, e1; expected_encoder_outputs((int)(r & 0x3F), b, &e0, &e1);
            if (o0 != e0 || o1 != e1) bad++;
            if ((shift_reg & 0x3F) != ((((r & 0x3F) << 1) | (uint32_t)b) & 0x3F)) bad++;
            /* the new state's predecessors (predecessor helpers) must include the old state exactly when it is the right one */
            int s = (int)(shift_reg & 0x3F);
            if (predecessor0(s) != (int)(r & 0x3F) && predecessor1(s) != (int)(r & 0x3F)) bad++;
            if ((s & 1) != b) bad++;
        }
        CHECK(bad == 0, "encoder, expected_encoder_outputs and predecessor0/1 agree on all 256 transitions (%d violations)", bad);
        /* predecessor helpers: each state has two distinct predecessors and each state is a predecessor of exactly two states */
        int cnt[NUM_STATES]; memset(cnt, 0, sizeof cnt);
        int distinct = 1; for (int s = 0; s < NUM_STATES; s++) { cnt[predecessor0(s)]++; cnt[predecessor1(s)]++; if (predecessor0(s) == predecessor1(s)) distinct = 0; }
        int two = 1; for (int s = 0; s < NUM_STATES; s++) if (cnt[s] != 2) two = 0;
        CHECK(distinct && two, "trellis butterflies: 2 distinct predecessors per state, 2 successors per state");
        CHECK(NUM_STATES == 64 && predecessor0(5) == 2 && predecessor1(5) == 34, "NUM_STATES 64, predecessors of state 5 are 2 and 34");

        /* impulse response: out0/out1 read left to right are 1111001 / 1011011, i.e. 171 / 133 octal (MSB first) */
        uint8_t a, b; int g0[7], g1[7];
        shift_reg = 0; for (int n = 0; n < 7; n++) { conv_encode_bit(n == 0, &a, &b); g0[n] = a; g1[n] = b; }
        int same_std = 1;
        for (int k = 0; k < 7; k++) if (g0[k] != G171[k] || g1[k] != G133[k]) same_std = 0;
        CHECK(same_std, "impulse response is 1111001 / 1011011 = the standard 171 / 133 code (out0 = %d%d%d%d%d%d%d)", g0[0], g0[1], g0[2], g0[3], g0[4], g0[5], g0[6]);
        CHECK(0x4F == 0117 && 0x6D == 0155, "the masks are the bit-reversed generators: 0x4F = 117 octal, 0x6D = 155 octal");
        /* random message: encoder == polynomial definition of 171/133 */
        static uint8_t u[300], c1[600], c3[600];
        for (int t = 0; t < 300; t++) u[t] = (lcg_next() >> 8) & 1;
        shift_reg = 0; for (int t = 0; t < 300; t++) conv_encode_bit(u[t], &c1[2 * t], &c1[2 * t + 1]);
        encode_taps(G171, G133, u, 300, c3);
        CHECK(memcmp(c1, c3, 600) == 0, "encoder == polynomial-tap definition of the standard 171/133 code on a 300-bit message");
        /* golden vector, generated from the tap definition for the input 1,0,1,1,0,1,0,0,1,1 followed by six flush zeros */
        {
            static const uint8_t in[16] = { 1,0,1,1,0,1,0,0,1,1, 0,0,0,0,0,0 };
            static const uint8_t want[16][2] = { {1,1},{1,0},{0,0},{1,0},{0,1},{1,0},{1,0},{1,0},{1,0},{1,0},{0,0},{1,1},{1,1},{0,1},{1,0},{1,1} };
            shift_reg = 0; int mism = 0; for (int n = 0; n < 16; n++) { uint8_t o0, o1; conv_encode_bit(in[n], &o0, &o1); mism += (o0 != want[n][0]) + (o1 != want[n][1]); }
            CHECK(mism == 0, "golden vector: %d mismatching bits", mism);
        }
        /* free distance = 10 (enumerate all inputs that start with 1, length 12, followed by a 6-bit flush) */
        int dmin = 1000;
        for (uint32_t w = 0; w < (1u << 11); w++) {
            shift_reg = 0; int weight = 0;
            for (int n = 0; n < 18; n++) { uint8_t bit = (n == 0) ? 1 : (n < 12 ? (w >> (n - 1)) & 1 : 0); conv_encode_bit(bit, &a, &b); weight += a + b; }
            if (weight < dmin) dmin = weight;
        }
        CHECK(dmin == 10, "free distance: min weight over 2048 short error events = %d, want 10 (the K=7 code's d_free)", dmin);
    }

    /* ===================== HB[107]: branch metrics ===================== */
    {
        int bad = 0;
        for (int t = 0; t < 50; t++) {
            int32_t l0 = (int32_t)(lcg_next() % 2001) - 1000, l1 = (int32_t)(lcg_next() % 2001) - 1000;
            int32_t bm[NUM_STATES][2]; build_branch_metrics(bm, l0, l1);
            for (int s = 0; s < NUM_STATES; s++) for (int w = 0; w < 2; w++) {
                int pred = w ? ((s >> 1) | 0x20) : (s >> 1), bit = s & 1;
                unsigned reg = ((unsigned)pred << 1) | (unsigned)bit;        /* 7-bit register the encoder would hold */
                int e0 = taps_out(G171, reg), e1 = taps_out(G133, reg);       /* from the tap lists, not from the masks */
                int want = (e0 ? l0 : -l0) + (e1 ? l1 : -l1);                /* LLR>0 = bit 0: cost +llr if the hypothesis says 1 */
                if (bm[s][w] != want) bad++;
            }
        }
        CHECK(bad == 0, "build_branch_metrics = sum of (+llr if hypothesis bit is 1, -llr if 0): %d mismatches", bad);
        int32_t bm[NUM_STATES][2]; build_branch_metrics(bm, 0, 777);
        int erasure_ok = 1; for (int s = 0; s < NUM_STATES; s++) for (int w = 0; w < 2; w++) if (bm[s][w] != 777 && bm[s][w] != -777) erasure_ok = 0;
        CHECK(erasure_ok, "an erasure (LLR 0) contributes nothing to the branch metric (prose, depuncturing)");
        build_branch_metrics(bm, 5, 9);
        int sym = 1; for (int s = 0; s < NUM_STATES; s++) for (int w = 0; w < 2; w++) { int32_t x = bm[s][w]; if (x != 14 && x != -14 && x != 4 && x != -4) sym = 0; }
        CHECK(sym, "metrics take only the four values +-(l0+l1), +-(l0-l1)");
    }

    /* ===================== HARNESS decode: clean channel, scattered errors, burst, random ===================== */
    {
        lcg_state = 4242u;
        make_message(); encode_with(conv_encode_bit);
        memcpy(g_rx, g_c, sizeof g_rx); hard_llrs(g_rx);
        CHECK(msg_errors(build_branch_metrics) == 0, "clean channel decodes exactly (499 message bits, depth %d)", MAX_TRACEBACK_DEPTH);
        /* depth sanity (prose): sizeof traceback buffer = 64 x 35 = 2240, "2240 traceback entries alone" */
        CHECK(sizeof V.traceback == 2240 && MAX_TRACEBACK_DEPTH == 35, "traceback buffer is %zu entries (prose Chapter 20: 64 x 35 = 2240)", sizeof V.traceback);
        /* isolated channel errors, >= 40 coded bits apart (free distance 10: each is individually correctable) */
        static const int flips[] = { 30, 111, 210, 333, 480, 602, 721, 850, 960 };
        memcpy(g_rx, g_c, sizeof g_rx); for (unsigned i = 0; i < sizeof flips / sizeof *flips; i++) g_rx[flips[i]] ^= 1;
        hard_llrs(g_rx);
        CHECK(msg_errors(build_branch_metrics) == 0, "9 isolated channel-bit errors are all corrected by hard-decision decoding");
        /* two errors inside one constraint length: d_free = 10 still corrects up to 4 errors in a window of ~2K, so 3 close errors are fine */
        memcpy(g_rx, g_c, sizeof g_rx); g_rx[400] ^= 1; g_rx[403] ^= 1; g_rx[409] ^= 1; hard_llrs(g_rx);
        CHECK(msg_errors(build_branch_metrics) == 0, "3 channel errors inside one constraint length are corrected (floor((d_free-1)/2) = 4)");
        /* a 24-bit channel burst defeats hard-decision Viterbi; interleaving the same burst fixes it (prose: Interleaving) */
        const int ROWS = 24, COLS = 45;                                  /* 24 x 45 = 1080 = 2 * NSTEPS */
        CHECK(ROWS * COLS == 2 * NSTEPS, "interleaver geometry covers the %d coded bits", 2 * NSTEPS);
        memcpy(g_rx, g_c, sizeof g_rx); for (int i = 0; i < 24; i++) g_rx[500 + i] ^= 1; hard_llrs(g_rx);
        int e_burst = msg_errors(build_branch_metrics);
        CHECK(e_burst > 0, "24 consecutive channel errors defeat the decoder without an interleaver (%d residual bit errors)", e_burst);
        static uint8_t tx[2 * NSTEPS], back[2 * NSTEPS];
        interleave_block(g_c, tx, ROWS, COLS);
        for (int i = 0; i < 24; i++) tx[500 + i] ^= 1;
        deinterleave_block(tx, back, ROWS, COLS); hard_llrs(back);
        int e_int = msg_errors(build_branch_metrics);
        CHECK(e_int == 0, "same 24-bit burst through interleave -> channel -> deinterleave is fully corrected (%d residual errors, was %d)", e_int, e_burst);
        printf("  [info] 24-bit burst: %d residual errors without interleaver, %d with a %dx%d block interleaver\n", e_burst, e_int, ROWS, COLS);
        /* random sparse errors at ~1% channel BER */
        int tot = 0;
        for (int set = 0; set < 20; set++) { memcpy(g_rx, g_c, sizeof g_rx); for (int i = 0; i < 10; i++) g_rx[(lcg_next() >> 3) % (2 * NSTEPS)] ^= 1; hard_llrs(g_rx); tot += msg_errors(build_branch_metrics); }
        CHECK(tot <= 8, "random ~1%% channel BER (10 of 1082 bits, 20 trials): %d residual message-bit errors", tot);
        /* soft information, constructed case.  Take the minimum-weight error event: flipping message bit p changes exactly 10 coded
         * bits (the impulse response, 5+5).  Corrupt 6 of those 10.  Hard decisions then see 6 errors vs 4 for the alternative path,
         * so a hard-decision ML decoder picks the WRONG message.  If the 6 corrupted samples are LOW confidence (3) and everything else
         * is confident (100), the alternative path must contradict 4 confident bits to explain them and the true path wins. */
        {
            const int p = 250; uint8_t imp[2 * 7]; uint8_t uu[7] = { 1,0,0,0,0,0,0 };
            { uint8_t cc[14]; shift_reg = 0; for (int t = 0; t < 7; t++) conv_encode_bit(uu[t], &cc[2 * t], &cc[2 * t + 1]); memcpy(imp, cc, sizeof imp); }
            int diffpos[16], nd = 0; for (int j = 0; j < 14; j++) if (imp[j]) diffpos[nd++] = 2 * p + j;
            CHECK(nd == 10, "the impulse response has weight 10 (%d)", nd);
            memcpy(g_rx, g_c, sizeof g_rx); for (int i = 0; i < 6; i++) g_rx[diffpos[i]] ^= 1;
            hard_llrs(g_rx);
            int e_hard = msg_errors(build_branch_metrics);
            CHECK(e_hard == 1, "hard decisions: 6 of the 10 differing bits corrupted -> decoder chooses the alternative path (1 message bit wrong, got %d)", e_hard);
            memcpy(g_rx, g_c, sizeof g_rx); hard_llrs(g_rx);                     /* correct signs, confidence 100 everywhere */
            for (int t = 0; t < NSTEPS; t++) { g_l0[t] *= 100; g_l1[t] *= 100; }
            for (int i = 0; i < 6; i++) { int pos = diffpos[i], t = pos / 2; if (pos & 1) g_l1[t] = -g_l1[t] / 100 * 3; else g_l0[t] = -g_l0[t] / 100 * 3; }
            int e_soft = msg_errors(build_branch_metrics);
            CHECK(e_soft == 0, "same 6 wrong-sign samples but low confidence (3 vs 100): soft decoder recovers the message (%d errors)", e_soft);
            /* and the same wrong samples at full confidence are indistinguishable from the hard case */
            for (int i = 0; i < 6; i++) { int pos = diffpos[i], t = pos / 2; if (pos & 1) g_l1[t] = g_l1[t] / 3 * 100; else g_l0[t] = g_l0[t] / 3 * 100; }
            CHECK(msg_errors(build_branch_metrics) == 1, "wrong samples at full confidence: back to the hard-decision outcome");
        }
    }

    /* ===================== HB[106]/[107]: traceback window mechanics and path-metric growth ===================== */
    {
        /* decoded_bits_out[0] is the OLDEST bit of the window: after a fresh message, the full window equals the last D message bits */
        lcg_state = 99u; make_message(); encode_with(conv_encode_bit); hard_llrs(g_c);
        memset(&V, 0, sizeof V); uint8_t win[MAX_TRACEBACK_DEPTH];
        int upto = 300;
        for (int t = 0; t < upto; t++) { int32_t bm[NUM_STATES][2]; build_branch_metrics(bm, g_l0[t], g_l1[t]); viterbi_acs_step(&V, bm); }
        viterbi_traceback(&V, win);
        int match = 1; for (int j = 0; j < MAX_TRACEBACK_DEPTH; j++) if (win[j] != g_msg[upto - MAX_TRACEBACK_DEPTH + j]) match = 0;
        CHECK(match, "after %d steps the traceback window [0..%d] = message bits %d..%d, oldest first (depth wrapped %d times)", upto, MAX_TRACEBACK_DEPTH - 1, upto - MAX_TRACEBACK_DEPTH, upto - 1, upto / MAX_TRACEBACK_DEPTH);
        CHECK(V.depth == upto % MAX_TRACEBACK_DEPTH, "depth points one past the newest column (%d)", V.depth);
        /* path metrics are never renormalised: with clean +-20000 LLRs the best metric falls by 40000 per step */
        memset(&V, 0, sizeof V);
        for (int t = 0; t < 1000; t++) { int32_t bm[NUM_STATES][2]; build_branch_metrics(bm, g_l0[t % NSTEPS] * 20000, g_l1[t % NSTEPS] * 20000); viterbi_acs_step(&V, bm); }
        int32_t best = V.path_metric[0]; for (int s = 1; s < NUM_STATES; s++) if (V.path_metric[s] < best) best = V.path_metric[s];
        long long steps_to_overflow = (long long)INT32_MAX / (-(long long)best / 1000);
        CHECK(best < -39000000, "[LIMITATION, prose-acknowledged] best path metric after 1000 steps at |LLR| 20000: %d (about -40000 per step)", (int)best);
        printf("  [info] int32_t path metrics overflow after about %lld steps at |LLR| = 20000 unless the min is subtracted each step (the prose gives that fix)\n", steps_to_overflow);
        CHECK(steps_to_overflow < 60000, "overflow horizon %lld steps", steps_to_overflow);
    }

    /* ===================== soft-decision gain (prose: "typically gains about 2 dB") ===================== */
    {
        const int NB = 400000;
        static const double ebh[] = { 4.50, 4.75, 5.00, 5.25 }, ebs[] = { 2.50, 2.75, 3.00, 3.25 };
        double bh[4], bs[4];
        for (int i = 0; i < 4; i++) { bh[i] = awgn_ber(NB, ebh[i], 0, 7); bs[i] = awgn_ber(NB, ebs[i], 1, 9); }
        for (int i = 0; i < 4; i++) printf("  [info] Eb/N0 %.2f dB: hard BER %.3e   |   Eb/N0 %.2f dB: soft BER %.3e\n", ebh[i], bh[i], ebs[i], bs[i]);
        double h = interp_ebn0(ebh, bh, 4, 1e-3), s = interp_ebn0(ebs, bs, 4, 1e-3);
        printf("  [info] BER 1e-3 needs %.2f dB hard, %.2f dB soft => soft-decision gain %.2f dB (quantised to +-127 with 1/16 steps, depth %d)\n", h, s, h - s, MAX_TRACEBACK_DEPTH);
        CHECK(h > 4 && s > 2, "interpolation found the 1e-3 crossings (%.2f, %.2f)", h, s);
        CHECK(h - s >= 2.0, "soft-decision gain at BER 1e-3 is %.2f dB; prose claims about 2 dB", h - s);
        /* and sanity against theory: soft K=7 r=1/2 needs ~ 2.5-3 dB for 1e-3, uncoded BPSK needs 6.8 dB */
        CHECK(s < 3.2 && s > 2.2, "soft-decision Eb/N0 for 1e-3 (%.2f dB) is in the literature range 2.4-3.0 dB", s);
        double unc = 6.79;                                              /* Q(sqrt(2 Eb/N0)) = 1e-3, closed form: Eb/N0 = 6.79 dB */
        CHECK(unc - s > 3.5, "coding gain over uncoded BPSK at 1e-3 is %.2f dB", unc - s);
    }

    /* ===================== HB[108]: puncturing ===================== */
    {
        int kept = 0; for (int i = 0; i < 6; i++) kept += puncture_pattern_3_4[i];
        CHECK(kept == 4, "pattern keeps 4 of 6 coded bits: 3 input bits -> 6 coded -> 4 sent = rate 3/4 (prose)");
        /* 802.11 rate-3/4: transmitted order A0 B0 A1 B2 (A3/B1 dropped) = kept indices 0,1,2,5 of A0 B0 A1 B1 A2 B2 */
        static const int want_idx[4] = { 0, 1, 2, 5 }; int j = 0, ok = 1;
        for (int i = 0; i < 6; i++) if (puncture_pattern_3_4[i]) { if (j >= 4 || want_idx[j++] != i) ok = 0; }
        CHECK(ok && j == 4, "pattern = IEEE 802.11's published 3/4 puncturing (A0 B0 A1 B2)");
        enum { NC = 1200 };
        static uint8_t enc[NC + 8], out[NC]; static int32_t llr[NC], dl[NC + 16];
        for (int i = 0; i < NC; i++) enc[i] = (uint8_t)((lcg_next() >> 4) & 1);
        int n = puncture_bits(enc, NC, out);
        CHECK(n == NC * 4 / 6, "1200 coded bits -> %d transmitted (want 800)", n);
        int bad = 0, pos = 0; for (int i = 0; i < NC; i++) { int k6 = i % 6; if (k6 == 0 || k6 == 1 || k6 == 2 || k6 == 5) { if (out[pos++] != enc[i]) bad++; } }
        CHECK(bad == 0 && pos == n, "kept bits appear in order, dropped ones (index 3 and 4 of every 6) are gone (%d bad)", bad);
        /* partial last period */
        int n_partial = puncture_bits(enc, NC + 2, out);
        CHECK(n_partial == 802, "1202 coded bits -> %d transmitted (800 + 2)", n_partial);
        { static const int exp5[5] = { 0, 1, 2, 3, 3 }; for (int i = 0; i < 5; i++) CHECK(puncture_bits(enc, i, out) == exp5[i], "n=%d coded bits -> %d sent (want %d)", i, puncture_bits(enc, i, out), exp5[i]); }
        /* de-puncturing symmetry */
        puncture_bits(enc, NC, out);
        for (int i = 0; i < n; i++) llr[i] = out[i] ? -50 : 50;
        int m = depuncture_llrs(llr, n, dl);
        CHECK(m == NC, "de-puncture returns %d values (want 1200)", m);
        bad = 0; for (int i = 0; i < NC; i++) { int k6 = i % 6; if (k6 == 3 || k6 == 4) { if (dl[i] != 0) bad++; } else if (dl[i] != (enc[i] ? -50 : 50)) bad++; }
        CHECK(bad == 0, "depuncture(puncture(x)): kept positions restored with their LLR, punctured positions are exact erasures (%d bad)", bad);
        /* partial period: canary values after the input must never be read */
        { static int32_t src[8]; for (int i = 0; i < 8; i++) src[i] = 777;
          int mm = depuncture_llrs(src, 5, dl);   /* 5 kept values: one full period (4) + 1 */
          int canary = 0; for (int i = 0; i < mm; i++) if (dl[i] == 777) canary++;
          CHECK(mm == 12 && canary == 5, "5 received values (one more than a full period): output length %d (one extra period of 6), 5 values placed", mm);
          for (int i = 0; i < 5; i++) src[i] = 100 + i;
          for (int i = 5; i < 8; i++) src[i] = 777;
          mm = depuncture_llrs(src, 5, dl);
          int leaked = 0; for (int i = 0; i < mm; i++) if (dl[i] == 777) leaked++;
          CHECK(leaked == 0, "de-puncture never reads past num_received (guard works): %d canaries leaked", leaked);
          CHECK(dl[0] == 100 && dl[1] == 101 && dl[2] == 102 && dl[3] == 0 && dl[4] == 0 && dl[5] == 103 && dl[6] == 104 && dl[7] == 0 && dl[8] == 0 && dl[9] == 0 && dl[10] == 0 && dl[11] == 0,
                "second period: first kept slot takes the 5th value, the rest are erasures");
          CHECK(depuncture_llrs(src, 0, dl) == 0, "zero received -> zero output"); }
        /* prose: 1000-byte payload -> 2000 bytes at rate 1/2, ~1333 bytes at 3/4 */
        { int coded_bits = 2 * 8000; static uint8_t dummy[16000], o2[16000]; memset(dummy, 1, sizeof dummy);
          int sent = puncture_bits(dummy, coded_bits, o2);
          CHECK(coded_bits / 8 == 2000 && sent / 8 == 1333 && sent == 10667, "1000-byte payload: %d coded bytes at rate 1/2, %d bits = %.1f bytes at 3/4 (16000 * 2/3 = 10666.7 -> 10667; prose: about 1333)", coded_bits / 8, sent, sent / 8.0);
          CHECK_NEAR((double)coded_bits / sent, 1.5, 0.001, "airtime ratio 1/2 -> 3/4 is 1.5x"); }
        /* end to end at rate 3/4: encode -> puncture -> LLR -> depuncture (erasures) -> the same Viterbi chain */
        lcg_state = 777u; make_message(); encode_with(conv_encode_bit);
        static uint8_t pc[2 * NSTEPS]; static int32_t pl[2 * NSTEPS], dpl[2 * NSTEPS + 16];
        int np = puncture_bits(g_c, 2 * NSTEPS, pc);                     /* 1080 coded -> 720 sent: rate 540/720 = 3/4 */
        CHECK(np == 720 && NSTEPS * 4 == np * 3, "540 trellis steps send %d bits: rate exactly 3/4", np);
        for (int run = 0; run < 3; run++) {
            for (int i = 0; i < np; i++) pl[i] = pc[i] ? -1 : 1;
            if (run == 1) pl[100] = -pl[100];                           /* one channel error */
            if (run == 2) { pl[100] = -pl[100]; pl[300] = -pl[300]; pl[500] = -pl[500]; }
            int mdp = depuncture_llrs(pl, np, dpl);
            for (int t = 0; t < NSTEPS; t++) { g_l0[t] = dpl[2 * t]; g_l1[t] = dpl[2 * t + 1]; }
            CHECK(mdp == 2 * NSTEPS, "depunctured length %d", mdp);
            CHECK(msg_errors(build_branch_metrics) == 0, "rate-3/4 punctured decode, %d channel errors: message recovered", run == 0 ? 0 : run == 1 ? 1 : 3);
        }
    }

    /* ===================== HB[109]: block interleaver ===================== */
    {
        static const int geo[][2] = { { 4, 5 }, { 24, 45 }, { 1, 17 }, { 17, 1 }, { 8, 8 }, { 7, 13 } };
        for (unsigned g = 0; g < sizeof geo / sizeof *geo; g++) {
            int rows = geo[g][0], cols = geo[g][1], n = rows * cols;
            if (n <= 0 || n > 2000) continue;
            static uint8_t in[2000], tx[2000], rx[2000];
            for (int i = 0; i < n; i++) in[i] = (uint8_t)(i * 31 + 7);              /* distinct-ish markers */
            interleave_block(in, tx, rows, cols);
            int bad = 0; for (int p = 0; p < n; p++) if (tx[p] != in[(p % rows) * cols + p / rows]) bad++;   /* closed form: tx position p reads row p%rows, column p/rows */
            CHECK(bad == 0, "%dx%d: interleaver is the column-major read of a row-major matrix (%d mismatches)", rows, cols, bad);
            deinterleave_block(tx, rx, rows, cols);
            CHECK(memcmp(in, rx, (size_t)n) == 0, "%dx%d: deinterleave(interleave(x)) == x", rows, cols);
            /* it is a true permutation */
            static uint8_t idx_in[2000], idx_tx[2000]; int seen[2000]; memset(seen, 0, sizeof seen);
            for (int i = 0; i < n; i++) idx_in[i] = (uint8_t)(i & 0xFF);
            interleave_block(idx_in, idx_tx, rows, cols);
            int perm = 1; for (int p = 0; p < n; p++) { int src = (p % rows) * cols + p / rows; if (seen[src]++) perm = 0; (void)idx_tx; }
            CHECK(perm, "%dx%d: each source index used exactly once", rows, cols);
        }
        /* burst-spreading claim: a burst of up to `rows` consecutive transmitted bits lands >= cols-1 apart in the original order */
        int rows = 24, cols = 45, n = rows * cols, min_sep = 1 << 30;
        for (int start = 0; start < n; start++) for (int len = 2; len <= rows && start + len <= n; len++) {
            int prev_idx[64]; int cnt = 0;
            for (int p = start; p < start + len; p++) prev_idx[cnt++] = (p % rows) * cols + p / rows;
            for (int a = 0; a < cnt; a++) for (int b = a + 1; b < cnt; b++) { int d = prev_idx[a] > prev_idx[b] ? prev_idx[a] - prev_idx[b] : prev_idx[b] - prev_idx[a]; if (d < min_sep) min_sep = d; }
        }
        CHECK(min_sep == cols - 1, "any burst of <= %d consecutive channel bits is spread to at least cols-1 = %d apart (measured minimum %d)", rows, cols - 1, min_sep);
        CHECK(min_sep > 2 * 7 * 2, "spread (%d coded bits = %d trellis steps) exceeds two constraint lengths", min_sep, min_sep / 2);
    }

    /* ===================== HB[110]: GF(2^8) multiply ===================== */
    {
        static uint8_t lg[256], ex[512];
        gf_tables_init(lg, ex);
        int bad = 0;
        for (unsigned a = 0; a < 256; a++) for (unsigned b = 0; b < 256; b++) if (gf_mul((uint8_t)a, (uint8_t)b, lg, ex) != gf_ref(a, b, 0x11d)) bad++;
        CHECK(bad == 0, "gf_mul == carry-less multiply reduced by x^8+x^4+x^3+x^2+1 (0x11d) for all 65536 operand pairs (%d wrong)", bad);
        CHECK(gf_ref(2, 0x80, 0x11d) == 0x1d && ex[8] == 0x1d && gf_mul(2, 0x80, lg, ex) == 0x1d, "alpha^8 = 0x1d: the tables are the 0x11d field");
        /* alpha = 2 is primitive: the 255 powers are distinct and non-zero */
        int seen[256]; memset(seen, 0, sizeof seen); int prim = 1; for (int i = 0; i < 255; i++) { if (ex[i] == 0 || seen[ex[i]]++) prim = 0; }
        CHECK(prim && ex[255] == 1, "alpha = 2 generates all 255 non-zero elements (0x11d is primitive)");
        /* field properties */
        int fb = 0; for (int t = 0; t < 4000; t++) { uint8_t a = (uint8_t)lcg_next(), b = (uint8_t)lcg_next(), c = (uint8_t)lcg_next();
            if (gf_mul(a, (uint8_t)(b ^ c), lg, ex) != (gf_mul(a, b, lg, ex) ^ gf_mul(a, c, lg, ex))) fb++;
            if (gf_mul(a, b, lg, ex) != gf_mul(b, a, lg, ex)) fb++;
            if (gf_mul(gf_mul(a, b, lg, ex), c, lg, ex) != gf_mul(a, gf_mul(b, c, lg, ex), lg, ex)) fb++; }
        CHECK(fb == 0, "distributive / commutative / associative on 4000 random triples (%d violations)", fb);
        for (unsigned a = 1; a < 256; a++) if (gf_mul((uint8_t)a, ex[255 - lg[a]], lg, ex) != 1) fb++;
        CHECK(fb == 0, "every non-zero element has the inverse alpha^(255-log a)");
        CHECK(gf_mul(0, 77, lg, ex) == 0 && gf_mul(77, 0, lg, ex) == 0 && gf_mul(1, 200, lg, ex) == 200, "0 annihilates, 1 is the identity");
        /* the polynomial matters: the AES field (0x11b) gives different products from the 0x11d tables */
        CHECK(gf_ref(0x57, 0x83, 0x11b) == 0xC1 && gf_ref(0x57, 0x83, 0x11d) != 0xC1, "FIPS-197 example 0x57*0x83 = 0xC1 holds in the 0x11b field only: the table polynomial is not interchangeable");
        /* table size: gf_mul() indexes gf_exp[log a + log b], up to 254 + 254 = 508, so gf_exp is stored doubled (512 entries) */
        int maxsum = 0, over255 = 0, total = 0;
        for (unsigned a = 1; a < 256; a++) for (unsigned b = 1; b < 256; b++) { int s = lg[a] + lg[b]; if (s > maxsum) maxsum = s; if (s > 255) over255++; total++; }
        CHECK(maxsum == 508 && maxsum < 512, "largest index gf_exp[] is read at: %d (inside the 512-entry table)", maxsum);
        int dbl = 1; for (int i = 0; i < 512 - 255; i++) if (ex[i + 255] != ex[i]) dbl = 0;
        CHECK(dbl, "gf_exp[] is stored doubled: gf_exp[i + 255] == gf_exp[i] up to index 511");
        printf("  [info] gf_exp index > 255 for %d of %d non-zero pairs (%.1f %%)\n", over255, total, 100.0 * over255 / total);
        CHECK(over255 * 2 > total - total / 10, "%d of %d non-zero operand pairs read gf_exp[] beyond index 255, so a 256-entry exp table would be overrun for about half of all products", over255, total);
        printf("  [info] tables used by gf_mul: gf_log 256 B + gf_exp 512 B (doubled) = 768 B\n");
    }

    /* ===================== encoder / branch-metric agreement ===================== */
    {
        /* an encoder with mirrored taps does not decode through these branch metrics: encoder and metrics must use the same code */
        lcg_state = 31337u; make_message(); encode_with(enc_mirrored);
        memcpy(g_rx, g_c, sizeof g_rx); hard_llrs(g_rx);
        int e_mirror = msg_errors(build_branch_metrics);
        CHECK(e_mirror > 100, "negative control: a mirrored-tap encoder is not decoded by the 171/133 metrics (%d bit errors)", e_mirror);
        lcg_state = 31337u; make_message(); encode_with(conv_encode_bit);
        memcpy(g_rx, g_c, sizeof g_rx); hard_llrs(g_rx);
        CHECK(msg_errors(build_branch_metrics) == 0, "171/133 encoder + branch metrics: clean channel decodes exactly");
        static const int flips[] = { 30, 111, 210, 333, 480, 602, 721, 850, 960 };
        for (unsigned i = 0; i < sizeof flips / sizeof *flips; i++) g_rx[flips[i]] ^= 1;
        hard_llrs(g_rx);
        CHECK(msg_errors(build_branch_metrics) == 0, "171/133 encoder + branch metrics: 9 isolated channel errors corrected");
    }
    TEST_DONE("test_D_fec");
}
