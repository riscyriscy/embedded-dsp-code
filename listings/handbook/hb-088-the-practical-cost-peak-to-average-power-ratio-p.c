/*
 * Embedded DSP Handbook, Chapter 10: OFDM and Multicarrier Modulation / The practical cost: peak-to-average power ratio (PAPR)
 * Listing 89 of 119 (index 88, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_ofdm.c include/hb_ofdm.h | Checked by: tests/test_D_ofdm.c: all-in-phase PAPR = N (18.06 dB, prose 18.1), clipping bound/phase/untouched samples, PAPR 7.20->3.92 dB, in-band EVM 10.7%
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Hard-clip an OFDM time-domain symbol's magnitude to a fixed ceiling, the simplest PAPR
// reduction technique: cheap to compute, but trades peak reduction for both in-band distortion
// (EVM degradation on the clipped samples themselves) and out-of-band spectral regrowth
// (Chapter 19's spectral mask concern), neither of which this function alone accounts for.
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
