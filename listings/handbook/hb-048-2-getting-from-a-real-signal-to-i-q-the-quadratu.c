/*
 * Embedded DSP Handbook, Chapter 9: Modulation / Demodulation and I/Q Signals / 2. Getting from a real signal to I/Q: the quadrature mixer
 * Listing 49 of 119 (index 48, counting from 0 in book order), as printed in the 1.0 edition of the book.
 * Status: tested | Built as: include/hb_nco.h src/hb_nco.c | Checked by: nco_phase_inc_for_freq tested vs double (tests/test_B_nco.c); the empty const-table stub (redefines cos_table/sin_table) is not built; its stated rounding rule matches HB[47]
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 * In the book (November 2026), nco_phase_inc_for_freq is replaced by a comment that points to Field Manual listing fm-025. The text below is the full listing, kept so the compiled module still matches it.
 */
// cos_table/sin_table: a 256-entry Q15 lookup spanning exactly one full cycle (0 to 2*pi),
// generated once, offline (the same coefficient-export workflow as Chapter 5's FIR coeffs),
// not recomputed at runtime. 256 entries matches quadrature_mix's 8-bit index (phase_acc's
// top 8 bits); a finer table (more entries) trades flash/RAM for less phase-quantization
// error, exactly the same table-size-vs-precision tradeoff as any other LUT in this book.
// Same rule as nco_table_init() above (round(32768 * x), clamped to the Q15 maximum 32767),
// so both versions hold identical values.
static const int16_t cos_table[256] = { /* round(32768 * cos(2*pi*i/256)), max 32767 */ };
static const int16_t sin_table[256] = { /* round(32768 * sin(2*pi*i/256)), max 32767 */ };

// phase_inc is what actually sets the NCO's output frequency: phase_acc is a uint32_t
// representing one full circle as the entire 0..2^32-1 range (Chapter 11's Costas loop relies
// on this exact convention), so advancing it by phase_inc every sample completes a full
// cycle every 2^32 / phase_inc samples. Solving for the phase_inc that gives a desired
// carrier frequency fc at sample rate fs:
uint32_t nco_phase_inc_for_freq(float fc, float fs) {
    return (uint32_t)((fc / fs) * 4294967296.0);   // 4294967296.0 = 2^32
}
