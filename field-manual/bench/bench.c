/* Instructions per call of each stage of the Field Manual chain, counted on the QEMU machine
 * mps2-an386 (Cortex-M4 with FPU) with -icount shift=0, where every instruction takes 1 ns of
 * virtual time. SysTick runs from the 25 MHz system clock, so one SysTick count is 40
 * instructions. Each stage runs CALLS times on prepared input; the count of an empty loop
 * with the same input is subtracted. On a board, bench_now() reads DWT->CYCCNT instead and
 * the same table gives cycles (BENCH_DWT). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "fm_adc.h"
#include "fm_simple_filters.h"
#include "fm_fir.h"
#include "fm_biquad.h"
#include "fm_nco.h"
#include "fm_cic.h"
#include "fm_goertzel.h"
#include "fm_agc.h"
#include "fm_loopback.h"
#include "fm_fec.h"
#include "fm_qpsk.h"

#define CALLS 4096u
#define SYST_CSR (*(volatile uint32_t *)0xE000E010u)
#define SYST_RVR (*(volatile uint32_t *)0xE000E014u)
#define SYST_CVR (*(volatile uint32_t *)0xE000E018u)
#define DWT_CTRL   (*(volatile uint32_t *)0xE0001000u)
#define DWT_CYCCNT (*(volatile uint32_t *)0xE0001004u)
#define DEMCR      (*(volatile uint32_t *)0xE000EDFCu)

static void bench_start(void) {
#ifdef BENCH_DWT
    DEMCR |= 1u << 24; DWT_CYCCNT = 0; DWT_CTRL |= 1u;   /* trace enable, then the cycle counter */
#else
    SYST_RVR = 0x00FFFFFFu; SYST_CVR = 0; SYST_CSR = 5u;  /* processor clock, no interrupt, on */
    while (SYST_CVR == 0u) { }                            /* the first count loads the reload value */
#endif
}
static uint32_t bench_now(void) {
#ifdef BENCH_DWT
    return DWT_CYCCNT;
#else
    return (0x00FFFFFFu - SYST_CVR) * 40u;              /* down counter, 40 instructions a count;
                                                           a stage stays far below 2^24 counts */
#endif
}

void loopback_reset(qpsk_loopback_rx_t *out, float carrier_hz);
static uint16_t raw12[CALLS];
static int16_t x15[CALLS];
static int32_t x32[CALLS];
static uint8_t bits[CALLS];
static volatile int32_t sink;
static uint32_t empty;

static void line(const char *stage, const char *unit, uint32_t total) {
    uint32_t net = total > empty ? total - empty : 0;
    uint32_t per100 = (uint32_t)(((uint64_t)net * 100u + CALLS / 2u) / CALLS);
    printf("%-34s %-10s %5lu.%02lu\n", stage, unit, (unsigned long)(per100 / 100u), (unsigned long)(per100 % 100u));
}

#define MEASURE(stage, unit, body)                       \
    do {                                                 \
        uint32_t t0 = bench_now();                       \
        for (uint32_t n = 0; n < CALLS; n++) { body; }   \
        line(stage, unit, bench_now() - t0);             \
    } while (0)

