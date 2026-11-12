/* Reset values for the Chapter 10 loopback receiver, plus the Chapter 6 worked-example
 * Costas gains. The host test (tests/test_chain.c) compiles this file into its own
 * translation unit and drives the receiver from it. */
#include <stdint.h>
#include "fm_loopback.h"
#include "fm_loops.h"
#include "fm_fir.h"
#include "generated/rrc_coeffs.h"   /* rrc_coeffs_q15[] from tools/design_rrc.py */

// Kp = 0.05192, Ki = 0.001385 from the worked example above. Chapter 10's
// decision samples have a = 5600, so Kd = 11200, at 2 samples/symbol:
costas_loop_t costas = { .phase = 0, .freq = 0,
                         .kp = COSTAS_KP_Q16(0.05192, 11200.0),   // = 207671349
                         .ki = COSTAS_KI_Q16(0.001385, 11200.0, 2) };  // = 2769885

void loopback_reset(qpsk_loopback_rx_t *out, float carrier_hz) {
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
    fir_init_q15(&rx.rrc_i, rrc_coeffs_q15);
    fir_init_q15(&rx.rrc_q, rrc_coeffs_q15);
    *out = rx;
}
