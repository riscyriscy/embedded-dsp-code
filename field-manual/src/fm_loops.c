/* Loop-gain quantization macro and ppm->NCO offset helper.
 * Source: Field Manual, Chapter 6. */
#include "fm_loops.h"

// f_c: nominal carrier (Hz) - THIS chain's own carrier/IF, not the RF number an antenna sees.
// ppm: total tolerance as a plain number (e.g. 40.0f for two 20 ppm crystals, not 40e-6f - the
// 1e-6 conversion happens inside this function). f_s: sample rate (Hz), the same one f_c is defined at.
// A coarse, one-time correction applied before Costas is enabled - not a replacement for it.
uint32_t ppm_to_phase_inc_offset(float f_c, float ppm, float f_s) {
    float freq_offset_hz = f_c * ppm * 1e-6f;
    return nco_phase_inc_for_freq(freq_offset_hz, f_s);
}
