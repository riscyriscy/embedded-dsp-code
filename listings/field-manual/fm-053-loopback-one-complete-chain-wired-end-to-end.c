/*
 * Embedded DSP Field Manual, Chapter 10: Testing the Chain / Loopback: one complete chain, wired end to end
 * Listing 54 of 57 (index 53, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: (init block, mirrored in tests/test_chain.c) | Checked by: tests/test_chain.c loopback_init()
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
qpsk_loopback_rx_t rx = { 0 };   // zero every field first - history[], hist_i/q, ted state, and
                                   // the bool/uint32_t counters all start at a valid, silent zero.
rx.agc.gain_q15      = 32768;     // 1.0 - AGC ramps from unity, not from a guess
rx.agc.target_power  = 1 << 28;   // Q30: a quarter of full scale^2 (RMS 0.5)
rx.agc.attack_shift  = 4;         // faster: anti-clip
rx.agc.release_shift = 8;         // slower: anti-pumping
rx.nco.base_inc = rx.nco.phase_inc =
    nco_phase_inc_for_freq(carrier_hz, 19200.0f);  // carrier_hz: your chain's own IF/carrier;
                                                     // fs = 9600 baud x 2 sps = 19200 Hz here
// rx.rrc_i.coeffs / rx.rrc_q.coeffs: load with this chain's RRC taps (Chapter 5) -
// history[] and idx are already zero from the { 0 } above.
rx.costas.kp = COSTAS_KP_Q16(0.05192, 11200.0);   // = 207671349: Chapter 6's worked
rx.costas.ki = COSTAS_KI_Q16(0.001385, 11200.0, 2); // = 2769885  example, Kd = 2 * 5600
