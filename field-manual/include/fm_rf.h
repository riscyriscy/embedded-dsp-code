/* Link-budget margin check (all quantities in dB / dBm).
 * Source: Field Manual, Chapter 7. */
#ifndef FM_RF_H
#define FM_RF_H


/* ---- function prototypes / shared state ---- */
int link_budget_margin_db(int tx_power_dbm, int tx_gain_dbi, int fspl_db, int rx_gain_dbi, int cable_loss_db, int required_snr_db, int noise_floor_dbm);

#endif /* FM_RF_H */