extern void initialise_monitor_handles(void);
int main(void) {
    initialise_monitor_handles();
    uint32_t lcg = 12345u;
    for (uint32_t n = 0; n < CALLS; n++) {               /* a 1 kHz tone at 8 kHz, small noise */
        lcg = lcg * 1664525u + 1013904223u;
        float v = 0.6f * sinf(2.0f * 3.14159265f * 1000.0f * (float)n / 8000.0f) + (float)((int32_t)(lcg >> 20) - 2048) / 40960.0f;
        raw12[n] = (uint16_t)(2048 + (int32_t)(v * 2000.0f));
        x15[n] = adc_to_q15(raw12[n]);
        x32[n] = x15[n];
        bits[n] = (uint8_t)(lcg >> 31);
    }
    bench_start();
    { uint32_t t0 = bench_now(); for (uint32_t n = 0; n < CALLS; n++) { sink = x15[n]; } empty = bench_now() - t0; }
    {   /* calibration: a loop of two instructions, subs and bne, run 1000000 times */
        uint32_t t0 = bench_now(), k = 1000000u;
        __asm volatile("1: subs %0, %0, #1\n bne 1b" : "+r"(k) : : "cc");
        printf("calibration: 2000000 instructions counted as %lu\n", (unsigned long)(bench_now() - t0));
    }
    printf("stage                              unit       instructions\n");

    MEASURE("adc_to_q15", "sample", sink = adc_to_q15(raw12[n]));
    dc_blocker_t dc = { .r_q15 = 32604 };
    MEASURE("dc_blocker_process", "sample", sink = dc_blocker_process(&dc, x32[n]));
    one_pole_lpf_t lp = { .alpha_q15 = 25956 };
    MEASURE("one_pole_lpf_process", "sample", sink = one_pole_lpf_process(&lp, x32[n]));
    fir_q15_t fir; fir_init_q15(&fir, fir_coeffs_q15);
    MEASURE("fir_process_q15, 19 taps", "sample", sink = fir_process_q15(&fir, x15[n]));
    fir_init_q15(&fir, fir_coeffs_q15);
    MEASURE("fir_process_symmetric_q15, 19 taps", "sample", sink = fir_process_symmetric_q15(&fir, x15[n]));
    biquad_q15_t bq; memset(&bq, 0, sizeof bq); design_notch_biquad(&bq, 1000.0f, 8000.0f, 5.0f);
    MEASURE("biquad_process_q15", "sample", sink = biquad_process_q15(&bq, x15[n]));
    nco_t nco = { 0 }; nco.base_inc = nco.phase_inc = nco_phase_inc_for_freq(1000.0f, 8000.0f);
    MEASURE("quadrature_mix and mixer_to_q15", "sample",
            int32_t i30; int32_t q30; quadrature_mix(&nco, x15[n], &i30, &q30);
            sink = mixer_to_q15(i30) + mixer_to_q15(q30));
    cic_decimator_t cic; memset(&cic, 0, sizeof cic); int32_t cic_out;
    MEASURE("cic_decimator_process, M = 64", "sample", sink = cic_decimator_process(&cic, x32[n], 64, &cic_out));
    goertzel_t g; goertzel_init(&g, 32, 256);
    MEASURE("goertzel_process_sample (float)", "sample", goertzel_process_sample(&g, (float)x15[n] * (1.0f / 32768.0f)));
    agc_t agc = { .gain_q15 = 32768, .target_power = 1 << 28, .attack_shift = 4, .release_shift = 8 };
    MEASURE("agc_apply and agc_update", "sample",
            int16_t o = agc_sample_to_q15(agc_apply(x32[n], agc.gain_q15)); sink = agc_update(&agc, o * o));
    MEASURE("qpsk_demodulate_symbol", "symbol", sink = qpsk_demodulate_symbol(x32[n], x32[(n + 7) % CALLS]));
    int32_t l1, l0;
    MEASURE("qpsk_soft_llr", "symbol", qpsk_soft_llr(x32[n], x32[(n + 7) % CALLS], &l1, &l0); sink = l1 + l0);
    uint8_t o0, o1;
    MEASURE("conv_encode_bit", "bit", conv_encode_bit(bits[n], &o0, &o1); sink = o0 + o1);
    static int32_t pm[NUM_STATES]; static int32_t bm[NUM_STATES][2]; static uint8_t win[NUM_STATES];
    for (int s = 0; s < NUM_STATES; s++) { bm[s][0] = s & 7; bm[s][1] = 7 - (s & 7); }
    MEASURE("viterbi_acs_step, 64 states", "bit", viterbi_acs_step(pm, bm, win); sink = win[n & 63]);
    static qpsk_loopback_rx_t rx; loopback_reset(&rx, 2400.0f);
    MEASURE("qpsk_loopback_process_sample", "sample", qpsk_loopback_process_sample(&rx, x32[n], (uint8_t)(n & 3)));
    return 0;
}
