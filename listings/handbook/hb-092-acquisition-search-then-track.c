/*
 * Embedded DSP Handbook, Chapter 11: Spread Spectrum: DSSS and FHSS / Acquisition: search, then track
 * Listing 93 of 119 (index 92, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_dsss.c include/hb_dsss.h | Checked by: tests/test_D_dsss.c: early-late error vs closed form from the code autocorrelation for delays -2..+2 samples, sign convention; N-D2 (array convention ambiguity)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Early-late DLL: the DSSS equivalent of Gardner's timing error detector, comparing
// correlation strength half a chip early and half a chip late against the current alignment.
// A positive error means the local code needs to shift earlier; negative means later.
int32_t dll_chip_error(const int8_t *early_corr_chips, const int8_t *late_corr_chips,
                         const int8_t *pn_sequence, int len) {
    int32_t early_energy = 0, late_energy = 0;
    for (int i = 0; i < len; i++) {
        early_energy += early_corr_chips[i] * pn_sequence[i];
        late_energy  += late_corr_chips[i]  * pn_sequence[i];
    }
    return early_energy - late_energy;  // feeds the same style of loop filter as Chapter 9
}
