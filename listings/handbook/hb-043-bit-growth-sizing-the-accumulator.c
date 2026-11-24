/*
 * Embedded DSP Handbook, Chapter 8: Multirate DSP: Decimation, Interpolation, and CIC Filters / Bit growth: sizing the accumulator
 * Listing 44 of 119 (index 43, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_cic.c include/hb_cic.h | Checked by: tests/test_B_cic.c (includes the .c for static cic_growth_bits): shift helper, over-removal bound, scaled output, DC levels -32768..32767
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Right-shift amount that undoes a CIC's inherent M^N passband gain (N=3 here, matching the
// integ[3]/comb[3] arrays above), bringing a decimated output back to roughly the same numeric
// scale as the input. Exact only when M is itself a power of 2 (log2(M) lands on an integer);
// for any other M this rounds UP to the next power of 2, so the shift removes MORE than the
// true M^N gain - by close to a factor of 2 per stage in the worst case (M just above a
// power-of-2 boundary), a real, fixed, predictable attenuation worth sizing for (or avoiding
// outright by choosing a power-of-2 M), not a bug to go chase down later.
static int cic_growth_bits(int N, int M) {
    int bits = 0;
    while ((1 << bits) < M) bits++;
    return N * bits;
}

// Same as cic_decimate_stage above, with the missing rescale step added: after the comb
// stages produce a decimated sample, round and shift it back down by growth_bits before
// handing it to the caller, instead of returning the still-M^N-scaled raw value.
bool cic_decimate_stage_scaled(cic_integrator_t integ[3], cic_comb_t comb[3], int32_t input,
                                 int decim_counter, int M, int32_t *output) {
    int64_t raw;
    if (!cic_decimate_stage(integ, comb, input, decim_counter, M, &raw)) return false;

    int gb = cic_growth_bits(3, M);
    int64_t rounded = (gb > 0) ? (raw + ((int64_t)1 << (gb - 1))) : raw;
    *output = (int32_t)(rounded >> gb);
    return true;
}

// The decimator-state wrapper for the scaled path: same counter handling as
// cic_decimator_process, but the output is back on the input's numeric scale.
bool cic_decimator_process_scaled(cic_decimator_t *d, int32_t input, int M, int32_t *output) {
    d->decim_counter = (d->decim_counter + 1) % M;
    return cic_decimate_stage_scaled(d->integ, d->comb, input, d->decim_counter, M, output);
}
