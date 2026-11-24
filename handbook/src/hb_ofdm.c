/* Source: Embedded DSP Handbook, Chapter 13 (OFDM and Multicarrier Modulation): HB[83] IFFT/FFT mapping, HB[84] cyclic prefix,
 * HB[85] CFO estimate from the CP, HB[86]/[87] one-tap equalizer + comb pilots, HB[88] PAPR clipping, HB[89] windowing.
 * MIT License.
 * Host build:
 *   - HB[83] calls qpsk_modulate_symbol()/iq_sample_t from Chapter 9 (HB[55]); pulled in via hb_qpsk.h (link src/hb_qpsk.c).
 *   - HB[83] ofdm_tx_scratch is a file-static array used only by example_ofdm_tx_call(); it stays in the .c file.
 *   - stdint.h / math.h are included (the listings use uint8_t, cosf, sqrtf). */
#include "hb_ofdm.h"

// ifft(): the inverse transform, defined here in terms of the same forward fft() from
// Chapter 7 rather than a second, separately maintained butterfly implementation. The
// standard identity: conjugate the input, run the ordinary forward FFT unmodified,
// conjugate the result, and scale by 1/N. This reuses Chapter 7's fft() exactly as written.
//
// Conjugates freq_domain_in IN PLACE instead of copying it into a local scratch array
// first: freq_domain_in isn't read again by this caller after the call returns, and an
// extra OFDM_MAX_SUBCARRIERS-entry complex_t array is 2048 * 8 bytes = 16 KB, entirely
// the wrong place to put it, on the very stack frame a Cortex-M's default stack (often a
// handful of KB) least affords it. If a caller needs its frequency-domain array preserved
// past this call, that caller conjugates its own copy first; this function doesn't grow a
// hidden buffer to do that on every caller's behalf.
void ifft(complex_t *freq_domain_in, complex_t *time_domain_out, int n) {
    for (int i = 0; i < n; i++) {
        freq_domain_in[i].im = -freq_domain_in[i].im;
    }
    fft(freq_domain_in, time_domain_out, n);   // Chapter 7's forward FFT, called unmodified
    for (int i = 0; i < n; i++) {
        float re = time_domain_out[i].re;
        time_domain_out[i].re =  re / n;
        time_domain_out[i].im = -time_domain_out[i].im / n;
    }
}

// freq_domain_scratch is CALLER-supplied, not a local array: the same OFDM_MAX_SUBCARRIERS
// * 8 bytes = 16 KB that ifft() above used to hide in a local array, just moved to wherever
// the caller actually wants that storage duration to live (typically a static or BSS
// buffer declared once, the same pattern Chapter 4's Lab 0 already uses for its own
// persistent FFT buffers) instead of growing fresh on this function's stack frame every
// call. Not reentrant as written: two calls sharing one freq_domain_scratch buffer from
// two contexts (two RTOS tasks, an ISR and a task) will corrupt each other's in-flight
// symbol; give each concurrent caller its own scratch buffer if that's a real possibility,
// the same reentrancy tradeoff Chapter 4's ping-pong DMA state carries.
void ofdm_modulate_symbol(uint8_t *bits, int num_subcarriers,
                            complex_t *freq_domain_scratch, complex_t *time_domain_out) {
    for (int k = 0; k < num_subcarriers; k++) {
        // Packed 2 bits per subcarrier, 4 subcarriers per byte, LSB-first within the byte:
        // bits[k/4] selects the byte, (k%4)*2 selects which 2-bit field within it. This is
        // NOT "bits[k] is the k-th bit"; it's an explicit packing convention, worth stating
        // outright since the indexing alone doesn't make that obvious.
        uint8_t dibit = (bits[k / 4] >> ((k % 4) * 2)) & 0x3;
        iq_sample_t sym = qpsk_modulate_symbol(dibit);
        freq_domain_scratch[k].re = sym.i;
        freq_domain_scratch[k].im = sym.q;
    }
    // IFFT: frequency bins -> time samples, including the 1/N scaling factor. Also
    // conjugates freq_domain_scratch in place (ifft()'s own note above); nothing here
    // reads it again afterward, so that's fine.
    ifft(freq_domain_scratch, time_domain_out, num_subcarriers);
}

// Typical caller: one static workspace, sized once at the compile-time ceiling, reused
// across every OFDM symbol rather than reallocated (let alone stack-allocated) per call.
static complex_t ofdm_tx_scratch[OFDM_MAX_SUBCARRIERS];

void example_ofdm_tx_call(uint8_t *bits, int num_subcarriers, complex_t *time_domain_out) {
    ofdm_modulate_symbol(bits, num_subcarriers, ofdm_tx_scratch, time_domain_out);
}

// Add cyclic prefix: copy the last cp_len samples to the front of the block.
void add_cyclic_prefix(complex_t *ofdm_symbol, int n, int cp_len, complex_t *out_with_cp) {
    for (int i = 0; i < cp_len; i++) {
        out_with_cp[i] = ofdm_symbol[n - cp_len + i];   // copy the tail to the front
    }
    for (int i = 0; i < n; i++) {
        out_with_cp[cp_len + i] = ofdm_symbol[i];
    }
}

// RX: discard the CP, then FFT the remainder.
void remove_cp_and_demod(complex_t *rx_with_cp, int n, int cp_len, complex_t *freq_domain_out) {
    fft(&rx_with_cp[cp_len], freq_domain_out, n);   // fft(): forward FFT, same butterfly core as Ch. 7
}

