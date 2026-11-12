/*
 * Embedded DSP Field Manual, Chapter 6: Loop Tuning and Bring-Up / Loop design: Bn*T, zeta, Kp, Ki
 * Listing 39 of 57 (index 38, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_loops.h src/fm_loops.c | Checked by: tests/test_nco_qpsk.c
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// f_c: nominal carrier (Hz) - THIS chain's own carrier/IF, not the RF number an antenna sees.
// ppm: total tolerance as a plain number (e.g. 40.0f for two 20 ppm crystals, not 40e-6f - the
// 1e-6 conversion happens inside this function). f_s: sample rate (Hz), the same one f_c is defined at.
// A coarse, one-time correction applied before Costas is enabled - not a replacement for it.
uint32_t ppm_to_phase_inc_offset(float f_c, float ppm, float f_s) {
    float freq_offset_hz = f_c * ppm * 1e-6f;
    return nco_phase_inc_for_freq(freq_offset_hz, f_s);
}

// Applied once at bring-up, against this chain's own carrier/IF (Chapter 10) - not the 915 MHz RF
// figure above, which would alias at this NCO's 19.2 kHz sample rate if passed in directly:
nco.phase_inc = nco.base_inc + ppm_to_phase_inc_offset(carrier_hz, 40.0f, 19200.0f);
