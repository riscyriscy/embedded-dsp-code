/* Source: Embedded DSP Handbook, Chapter 7 (Link budget: will the signal actually arrive with enough SNR).
 * link_budget_margin_db(): integer-dB margin check.
 * MIT License. */
#ifndef HB_LINK_BUDGET_H
#define HB_LINK_BUDGET_H


/* ---- function prototypes / shared state ---- */
int link_budget_margin_db(int tx_power_dbm, int tx_gain_dbi, int fspl_db, int rx_gain_dbi, int cable_loss_db, int required_snr_db, int noise_floor_dbm);

#endif /* HB_LINK_BUDGET_H */