// CFO estimate from the cyclic prefix's self-similarity: correlate the CP against the tail
// of the same OFDM symbol it was copied from, n samples later. The phase of that correlation
// is directly proportional to the carrier frequency offset over one OFDM symbol period.
complex_t cfo_estimate_from_cp(const complex_t *rx_with_cp, int n, int cp_len) {
    complex_t corr = { 0.0f, 0.0f };
    for (int i = 0; i < cp_len; i++) {
        complex_t a = rx_with_cp[i];                  // the CP itself
        complex_t b = rx_with_cp[n + i];               // the tail sample it was copied from
        complex_t b_conj = { b.re, -b.im };
        complex_t prod = cmul(a, b_conj);              // cmul from Chapter 7
        corr.re += prod.re;
        corr.im += prod.im;
    }
    // atan2 of this value (Chapter 9) is the CFO phase over n samples, already signed as a
    // correction: atan2(corr.im, corr.re) / n is the phase_per_sample for cfo_correct().
    return corr;
}

// Correct CFO by rotating every sample in the block by the estimated per-sample phase
// increment, the same NCO-driven rotation as Chapter 9's quadrature mixer, just derived
// from this measurement instead of a fixed known frequency.
void cfo_correct(complex_t *block, int n, float phase_per_sample) {
    float phase = 0.0f;
    for (int i = 0; i < n; i++) {
        float c = cosf(phase), s = sinf(phase);
        float re = block[i].re * c - block[i].im * s;
        float im = block[i].re * s + block[i].im * c;
        block[i].re = re;
        block[i].im = im;
        phase += phase_per_sample;
    }
}

// One-tap equalizer per subcarrier: divide received symbol by estimated channel gain H_k.
complex_t equalize_subcarrier(complex_t received, complex_t channel_estimate_h) {
    float mag_sq = channel_estimate_h.re * channel_estimate_h.re
                 + channel_estimate_h.im * channel_estimate_h.im;
    complex_t h_conj = { channel_estimate_h.re, -channel_estimate_h.im };
    complex_t numerator = cmul(received, h_conj);     // cmul from Chapter 7
    complex_t result = { numerator.re / mag_sq, numerator.im / mag_sq };
    return result;
}

// Estimate channel gain at each pilot (received pilot symbol / known transmitted pilot value),
// then linearly interpolate between pilots to estimate the channel at data subcarriers.
// Subcarriers beyond the last pilot hold that pilot's estimate (nothing to interpolate to).
void estimate_channel_comb_pilots(const complex_t *received, const complex_t *known_pilots,
                                    complex_t *channel_estimate, int num_subcarriers) {
    // Step 1: direct measurement at each pilot subcarrier.
    for (int k = 0; k < num_subcarriers; k += PILOT_SPACING) {
        float mag_sq = known_pilots[k].re * known_pilots[k].re
                     + known_pilots[k].im * known_pilots[k].im;
        complex_t pilot_conj = { known_pilots[k].re, -known_pilots[k].im };
        complex_t h = cmul(received[k], pilot_conj);
        channel_estimate[k].re = h.re / mag_sq;
        channel_estimate[k].im = h.im / mag_sq;
    }
    // Step 2: linear interpolation for the data subcarriers between each pair of pilots.
    for (int k = 0; k < num_subcarriers - PILOT_SPACING; k += PILOT_SPACING) {
        for (int j = 1; j < PILOT_SPACING; j++) {
            float frac = (float)j / PILOT_SPACING;
            channel_estimate[k + j].re = channel_estimate[k].re
                + frac * (channel_estimate[k + PILOT_SPACING].re - channel_estimate[k].re);
            channel_estimate[k + j].im = channel_estimate[k].im
                + frac * (channel_estimate[k + PILOT_SPACING].im - channel_estimate[k].im);
        }
    }
    // Step 3: no pilot lies beyond the last one, so interpolation cannot reach the
    // subcarriers after it; hold the last pilot's estimate flat across that tail.
    int last_pilot = ((num_subcarriers - 1) / PILOT_SPACING) * PILOT_SPACING;
    for (int k = last_pilot + 1; k < num_subcarriers; k++) {
        channel_estimate[k] = channel_estimate[last_pilot];
    }
}

// Hard-clip an OFDM time-domain symbol's magnitude to a fixed ceiling, the simplest PAPR
// reduction technique: cheap to compute, but trades peak reduction for both in-band distortion
// (EVM degradation on the clipped samples themselves) and out-of-band spectral regrowth
// (Chapter 22's spectral mask concern), neither of which this function alone accounts for.
void ofdm_clip_papr(complex_t *time_domain, int n, float clip_ceiling) {
    for (int i = 0; i < n; i++) {
        float mag = sqrtf(time_domain[i].re * time_domain[i].re
                         + time_domain[i].im * time_domain[i].im);
        if (mag > clip_ceiling) {
            float scale = clip_ceiling / mag;
            time_domain[i].re *= scale;
            time_domain[i].im *= scale;   // preserve phase, only reduce magnitude
        }
    }
}

// Raised-cosine windowing applied to the edges of an OFDM symbol (post-IFFT, post-CP),
// tapering the first and last window_len samples smoothly instead of leaving hard edges.
// The tapered region typically overlaps into the cyclic prefix of the adjacent symbol
// rather than adding extra samples, so throughput isn't reduced to apply this.
void ofdm_apply_window(complex_t *symbol_with_cp, int total_len, int window_len) {
    for (int i = 0; i < window_len; i++) {
        float taper = 0.5f * (1.0f - cosf(3.14159265f * i / window_len));  // 0 -> 1 raised-cosine ramp
        symbol_with_cp[i].re *= taper;
        symbol_with_cp[i].im *= taper;
        int tail = total_len - 1 - i;
        symbol_with_cp[tail].re *= taper;
        symbol_with_cp[tail].im *= taper;
    }
}
