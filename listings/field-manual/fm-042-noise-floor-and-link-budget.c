/*
 * Embedded DSP Field Manual, Chapter 7: RF Numbers You Must Type / Noise floor and link budget
 * Listing 43 of 57 (index 42, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_rf.h src/fm_rf.c | Checked by: tests/test_nco_qpsk.c (link margin); the surrounding formulas are prose
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
Received power (dBm) = Tx power + Tx antenna gain - path loss + Rx antenna gain - cable loss
FSPL (dB) = 20*log10(distance_km) + 20*log10(freq_MHz) + 32.44

int link_budget_margin_db(int tx_power_dbm, int tx_gain_dbi, int fspl_db,
                           int rx_gain_dbi, int cable_loss_db, int required_snr_db,
                           int noise_floor_dbm) {
    int rx_power_dbm = tx_power_dbm + tx_gain_dbi - fspl_db + rx_gain_dbi - cable_loss_db;
    int achieved_snr_db = rx_power_dbm - noise_floor_dbm;
    return achieved_snr_db - required_snr_db;   // positive = margin, negative = link won't close
}
