/*
 * Embedded DSP Handbook, Chapter 10: RF Front-End and Link Budget Basics / Link budget: will the signal actually arrive with enough SNR
 * Listing 64 of 119 (index 63, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_link_budget.h src/hb_link_budget.c | Checked by: tests/test_C_ch7_link.c (hand-computed cases, sign of every argument)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Simple link-budget margin check in C - values in dB/dBm as computed above.
int link_budget_margin_db(int tx_power_dbm, int tx_gain_dbi, int fspl_db,
                           int rx_gain_dbi, int cable_loss_db, int required_snr_db,
                           int noise_floor_dbm) {
    int rx_power_dbm = tx_power_dbm + tx_gain_dbi - fspl_db + rx_gain_dbi - cable_loss_db;
    int achieved_snr_db = rx_power_dbm - noise_floor_dbm;
    return achieved_snr_db - required_snr_db;  // positive = margin, negative = link won't close
}
