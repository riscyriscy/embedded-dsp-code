/* Source: Embedded DSP Handbook, Chapter 13 (OFDM and Multicarrier Modulation): HB[83] IFFT/FFT mapping, HB[84] cyclic prefix,
 * HB[85] CFO estimate from the CP, HB[86]/[87] one-tap equalizer + comb pilots, HB[88] PAPR clipping, HB[89] windowing.
 * MIT License.
 * Host build:
 *   - HB[83] calls qpsk_modulate_symbol()/iq_sample_t from Chapter 9 (HB[55]); pulled in via hb_qpsk.h (link src/hb_qpsk.c).
 *   - HB[83] ofdm_tx_scratch is a file-static array used only by example_ofdm_tx_call(); it stays in the .c file.
 *   - stdint.h / math.h are included (the listings use uint8_t, cosf, sqrtf). */
#ifndef HB_OFDM_H
#define HB_OFDM_H

#include <stdint.h>
#include <math.h>
#include "hb_complex.h"
#include "hb_fft.h"
#include "hb_qpsk.h"

// Conceptual TX: map bits to per-subcarrier QPSK symbols, then IFFT to time domain.
// (Reuses complex_t and qpsk_modulate_symbol from Chapter 9.)

// A fixed-size, compile-time-bounded array, not a VLA: MISRA and several embedded
// toolchains (C89-only, some -std flags) reject `complex_t freq_domain[num_subcarriers]`
// when num_subcarriers is a runtime parameter rather than a constant. The bound also sets
// how big a caller-owned workspace below needs to be - see the not-on-the-stack note there.
#define OFDM_MAX_SUBCARRIERS 2048

#define PILOT_SPACING 8   // every 8th subcarrier is a pilot, the rest carry data

/* ---- function prototypes / shared state ---- */
void ifft(complex_t *freq_domain_in, complex_t *time_domain_out, int n);
void ofdm_modulate_symbol(uint8_t *bits, int num_subcarriers, complex_t *freq_domain_scratch, complex_t *time_domain_out);
void example_ofdm_tx_call(uint8_t *bits, int num_subcarriers, complex_t *time_domain_out);
void add_cyclic_prefix(complex_t *ofdm_symbol, int n, int cp_len, complex_t *out_with_cp);
void remove_cp_and_demod(complex_t *rx_with_cp, int n, int cp_len, complex_t *freq_domain_out);
complex_t cfo_estimate_from_cp(const complex_t *rx_with_cp, int n, int cp_len);
void cfo_correct(complex_t *block, int n, float phase_per_sample);
complex_t equalize_subcarrier(complex_t received, complex_t channel_estimate_h);
void estimate_channel_comb_pilots(const complex_t *received, const complex_t *known_pilots, complex_t *channel_estimate, int num_subcarriers);
void ofdm_clip_papr(complex_t *time_domain, int n, float clip_ceiling);
void ofdm_apply_window(complex_t *symbol_with_cp, int total_len, int window_len);

#endif /* HB_OFDM_H */
