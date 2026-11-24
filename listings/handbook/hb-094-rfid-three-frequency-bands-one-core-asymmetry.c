/*
 * Embedded DSP Handbook, Chapter 15: On-Off Keying (OOK), RFID, and NFC / RFID: three frequency bands, one core asymmetry
 * Listing 95 of 119 (index 94, counting from 0 in book order), exactly as it appears in the book.
 * Status: syntax-only | Built as: reference/hbD_094_tag_backscatter_bit.c.txt tests/shims/hbD_hb094_tag_backscatter.inc | Checked by: tests/test_D_snippets.c: compiled verbatim against a recording HAL stub; only control flow (bit->load state) and the 13.56 MHz/16 = 847.5 kHz arithmetic checked
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Conceptual illustration of backscatter load modulation at the tag (not something you'd
// implement on a general MCU - real passive tags use dedicated analog frontend silicon -
// but useful for understanding what that silicon is doing.)
void tag_backscatter_bit(uint8_t bit) {
    if (bit) {
        set_antenna_load(LOAD_STATE_HIGH);   // shifts reflected amplitude/phase one way
    } else {
        set_antenna_load(LOAD_STATE_LOW);    // shifts it the other way
    }
    // The reader observes this as a small amplitude ripple on its own transmitted carrier,
    // demodulated with the same envelope-detection approach shown above (often via a
    // synchronous subcarrier at a fixed offset frequency, e.g. 847 kHz for ISO 14443 Type A,
    // to separate the weak backscatter signal from the much stronger transmitted carrier).
}
