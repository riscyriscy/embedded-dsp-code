/* Source: Embedded DSP Handbook, Chapter 22 ("One complete loopback chain, wired end to end").
 * Complete digital-loopback QPSK receiver (AGC -> mixer -> RRC -> Gardner -> Costas -> slicer -> BER).
 * MIT License. */
#ifndef HB_LOOPBACK_H
#define HB_LOOPBACK_H

#include <stdint.h>
#include <stdbool.h>
#include "hb_agc.h"
#include "hb_nco.h"
#include "hb_fir_q15.h"
#include "hb_timing.h"
#include "hb_timing_loop.h"
#include "hb_costas.h"
#include "hb_testkit.h"
#include "hb_qpsk.h"

#define GARDNER_ONLY_SYMBOLS 40   // symbols of Gardner-only acquisition before the Costas loop comes
                                   // online - Chapter 12's own staged-acquisition guidance: let timing
                                   // recovery converge first, then layer the (narrower) carrier loop
                                   // on top, rather than adapting both loops from sample 1.

typedef struct {
    agc_t          agc;
    nco_t          nco;
    fir_q15_t      rrc_i, rrc_q;           // matched filter state, one per rail (Chapter 11, section 1) -
                                           // fir_q15_t, not Chapter 2's float fir_filter_t: everything in
                                           // this loopback runs fixed-point, mixer output included
    int32_t        hist_i[2], hist_q[2];  // most recent matched-filter outputs per rail:
                                           // [0] = mid-symbol sample, [1] = decision sample. At the
                                           // 2 samples/symbol this chain runs at, that pair IS the
                                           // mid/curr pair Gardner needs - no separate fractional
                                           // interpolator is required at exactly 2 sps.
    gardner_ted_t  ted_i, ted_q;          // one real-valued TED per rail (see the note below)
    timing_loop_t  timing_loop;
    costas_loop_t  costas;
    int32_t        mu;                    // current fractional timing estimate, Q-format
                                           // larger mu is later; this 2 sps
                                           // chain has no interpolator for it
    int            sample_parity;         // alternates 0/1 across each symbol's 2 samples
    uint32_t       symbols_processed;
    bool           costas_enabled;        // gated on GARDNER_ONLY_SYMBOLS in, see above
    ber_counter_t  ber;
} qpsk_loopback_rx_t;

/* ---- function prototypes / shared state ---- */
void qpsk_loopback_process_sample(qpsk_loopback_rx_t *rx, int32_t raw_sample, uint8_t tx_dibit);

/* Host build: hb_nco.h keeps its cos/sin tables file-static (one private copy per translation unit), so the
 * copy that quadrature_mix() reads inside hb_loopback.c is filled by this wrapper. */
void qpsk_loopback_init_tables(void);

#endif /* HB_LOOPBACK_H */
