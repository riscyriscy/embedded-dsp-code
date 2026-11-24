/* Chapter 22 (Test and Validation), HB[111]..HB[116] (src/hb_testkit.c).
 * HB[117]/[118] (the complete loopback chain) are tested separately by the coordinator.
 * References: closed forms (EVM of known-size error vectors; EVM of AWGN = sqrt(noise power / signal power)); text captured from the
 * UART-dump functions by redirecting stdout and parsed back; sizing arithmetic done in exact integer/long double terms. */
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "check.h"
#include "hb_testkit.h"

/* HAL symbol HB[116] calls (the board's ADC read): test stub */
static int adc_calls = 0;
int32_t adc_read_rx_sample(void) { adc_calls++; return 424242; }

/* run fn with stdout redirected into buf */
static char cap[1 << 20];
#define CAPTURE(stmt) do { fflush(stdout); FILE *tf_ = tmpfile(); int sv_ = dup(1); dup2(fileno(tf_), 1); stmt; fflush(stdout); dup2(sv_, 1); close(sv_); \
    rewind(tf_); size_t n_ = fread(cap, 1, sizeof cap - 1, tf_); cap[n_] = 0; fclose(tf_); } while (0)

static double gauss(void) { double a = (lcg_next() + 0.5) / 16777216.0, b = (lcg_next() + 0.5) / 16777216.0; return sqrt(-2.0 * log(a)) * cos(6.283185307179586 * b); }

