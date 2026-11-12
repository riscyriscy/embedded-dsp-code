/* Behavioral tests for the Chapter 5-7 and 10 building blocks: NCO/mixer, QPSK map/slicer, AGC,
 * Gardner TED, timing loop, Costas detector/loop, lock detector, ppm helper, BER, link budget. */
#include <stdint.h>
#include <stdbool.h>
#include "check.h"
#include "fm_qformat.h"
#include "fm_nco.h"
#include "fm_qpsk.h"
#include "fm_agc.h"
#include "fm_timing.h"
#include "fm_costas.h"
#include "fm_loops.h"
#include "fm_lock.h"
#include "fm_ber.h"
#include "fm_rf.h"

static void test_nco(void) {
    /* fs/8 is exactly 2^29; the full circle is exactly 2^32. */
    CHECK(nco_phase_inc_for_freq(1000.0f, 8000.0f) == (1u << 29), "phase_inc(1k @ 8k) = %u", nco_phase_inc_for_freq(1000.0f, 8000.0f));
    CHECK(nco_phase_inc_for_freq(2000.0f, 8000.0f) == (1u << 30), "phase_inc(2k @ 8k)");
    /* Tables: first entries, quarter-turn relation, and sin^2 + cos^2 ~= 1 everywhere. */
    CHECK(cos_table[0] == 32767 && sin_table[0] == 0, "table[0]");
    int worst = 0;
    for (int i = 0; i < NCO_TABLE_SIZE; i++) {
        double r = hypot(cos_table[i], sin_table[i]);
        int d = (int)lrint(fabs(r - 32767.0)); if (d > worst) worst = d;
        double a = 2.0 * M_PI * i / NCO_TABLE_SIZE;
        CHECK(abs(cos_table[i] - (int)lrint(32767.0 * cos(a))) <= 1, "cos_table[%d]", i);
        CHECK(abs(sin_table[i] - (int)lrint(32767.0 * sin(a))) <= 1, "sin_table[%d]", i);
    }
    CHECK(worst <= 2, "table radius error %d LSB", worst);
    /* Mixing a real tone at exactly the NCO frequency: I averages A/2, Q averages ~0 (Q15 out). */
    nco_t nco = { 0 }; nco.base_inc = nco.phase_inc = nco_phase_inc_for_freq(1000.0f, 8192.0f);
    double si = 0, sq = 0; int N = 8192;
    for (int n = 0; n < N; n++) {
        int16_t x = (int16_t)lrint(20000.0 * cos(2.0 * M_PI * 1000.0 * n / 8192.0));
        int32_t i32, q32; quadrature_mix(&nco, x, &i32, &q32);
        si += mixer_to_q15(i32); sq += mixer_to_q15(q32);
    }
    CHECK_NEAR(si / N, 10000.0, 150.0, "mixer I mean (A/2)");   /* 8-bit table phase quantization => small bias */
    CHECK_NEAR(sq / N, 0.0, 150.0, "mixer Q mean");
    /* mixer_to_q15: rounding and saturation, never a wrapping cast. */
    CHECK(mixer_to_q15(0) == 0, "mixer 0");
    CHECK(mixer_to_q15(1 << 14) == 1, "mixer rounds half up");
    CHECK(mixer_to_q15((1 << 14) - 1) == 0, "mixer rounds down below half");
    /* Largest legitimate Q30 products (|x| <= 32768*32768 = 2^30): -32768 * -32768 rounds to +32768 and must clip. */
    CHECK(mixer_to_q15(32768 * 32768) == 32767, "mixer saturates high (-32768 * -32768 case)");
    CHECK(mixer_to_q15(32767 * 32767) == 32766 || mixer_to_q15(32767 * 32767) == 32767, "mixer full-scale product");
    CHECK(mixer_to_q15(-32768 * 32767) == -32767, "mixer full-scale negative product");
    CHECK(mixer_to_q15(-32768 * 32768) == -32768, "mixer -2^30 -> -32768");
}

