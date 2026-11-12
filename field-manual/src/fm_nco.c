/* NCO, quadrature mixer, Q30->Q15 rescale.
 * Source: Field Manual, Chapter 5.
 * The sine/cosine tables come from the generated header (tools/design_nco_table.py). */
#include "fm_nco.h"

// phase_inc for a desired carrier frequency fc at sample rate fs:
uint32_t nco_phase_inc_for_freq(float fc, float fs) {
    return (uint32_t)((fc / fs) * 4294967296.0);   // 2^32
}
