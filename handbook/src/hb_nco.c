/* Source: Embedded DSP Handbook, Chapter 9 (section 2, the quadrature mixer).
 * NCO, quadrature mixer, Q30->Q15 rescale. Tables are filled at runtime by nco_table_init().
 * Host build: the tables are file-static, so nco_table_init() lives in the header as static inline;
 * M_PI needs _DEFAULT_SOURCE/_GNU_SOURCE with -std=c11.
 * The equations behind the code:
 *   s(t) = I(t)*cos(2*pi*fc*t) - Q(t)*sin(2*pi*fc*t)
 *   I(t) = LPF[ x(t) * cos(2*pi*fc*t) ]
 *   Q(t) = LPF[ x(t) * -sin(2*pi*fc*t) ] */
#include "hb_nco.h"

/* Alternative to nco_table_init(): the same tables generated offline and shipped as constants.
 * Not built here, because it would redefine the cos_table/sin_table of hb_nco.h. */
#if 0
// cos_table/sin_table: a 256-entry Q15 lookup spanning exactly one full cycle (0 to 2*pi),
// generated once, offline (the same coefficient-export workflow as Chapter 5's FIR coeffs),
// not recomputed at runtime. 256 entries matches quadrature_mix's 8-bit index (phase_acc's
// top 8 bits); a finer table (more entries) trades flash/RAM for less phase-quantization
// error, exactly the same table-size-vs-precision tradeoff as any other LUT in this book.
// Same rule as nco_table_init() above (round(32768 * x), clamped to the Q15 maximum 32767),
// so both versions hold identical values.
static const int16_t cos_table[256] = { /* round(32768 * cos(2*pi*i/256)), max 32767 */ };
static const int16_t sin_table[256] = { /* round(32768 * sin(2*pi*i/256)), max 32767 */ };
#endif

// phase_inc is what actually sets the NCO's output frequency: phase_acc is a uint32_t
// representing one full circle as the entire 0..2^32-1 range (Chapter 11's Costas loop relies
// on this exact convention), so advancing it by phase_inc every sample completes a full
// cycle every 2^32 / phase_inc samples. Solving for the phase_inc that gives a desired
// carrier frequency fc at sample rate fs:
uint32_t nco_phase_inc_for_freq(float fc, float fs) {
    return (uint32_t)((fc / fs) * 4294967296.0);   // 4294967296.0 = 2^32
}
