/* Link-budget margin check (all quantities in dB / dBm).
 * Source: Field Manual, Chapter 7. */
#include "fm_rf.h"

int link_budget_margin_db(int tx_power_dbm, int tx_gain_dbi, int fspl_db,
                           int rx_gain_dbi, int cable_loss_db, int required_snr_db,
                           int noise_floor_dbm) {
    int rx_power_dbm = tx_power_dbm + tx_gain_dbi - fspl_db + rx_gain_dbi - cable_loss_db;
    int achieved_snr_db = rx_power_dbm - noise_floor_dbm;
    return achieved_snr_db - required_snr_db;   // positive = margin, negative = link won't close
}
