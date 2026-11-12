/* Complete digital-loopback QPSK receiver.
 * Source: Field Manual, Chapter 10. */
#ifndef FM_LOOPBACK_H
#define FM_LOOPBACK_H

#include "fm_qformat.h"
#include "fm_agc.h"
#include "fm_nco.h"
#include "fm_fir.h"
#include "fm_timing.h"
#include "fm_costas.h"
#include "fm_ber.h"
#include "fm_qpsk.h"
#include <stdio.h>

#define GARDNER_ONLY_SYMBOLS 40   // let timing converge before the (narrower) carrier loop

                                   // comes online - adapting both loops from sample 1 fights itself

typedef struct {
    agc_t          agc;
    nco_t          nco;
    fir_q15_t      rrc_i, rrc_q;          // matched filter state, one per rail (Chapter 5)
    int32_t        hist_i[2], hist_q[2];  // [0] mid-symbol, [1] decision sample - at 2 sps this
                                           // pair IS what Gardner needs, no separate interpolator
    gardner_ted_t  ted_i, ted_q;
    timing_loop_t  timing_loop;
    costas_loop_t  costas;
    int32_t        mu;                    // diagnostic only: running Gardner timing-loop output.
                                           // This loopback is sample-synchronous (2 sps, no
                                           // fractional interpolator), so mu is NOT fed back into a
                                           // resampler here, and the reset block below leaves the
                                           // timing loop's kp/ki at 0, so mu stays 0 until you set
                                           // them. It is a hook to log, not a sub-sample position.
    int            sample_parity;
    uint32_t       symbols_processed;
    bool           costas_enabled;
    ber_counter_t  ber;
} qpsk_loopback_rx_t;

/* ---- function prototypes / shared state ---- */
void qpsk_loopback_process_sample(qpsk_loopback_rx_t *rx, int32_t raw_sample, uint8_t tx_dibit);
void dump_loop_state_uart(const qpsk_loopback_rx_t *rx);

#endif /* FM_LOOPBACK_H */