static void test_qpsk(void) {
    for (uint8_t d = 0; d < 4; d++) {
        iq_sample_t s = qpsk_modulate_symbol(d);
        CHECK(qpsk_demodulate_symbol(s.i, s.q) == d, "round trip dibit %u", d);
        CHECK(qpsk_demodulate_symbol(s.i / 4, s.q / 4) == d, "attenuated round trip %u", d);
    }
    CHECK(qpsk_modulate_symbol(0x7).i == qpsk_table[3].i, "modulate masks to 2 bits");
    /* Unit energy: |point|^2 ~ 1.0 in Q15. */
    for (int d = 0; d < 4; d++) {
        double e = ((double)qpsk_table[d].i * qpsk_table[d].i + (double)qpsk_table[d].q * qpsk_table[d].q) / (32768.0 * 32768.0);
        CHECK_NEAR(e, 1.0, 1e-3, "constellation energy");
    }
    /* Gray property: adjacent points (differ in exactly one axis sign) differ in exactly one bit. */
    for (int a = 0; a < 4; a++) for (int b = 0; b < 4; b++) {
        int axes = (qpsk_table[a].i != qpsk_table[b].i) + (qpsk_table[a].q != qpsk_table[b].q);
        int bits = __builtin_popcount(a ^ b);
        CHECK(axes == bits, "gray mapping: %d vs %d", a, b);
    }
    /* Bit order: I is bit1, Q is bit0 (the Chapter 9 LLR comment depends on this). */
    CHECK(qpsk_demodulate_symbol(-1, 1) == 2 && qpsk_demodulate_symbol(1, -1) == 1, "I=bit1, Q=bit0");
}

static void test_agc(void) {
    agc_t a = { .gain_q15 = 32768, .target_power = 1 << 28, .attack_shift = 4, .release_shift = 8 };
    /* Input power exactly at target: error 0, gain must not move. */
    for (int n = 0; n < 100; n++) agc_update(&a, 1 << 28);
    CHECK(a.gain_q15 == 32768, "agc at target moved: %d", a.gain_q15);
    /* Units: powers are Q30 (full scale^2 = 1 << 30), the gain is Q15. A power error of 1 << 28 (a quarter of
     * full scale^2) moves the gain by (2^28 / 2^30) / 2^shift of unity per sample: 1/4/16 = 1/64 of 32768 = 512 at
     * attack_shift 4 (too strong -> gain falls), and 1/4/256 = 1/1024 of 32768 = 32 at release_shift 8. */
    int32_t g0 = a.gain_q15;
    agc_update(&a, (1 << 28) + (1 << 28));   int32_t gd = g0 - a.gain_q15;
    CHECK(gd == 512, "attack step %d (want 512 = 1/64 of unity)", gd);
    g0 = a.gain_q15; agc_update(&a, 0);
    CHECK(a.gain_q15 - g0 == 32, "release step %d (want 32 = 1/1024 of unity)", a.gain_q15 - g0);
    /* Sign symmetry: a +e and -e error with the same shift move the gain by equal magnitudes. */
    agc_t s1 = { 32768, 1 << 28, 6, 6 }, s2 = s1;
    agc_update(&s1, (1 << 28) - (1 << 26)); agc_update(&s2, (1 << 28) + (1 << 26));
    CHECK(s1.gain_q15 - 32768 == 32 && 32768 - s2.gain_q15 == 32, "agc sign symmetry: %d vs %d", s1.gain_q15 - 32768, 32768 - s2.gain_q15);
    /* Rails: 1 .. 16.0x. */
    agc_t hi = { 32768, 1 << 28, 4, 4 }; for (int n = 0; n < 100000; n++) agc_update(&hi, 0);
    CHECK(hi.gain_q15 == 32768 * 16, "agc upper clamp %d", hi.gain_q15);
    agc_t lo = { 32768, 0, 2, 2 }; for (int n = 0; n < 100000; n++) agc_update(&lo, 1 << 28);
    CHECK(lo.gain_q15 == 1, "agc lower clamp %d", lo.gain_q15);
    CHECK(agc_apply(1000, 32768) == 1000 && agc_apply(1000, 65536) == 2000 && agc_apply(-1000, 16384) == -500, "agc_apply");
    CHECK(agc_apply(30000, 32768 * 16) == 480000, "agc_apply is wide/unsaturated");
    CHECK(agc_sample_to_q15(480000) == 32767 && agc_sample_to_q15(-480000) == -32768 && agc_sample_to_q15(-5) == -5, "agc_sample_to_q15 clips");
    /* Closed loop: measure the power of the sample AFTER agc_apply(). With equal attack and release shifts the
     * average output power settles on target_power, so the gain ends at sqrt(target / input power) whatever the
     * input level (here a sine of amplitude A has power A^2 / 2). */
    const double amps[] = { 4000.0, 8000.0, 16000.0 };
    for (size_t k = 0; k < sizeof amps / sizeof amps[0]; k++) {
        agc_t c = { .gain_q15 = 32768, .target_power = 1 << 28, .attack_shift = 6, .release_shift = 6 };
        double pw = 0; int cnt = 0;
        for (int n = 0; n < 60000; n++) {
            int16_t x = (int16_t)lrint(amps[k] * sin(2 * M_PI * 0.0123 * n));
            int32_t o = agc_sample_to_q15(agc_apply(x, c.gain_q15));
            agc_update(&c, o * o);
            if (n >= 40000) { pw += (double)o * o; cnt++; }
        }
        double want_gain = sqrt(268435456.0 / (amps[k] * amps[k] / 2.0));
        CHECK_NEAR(c.gain_q15 / 32768.0, want_gain, 0.01 * want_gain, "closed-loop AGC settled gain");
        CHECK_NEAR(pw / cnt, 268435456.0, 0.02 * 268435456.0, "closed-loop AGC output power vs target");
    }
}

