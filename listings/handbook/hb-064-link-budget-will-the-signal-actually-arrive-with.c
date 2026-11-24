/*
 * Embedded DSP Handbook, Chapter 10: RF Front-End and Link Budget Basics / Link budget: will the signal actually arrive with enough SNR
 * Listing 65 of 119 (index 64, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: tests/shims/hb_C_snippet_64.inc | Checked by: tests/test_C_ch7_link.c (the call run verbatim, returns 33)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
link_budget_margin_db(/*tx_power_dbm=*/14, /*tx_gain_dbi=*/0, /*fspl_db=*/92,
                       /*rx_gain_dbi=*/0, /*cable_loss_db=*/0, /*required_snr_db=*/10,
                       /*noise_floor_dbm=*/-121);
// rx_power_dbm    = 14 + 0 - 92 + 0 - 0 = -78
// achieved_snr_db = -78 - (-121) = 43
// returns 43 - 10 = +33 dB of margin
