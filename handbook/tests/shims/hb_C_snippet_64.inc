/* TEST-ONLY. HB[64]: the worked call of link_budget_margin_db(), verbatim, #included inside a test function. */
link_budget_margin_db(/*tx_power_dbm=*/14, /*tx_gain_dbi=*/0, /*fspl_db=*/92,
                       /*rx_gain_dbi=*/0, /*cable_loss_db=*/0, /*required_snr_db=*/10,
                       /*noise_floor_dbm=*/-121);
// rx_power_dbm    = 14 + 0 - 92 + 0 - 0 = -78
// achieved_snr_db = -78 - (-121) = 43
// returns 43 - 10 = +33 dB of margin