static void test_timing(void) {
    /* Gardner on a clean +A -> -A transition: mid sample at the zero crossing gives 0 error. */
    gardner_ted_t t = { 0, 20000 };
    CHECK(gardner_error(&t, 0, -20000) == 0, "on-time Gardner error is zero");
    /* Sample late: mid already negative on a +->- transition => error sign flips relative to early. */
    gardner_ted_t te = { 0, 20000 }, tl = { 0, 20000 };
    int32_t e_early = gardner_error(&te, 8000, -20000);    /* mid still positive */
    int32_t e_late  = gardner_error(&tl, -8000, -20000);   /* mid already negative */
    CHECK(e_early * e_late < 0, "Gardner early/late have opposite sign: %d / %d", e_early, e_late);
    CHECK(t.prev_sample == -20000, "Gardner stores the decision sample");
    CHECK(timing_interpolate_linear(100, 300, 0) == 100 && timing_interpolate_linear(100, 300, 16384) == 200
          && timing_interpolate_linear(300, 100, 16384) == 200, "linear interpolator");
    timing_loop_t l = { 0, 65536 / 2, 65536 / 8 };
    int32_t out = timing_loop_update(&l, 1000);
    CHECK(l.integrator == 125 && out == 500 + 125, "timing loop PI: int=%d out=%d", l.integrator, out);
    out = timing_loop_update(&l, 0);
    CHECK(out == 125, "integrator holds with zero error");
}

static void test_costas(void) {
    /* The detector is invariant to which quadrant the point is in, and signed with the rotation. */
    double th = 0.05;   /* rad */
    for (int d = 0; d < 4; d++) {
        double a = atan2(qpsk_table[d].q, qpsk_table[d].i) + th;
        int32_t i = (int32_t)lrint(32767.0 * cos(a)), q = (int32_t)lrint(32767.0 * sin(a));
        int32_t e = costas_phase_error(i, q);
        CHECK(e > 0, "Costas error positive for +rotation, quadrant %d: %d", d, e);
        a -= 2 * th; i = (int32_t)lrint(32767.0 * cos(a)); q = (int32_t)lrint(32767.0 * sin(a));
        CHECK(costas_phase_error(i, q) < 0, "Costas error negative for -rotation, quadrant %d", d);
        CHECK(costas_phase_error(qpsk_table[d].i, qpsk_table[d].q) == 0, "Costas error zero on ideal point %d", d);
    }
    /* Worked-example gains for Kd = 2 * 5600 at 2 samples/symbol: Kp * (2^32 / 2 pi) / Kd * 2^16 and the same / sps. */
    costas_loop_t c = { 0, 0, COSTAS_KP_Q16(0.05192, 11200.0), COSTAS_KI_Q16(0.001385, 11200.0, 2) };
    CHECK(c.kp == 207671349, "COSTAS_KP_Q16 = %d", c.kp);
    CHECK(c.ki == 2769885, "COSTAS_KI_Q16 = %d", c.ki);
    CHECK(FLOAT_TO_Q16(1.0f) == 65536 && FLOAT_TO_Q16(-0.5f) == -32768, "FLOAT_TO_Q16");
    /* 64-bit products: kp * err is ~2^42 here, far past int32. */
    costas_loop_t big = c; costas_update(&big, 20000);
    CHECK_NEAR(big.phase, (double)c.kp * 20000.0 / 65536.0, 1.0, "costas_update proportional term (64-bit product)");
    CHECK_NEAR(big.freq, (double)c.ki * 20000.0 / 65536.0, 1.0, "costas_update integrator term (64-bit product)");
    CHECK(big.phase > 0 && big.freq > 0, "costas_update sign");
    nco_t nco = { 1000, 5000, 5000 };
    costas_update(&c, 65536);
    CHECK(c.freq == c.ki && c.phase == c.kp, "costas_update split: freq %d phase %d", c.freq, c.phase);
    costas_apply_correction(&c, &nco);
    CHECK(nco.phase_inc == 5000u + (uint32_t)c.freq, "rate set, not accumulated");
    CHECK(nco.phase_acc == 1000u + (uint32_t)c.kp && c.phase == 0, "one-time nudge consumed");
    costas_apply_correction(&c, &nco);
    CHECK(nco.phase_acc == 1000u + (uint32_t)c.kp, "nudge not re-applied");
    CHECK(nco.phase_inc == 5000u + (uint32_t)c.freq, "rate stays set (idempotent)");

    /* Pull-in on a constant frequency offset, one update per symbol, against an ideal detector input (the QPSK
     * point rotated by the loop's own phase error). The same Kp/Ki in the linear model
     *   fe += Ki*e; theta += fe + Kp*e   (radians, per symbol)
     * settle within 5% of the offset at symbol 78, whatever the offset; the fixed-point loop must agree. */
    const double Kp = 0.05192, Ki = 0.001385, a_amp = 5600.0, W = 4294967296.0 / (2.0 * M_PI);
    const double offs[] = { 0.01, 0.001, 0.0001 };   /* rad/symbol */
    for (size_t k = 0; k < sizeof offs / sizeof offs[0]; k++) {
        int model_settle = 1; { double fe = 0, th = 0; for (int n = 1; n < 400; n++) { double e = offs[k] * n - th; fe += Ki * e; th += fe + Kp * e; if (fabs(fe - offs[k]) > 0.05 * offs[k]) model_settle = n + 1; } }
        costas_loop_t cl = { 0, 0, COSTAS_KP_Q16(Kp, 2.0 * a_amp), COSTAS_KI_Q16(Ki, 2.0 * a_amp, 2) };
        nco_t nc = { 0, 0, 0 };
        int settle = 1; double th_true = 0;
        for (int n = 1; n < 400; n++) {
            th_true += offs[k];
            double d = th_true - (double)(int32_t)nc.phase_acc / W;
            int32_t ii = (int32_t)lrint(a_amp * sqrt(2.0) * cos(M_PI / 4 + d)), qq = (int32_t)lrint(a_amp * sqrt(2.0) * sin(M_PI / 4 + d));
            costas_update(&cl, costas_phase_error(ii, qq));
            costas_apply_correction(&cl, &nc);
            nc.phase_acc += 2u * (uint32_t)cl.freq;   /* the 2 samples of this symbol at the corrected rate (base rate 0) */
            if (fabs(2.0 * cl.freq / W - offs[k]) > 0.05 * offs[k]) settle = n + 1;
        }
        CHECK(model_settle == 78, "linear model settles at symbol %d (book: 78)", model_settle);
        CHECK(settle >= model_settle - 1 && settle <= model_settle + 1, "offset %g rad/symbol: fixed-point loop settles at symbol %d, linear model at %d", offs[k], settle, model_settle);
    }
}

