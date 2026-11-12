/*
 * Embedded DSP Handbook, Chapter 18: Forward Error Correction (FEC) / Puncturing: getting other code rates from the same K=7 encoder
 * Listing 109 of 119 (index 108, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_fec.c include/hb_fec.h | Checked by: tests/test_D_fec.c: pattern == 802.11 3/4 (A0 B0 A1 B2), counts, de-puncture symmetry and canary no-overread, 1333-byte prose arithmetic, rate-3/4 end-to-end decode
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Rate-3/4 puncturing of a rate-1/2 encoder's output: a repeating 6-bit pattern that keeps
// 4 of every 6 encoded bits (3 input bits -> 4 transmitted bits = rate 3/4), a standard
// WiFi (802.11) puncture pattern for this rate. 1 = keep, 0 = puncture (drop).
static const uint8_t puncture_pattern_3_4[6] = { 1, 1, 1, 0, 0, 1 };

// Apply puncturing to a rate-1/2 encoder's bit stream, writing only the kept bits out.
int puncture_bits(const uint8_t *encoded_bits, int num_bits, uint8_t *punctured_out) {
    int out_idx = 0;
    for (int i = 0; i < num_bits; i++) {
        if (puncture_pattern_3_4[i % 6]) {
            punctured_out[out_idx++] = encoded_bits[i];
        }
    }
    return out_idx;  // number of bits actually transmitted
}

// At the receiver, before Viterbi decoding: re-insert erasures (a neutral, zero-confidence
// LLR) at every position the transmitter punctured, restoring the original rate-1/2 shape
// the Viterbi decoder above expects. The decoder still works correctly on an erasure, an
// LLR of exactly zero simply contributes no information to that position's branch metric.
int depuncture_llrs(const int32_t *received_llrs, int num_received, int32_t *depunctured_out) {
    int in_idx = 0, out_idx = 0;
    while (in_idx < num_received) {
        for (int i = 0; i < 6; i++) {
            if (puncture_pattern_3_4[i]) {
                // Guard against a final, partial puncture period: num_received isn't always
                // an exact multiple of the pattern's kept-bit count, and reading past the end
                // of received_llrs here is exactly the kind of one-off buffer overrun that's
                // easy to miss in testing but real in production once a packet length doesn't
                // land on a period boundary.
                depunctured_out[out_idx++] = (in_idx < num_received) ? received_llrs[in_idx++] : 0;
            } else {
                depunctured_out[out_idx++] = 0;
            }
        }
    }
    return out_idx;
}