int main(void) {
    /* ===================== HB[111] BER counter ===================== */
    {
        ber_counter_t c = { 0, 0 };
        uint8_t tx[1000], rx[1000]; int errs = 0;
        for (int i = 0; i < 1000; i++) { tx[i] = (lcg_next() >> 5) & 1; rx[i] = tx[i]; if (i % 37 == 5) { rx[i] ^= 1; errs++; } }
        for (int i = 0; i < 1000; i++) ber_test_bit(&c, tx[i], rx[i]);
        CHECK(c.bits_tested == 1000 && (int)c.bit_errors == errs, "counter: %llu errors in %llu bits (expected %d in 1000)", (unsigned long long)c.bit_errors, (unsigned long long)c.bits_tested, errs);
        CHECK_NEAR(ber_current_rate(&c), errs / 1000.0, 1e-7, "BER = errors / bits");
        ber_counter_t z = { 0, 0 }; double r = ber_current_rate(&z);
        CHECK(r == -1.0, "ber_current_rate() before any bit is tested is -1.0 (no data), not 0/0 = NaN (got %f)", r);
        ber_counter_t all = { 0, 0 }; for (int i = 0; i < 100; i++) ber_test_bit(&all, 0, 1);
        CHECK_NEAR(ber_current_rate(&all), 1.0, 1e-9, "all bits wrong -> 1.0");
        /* sizing: a 1e-9 BER test needs more bits than a 32-bit counter holds, so the counters are 64-bit */
        uint64_t need = required_bits_for_ber_confidence(1e-9, 10);
        CHECK(need > (uint64_t)UINT32_MAX && sizeof c.bits_tested == 8 && sizeof c.bit_errors == 8, "verifying BER 1e-9 with 10 errors needs %llu bits (> 2^32 - 1 = %u); both counters are 64-bit", (unsigned long long)need, UINT32_MAX);
        ber_counter_t w = { 3, UINT32_MAX }; ber_test_bit(&w, 0, 0); ber_test_bit(&w, 0, 1);
        CHECK(w.bits_tested == (uint64_t)UINT32_MAX + 2 && w.bit_errors == 4, "the counter crosses 2^32 bits without wrapping (%llu bits, %llu errors)", (unsigned long long)w.bits_tested, (unsigned long long)w.bit_errors);
        CHECK_NEAR(ber_current_rate(&w), 4.0 / ((double)UINT32_MAX + 2), 1e-15, "rate after crossing 2^32 bits");
        ber_counter_t big = { 10, 10000000000ULL };
        CHECK_NEAR(ber_current_rate(&big), 1e-9, 1e-18, "10 errors in 1e10 bits reads 1e-9");
        printf("  [info] 2^32 bits = %.1f minutes of a 1 Mbps link\n", 4294967296.0 / 1e6 / 60.0);
        /* the counters accumulate over the whole run: 500 bad then 500 good bits read 0.5, and zeroing the struct starts a new measurement */
        ber_counter_t cum = { 0, 0 }; for (int i = 0; i < 500; i++) ber_test_bit(&cum, 0, 1); for (int i = 0; i < 500; i++) ber_test_bit(&cum, 0, 0);
        CHECK_NEAR(ber_current_rate(&cum), 0.5, 1e-9, "cumulative: 500 bad then 500 good reads 0.5");
        memset(&cum, 0, sizeof cum); ber_test_bit(&cum, 0, 0);
        CHECK(ber_current_rate(&cum) == 0.0, "after zeroing the struct a new measurement starts from scratch");
    }

    /* ===================== HB[112] required_bits_for_ber_confidence ===================== */
    {
        CHECK(required_bits_for_ber_confidence(1e-6, 10) == 10000000ULL, "example: BER 1e-6, 10 errors -> 10,000,000 bits (got %llu)", (unsigned long long)required_bits_for_ber_confidence(1e-6, 10));
        CHECK(required_bits_for_ber_confidence(1e-3, 100) == 100000ULL, "BER 1e-3, 100 errors -> 100,000");
        CHECK(required_bits_for_ber_confidence(0.5, 10) == 20ULL, "BER 0.5, 10 errors -> 20");
        /* exact answers (in integer arithmetic) for decimal targets: required = errors * 10^k */
        struct { double ber; uint32_t e; uint64_t exact; } cases[] = {
            { 1e-5, 10, 1000000ULL }, { 1e-5, 1000, 100000000ULL }, { 1e-9, 1000, 1000000000000ULL }, { 1e-4, 10, 100000ULL }, { 1e-7, 10, 100000000ULL },
        };
        int off = 0;
        for (unsigned i = 0; i < sizeof cases / sizeof *cases; i++) {
            uint64_t got = required_bits_for_ber_confidence(cases[i].ber, cases[i].e);
            if (got != cases[i].exact) { off++; printf("  [info] errors=%u BER=%g: returns %llu, exact %llu\n", cases[i].e, cases[i].ber, (unsigned long long)got, (unsigned long long)cases[i].exact); }
        }
        CHECK(off == 0, "round decimal cases return the exact length (10/1e-5 -> 1,000,000; 1000/1e-5 -> 100,000,000; 1000/1e-9 -> 1e12): %d of 5 differ", off);
        CHECK(required_bits_for_ber_confidence(3e-6, 50) == 16666667ULL, "50 errors at 3e-6: 16,666,666.67 rounds up to 16,666,667");
        /* a required length is never short: result * target >= errors in exact arithmetic, and one bit fewer is short (long double check) */
        int never_short = 1;
        for (int t = 0; t < 2000; t++) {
            uint32_t e = 1 + lcg_next() % 1000; double ber = (1 + lcg_next() % 999) * pow(10.0, -(int)(3 + lcg_next() % 7));
            uint64_t n = required_bits_for_ber_confidence(ber, e);
            if ((long double)n * (long double)ber < (long double)e * (1.0L - 1e-15L)) never_short = 0;
            if ((long double)(n - 1) * (long double)ber >= (long double)e * (1.0L + 1e-15L)) never_short = 0;
        }
        CHECK(never_short, "required length is the smallest integer n with n * BER >= errors, on 2000 random targets");
        CHECK(required_bits_for_ber_confidence(1e-9, 10) == 10000000000ULL, "BER 1e-9 / 10 errors = 1e10 bits");
        /* the comment's arithmetic: 10,000,000 bits at 1 Mbps is 10 seconds */
        CHECK_NEAR(10000000.0 / 1e6, 10.0, 1e-12, "10,000,000 bits at 1 Mbps = 10 s (comment)");
        /* prose: 1,000 error-free bits only bounds BER below ~1 in 1,000: 'rule of three' 95%% upper bound is 3/n = 0.3%% */
        CHECK_NEAR(3.0 / 1000, 0.003, 1e-12, "rule of three for 0 errors in 1000 bits: 0.3%% (prose: 'roughly 1 in 1,000')");
    }

    /* ===================== HB[113] constellation dump ===================== */
    {
        int32_t iq[6] = { 0, 1, -1, 2147483647, -2147483647 - 1, 12345 }, qq[6] = { 7, -8, 0, -2147483647 - 1, 2147483647, -54321 };
        CAPTURE(dump_iq_buffer_uart(iq, qq, 6));
        const char *want = "0,7\n1,-8\n-1,0\n2147483647,-2147483648\n-2147483648,2147483647\n12345,-54321\n";
        CHECK(strcmp(cap, want) == 0, "constellation dump text:\n%s", cap);
        CAPTURE(dump_iq_buffer_uart(iq, qq, 0));
        CHECK(cap[0] == 0, "count 0 prints nothing");
    }

    /* ===================== HB[114] eye diagram dump ===================== */
    {
        enum { SPS = 4, NSYM = 5, TOT = SPS * NSYM + 3 };           /* 3 trailing samples: a partial symbol */
        int32_t w[TOT]; for (int i = 0; i < TOT; i++) w[i] = (i * 37) % 101 - 50;
        CAPTURE(dump_eye_diagram_uart(w, TOT, SPS));
        int lines = 0, bad = 0; const char *p = cap;
        while (*p) {
            int k; long a; int n;
            if (sscanf(p, "%d,%ld%n", &k, &a, &n) != 2) { bad++; break; }
            int idx = lines;                                          /* trace number = idx / SPS, sample = idx % SPS */
            if (k != idx % SPS || a != w[(idx / SPS) * SPS + (idx % SPS)]) bad++;
            lines++; p += n; if (*p == '\n') p++; else { bad++; break; }
        }
        CHECK(bad == 0 && lines == SPS * NSYM, "eye dump: %d lines (want %d), time index cycles 0..%d, amplitudes in order (%d bad); partial last symbol dropped", lines, SPS * NSYM, SPS - 1, bad);
        /* an eye of a clean pulse train: every trace identical at each k (traces overlay exactly) */
        enum { S2 = 8, N2 = 40 };
        static int32_t y[S2 * N2]; static const int shape[S2] = { 0, 300, 900, 1000, 1000, 900, 300, 0 };
        for (int i = 0; i < S2 * N2; i++) y[i] = shape[i % S2];
        CAPTURE(dump_eye_diagram_uart(y, S2 * N2, S2));
        int same = 1; p = cap; for (int i = 0; i < S2 * N2; i++) { int k; long a; int n; sscanf(p, "%d,%ld%n", &k, &a, &n); p += n + 1; if (a != shape[k]) same = 0; }
        CHECK(same, "noiseless pulse train: all 40 traces overlay at the same amplitude for each k");
        CAPTURE(dump_eye_diagram_uart(w, 3, SPS));
        CHECK(cap[0] == 0, "fewer samples than one symbol: nothing printed");
    }

    /* ===================== HB[115] EVM ===================== */
    {
        /* known error vectors: every point displaced by 10% of its own magnitude -> EVM 10% (QPSK, equal magnitudes) */
        enum { N = 4000 };
        static complex_t rx[N], id[N];
        for (int n = 0; n < N; n++) {
            id[n].re = (lcg_next() & 1) ? 0.70710678f : -0.70710678f; id[n].im = (lcg_next() & 1) ? 0.70710678f : -0.70710678f;
            float ang = (float)(6.2831853 * (lcg_next() / 16777216.0));
            rx[n].re = id[n].re + 0.1f * cosf(ang); rx[n].im = id[n].im + 0.1f * sinf(ang);                /* |error| = 0.1, |ideal| = 1 */
        }
        CHECK_NEAR(compute_evm_percent(rx, id, N), 10.0, 1e-3, "constant |error| = 0.1 on unit-magnitude QPSK -> EVM 10%%");
        CHECK_NEAR(compute_evm_percent(id, id, N), 0.0, 1e-9, "received == ideal -> 0%%");
        /* scale invariance: normalized by reference power */
        for (int n = 0; n < N; n++) { rx[n].re *= 1000; rx[n].im *= 1000; id[n].re *= 1000; id[n].im *= 1000; }
        CHECK_NEAR(compute_evm_percent(rx, id, N), 10.0, 1e-2, "scaling both by 1000 leaves EVM unchanged");
        /* AWGN: EVM = sqrt(noise power / signal power) = sigma*sqrt(2)/|s| (closed form) */
        for (int n = 0; n < N; n++) {
            id[n].re = (lcg_next() & 1) ? 1.0f : -1.0f; id[n].im = (lcg_next() & 1) ? 1.0f : -1.0f;       /* |s|^2 = 2 */
            float sg = 0.05f; rx[n].re = id[n].re + sg * (float)gauss(); rx[n].im = id[n].im + sg * (float)gauss();
        }
        double want = 100.0 * sqrt(2.0 * 0.05 * 0.05 / 2.0);                                              /* 5.0 % */
        CHECK_NEAR(compute_evm_percent(rx, id, N), want, 0.15, "AWGN sigma 0.05 per axis on |s|^2 = 2: EVM = 5.0%% (statistical, 4000 symbols)");
        /* independent double-precision computation on 16-QAM with noise */
        for (int n = 0; n < N; n++) {
            static const float lv[4] = { -3, -1, 1, 3 };
            id[n].re = lv[(lcg_next() >> 3) & 3]; id[n].im = lv[(lcg_next() >> 3) & 3];
            rx[n].re = id[n].re + 0.2f * (float)gauss(); rx[n].im = id[n].im + 0.2f * (float)gauss();
        }
        double es = 0, rs = 0, mag = 0;
        for (int n = 0; n < N; n++) { double a = (double)rx[n].re - id[n].re, b = (double)rx[n].im - id[n].im; es += a * a + b * b; rs += (double)id[n].re * id[n].re + (double)id[n].im * id[n].im; mag += sqrt((double)id[n].re * id[n].re + (double)id[n].im * id[n].im); }
        CHECK_NEAR(compute_evm_percent(rx, id, N), 100.0 * sqrt(es / rs), 1e-3, "16-QAM: matches a double-precision evaluation of the RMS-normalised definition");
        /* the normalisation matters: dividing by the mean magnitude instead of the RMS magnitude gives a different number */
        double evm_mag = 100.0 * sqrt(es / N) / (mag / N), evm_rms = 100.0 * sqrt(es / rs);
        CHECK(fabs(evm_mag - evm_rms) / evm_rms > 0.03, "for 16-QAM, mean-magnitude normalisation (%.2f%%) and the code's RMS normalisation (%.2f%%) differ by %.1f%%", evm_mag, evm_rms, 100 * fabs(evm_mag - evm_rms) / evm_rms);
    }

    /* ===================== HB[116] digital loopback ===================== */
    {
        radio_config_t on = { true }, off = { false };
        adc_calls = 0;
        CHECK(radio_get_rx_sample(&on, 1234) == 1234 && adc_calls == 0, "loopback on: returns the TX sample and never touches the ADC");
        CHECK(radio_get_rx_sample(&off, 1234) == 424242 && adc_calls == 1, "loopback off: reads the ADC (stub value), ignores the TX sample");
        int ok = 1; for (int32_t v = -100000; v <= 100000; v += 997) if (radio_get_rx_sample(&on, v) != v) ok = 0;
        CHECK(ok, "loopback is bit-exact for any sample value");
    }
    TEST_DONE("test_D_testkit");
}