static void test_lock_and_misc(void) {
    lock_detector_t ld = { .smoothed_abs_error = 100000, .alpha_q15 = 8192, .threshold = 500, .required_count = 8 };
    int first_lock = -1;
    for (int n = 0; n < 200; n++) if (lock_detector_update(&ld, (n & 1) ? 100 : -100) && first_lock < 0) first_lock = n;
    CHECK(first_lock > 8, "lock declared immediately (n=%d); needs N consecutive smoothed samples", first_lock);
    CHECK(first_lock > 0 && first_lock < 100, "lock detector never/slowly locked (n=%d)", first_lock);
    CHECK(lock_detector_update(&ld, 1000000) == false && ld.consecutive_count == 0, "large error resets the lock counter");

    /* ppm helper: 40 ppm of a 1 MHz carrier at fs = 19.2 kHz. */
    uint32_t off = ppm_to_phase_inc_offset(1.0e6f, 40.0f, 19200.0f);
    CHECK_NEAR((double)off, 40.0 / 19200.0 * 4294967296.0, 1.0e3, "ppm_to_phase_inc_offset");
    CHECK(FLOAT_TO_Q16(1.0f) == 65536 && FLOAT_TO_Q16(-0.5f) == -32768, "FLOAT_TO_Q16");

    ber_counter_t b = { 0, 0 };
    for (int n = 0; n < 1000; n++) ber_test_bit(&b, n & 1, (n % 100 == 0) ? !(n & 1) : (n & 1));
    CHECK(b.bits_tested == 1000 && b.bit_errors == 10 && fabsf(ber_current_rate(&b) - 0.01f) < 1e-6f, "BER counter");
    CHECK(required_bits_for_ber_confidence(1e-6, 10) == 10000000ULL, "10 errors at 1e-6 needs 10,000,000 bits (book text)");

    /* Link budget: 20 dBm + 3 + 3 - 100 - 2 = -76 dBm; noise -100 => 24 dB SNR; need 10 => +14. */
    CHECK(link_budget_margin_db(20, 3, 100, 3, 2, 10, -100) == 14, "link margin");
    CHECK(link_budget_margin_db(0, 0, 120, 0, 0, 10, -100) < 0, "negative margin => link doesn't close");
}

int main(void) {
    test_nco(); test_qpsk(); test_agc(); test_timing(); test_costas(); test_lock_and_misc();
    TEST_DONE("test_nco_qpsk");
}
