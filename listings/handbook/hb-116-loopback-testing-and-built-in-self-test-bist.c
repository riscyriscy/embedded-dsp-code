/*
 * Embedded DSP Handbook, Chapter 19: Test and Validation Methodology / Loopback testing and built-in self-test (BIST)
 * Listing 117 of 119 (index 116, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_testkit.c include/hb_testkit.h | Checked by: tests/test_D_testkit.c: loopback on returns the TX sample bit-exactly without calling the ADC; off reads the (stub) ADC; adc_read_rx_sample is declared in the module
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Digital loopback: bypass the RF front end entirely and feed modulated I/Q samples
// straight into the demodulation chain. Verifies the DSP chain (Chapters 6, 8, 9)
// independent of any RF hardware fault.
typedef struct {
    bool loopback_enabled;
} radio_config_t;

int32_t radio_get_rx_sample(radio_config_t *cfg, int32_t tx_sample) {
    if (cfg->loopback_enabled) {
        return tx_sample;   // digital loopback: skip the ADC/RF path entirely
    }
    return adc_read_rx_sample();   // normal path
}

// RF loopback (with an external cable/attenuator between TX and RX connectors) additionally
// exercises the mixer, PA, LNA, and ADC, catching hardware faults digital loopback cannot.
