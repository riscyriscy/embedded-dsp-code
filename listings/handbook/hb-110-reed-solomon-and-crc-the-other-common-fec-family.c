/*
 * Embedded DSP Handbook, Chapter 21: Forward Error Correction (FEC) / Reed-Solomon and CRC: the other common FEC family
 * Listing 111 of 119 (index 110, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_fec.c include/hb_fec.h | Checked by: tests/test_D_fec.c: gf_mul vs carry-less multiply mod 0x11d for all 65536 pairs, primitive element, field axioms, table sizes
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// GF(2^8) multiply via precomputed log/antilog tables - the primitive every RS encode/decode
// step above reduces to. gf_log[]/gf_exp[] are computed once, below, from the field's
// primitive polynomial (0x11d, x^8+x^4+x^3+x^2+1, the choice QR codes' Reed-Solomon
// uses), a fixed lookup table, not something recomputed per call. gf_exp is stored
// twice over (512 entries) because gf_mul() indexes it with a sum of two logs, at
// most 254 + 254 = 508.
void gf_tables_init(uint8_t gf_log[256], uint8_t gf_exp[512]) {
    unsigned x = 1;
    for (int i = 0; i < 255; i++) {
        gf_exp[i] = (uint8_t)x;
        gf_log[x] = (uint8_t)i;
        x <<= 1;                       // multiply by alpha = 2
        if (x & 0x100) x ^= 0x11d;     // reduce modulo the field polynomial
    }
    for (int i = 255; i < 512; i++) gf_exp[i] = gf_exp[i - 255];
    gf_log[0] = 0;   // log(0) is undefined; gf_mul() tests for a zero operand
}

// Multiply two field elements; zero has no logarithm, so it is handled first.
uint8_t gf_mul(uint8_t a, uint8_t b, const uint8_t *gf_log, const uint8_t *gf_exp) {
    if (a == 0 || b == 0) return 0;
    return gf_exp[gf_log[a] + gf_log[b]];   // multiply = add the logs, then look up the antilog
}
