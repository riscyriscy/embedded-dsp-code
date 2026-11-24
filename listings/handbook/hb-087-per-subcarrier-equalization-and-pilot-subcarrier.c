/*
 * Embedded DSP Handbook, Chapter 13: OFDM and Multicarrier Modulation / Per-subcarrier equalization and pilot subcarriers
 * Listing 88 of 119 (index 87, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_ofdm.c include/hb_ofdm.h | Checked by: tests/test_D_ofdm.c: exact for linear channel, == linear interpolation of true pilot gains, tail subcarriers covered
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#define PILOT_SPACING 8   // every 8th subcarrier is a pilot, the rest carry data

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
