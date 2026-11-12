/*
 * Embedded DSP Handbook, Chapter 10: OFDM and Multicarrier Modulation / The core mechanism: IFFT at TX, FFT at RX
 * Listing 84 of 119 (index 83, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_ofdm.c include/hb_ofdm.h (needs src/hb_qpsk.c, src/hb_fft.c) | Checked by: tests/test_D_ofdm.c: ifft vs naive IDFT, round trip N=2048, modulate vs IDFT of Gray QPSK, bit packing, TX->RX dibits
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Conceptual TX: map bits to per-subcarrier QPSK symbols, then IFFT to time domain.
// (Reuses complex_t and qpsk_modulate_symbol from Chapter 6.)

// A fixed-size, compile-time-bounded array, not a VLA: MISRA and several embedded
// toolchains (C89-only, some -std flags) reject `complex_t freq_domain[num_subcarriers]`
// when num_subcarriers is a runtime parameter rather than a constant. The bound also sets
// how big a caller-owned workspace below needs to be - see the not-on-the-stack note there.
#define OFDM_MAX_SUBCARRIERS 2048

// ifft(): the inverse transform, defined here in terms of the same forward fft() from
// Chapter 4 rather than a second, separately maintained butterfly implementation. The
// standard identity: conjugate the input, run the ordinary forward FFT unmodified,
// conjugate the result, and scale by 1/N. This reuses Chapter 4's fft() exactly as written.
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
    fft(freq_domain_in, time_domain_out, n);   // Chapter 4's forward FFT, called unmodified
    for (int i = 0; i < n; i++) {
        float re = time_domain_out[i].re;
        time_domain_out[i].re =  re / n;
        time_domain_out[i].im = -time_domain_out[i].im / n;
    }
}

// freq_domain_scratch is CALLER-supplied, not a local array: the same OFDM_MAX_SUBCARRIERS
// * 8 bytes = 16 KB that ifft() above used to hide in a local array, just moved to wherever
// the caller actually wants that storage duration to live (typically a static or BSS
// buffer declared once, the same pattern Chapter 1's Lab 0 already uses for its own
// persistent FFT buffers) instead of growing fresh on this function's stack frame every
// call. Not reentrant as written: two calls sharing one freq_domain_scratch buffer from
// two contexts (two RTOS tasks, an ISR and a task) will corrupt each other's in-flight
// symbol; give each concurrent caller its own scratch buffer if that's a real possibility,
// the same reentrancy tradeoff Chapter 1's ping-pong DMA state carries.
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
