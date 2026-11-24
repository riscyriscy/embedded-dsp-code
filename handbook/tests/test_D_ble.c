/* Chapter 16 (BLE), HB[95] GFSK bit decision, HB[96] CRC-16-CCITT and whitening (src/hb_ble.c + src/hb_fm_demod.c).
 * References:
 *   - CRC: a bit-by-bit polynomial division written from the definition; the catalogue check value for the nine ASCII
 *     digits "123456789" (CRC-16/CCITT-FALSE = 0x29B1 for init 0xFFFF, CRC-16/XMODEM = 0x31C3 for init 0x0000); and values
 *     for other messages from Python's binascii.crc_hqx (independent implementation of the same CRC).
 *   - Whitening: the Bluetooth Core Specification LFSR (x^7 + x^4 + 1, position 0 = 1, positions 1..6 = channel index),
 *     written twice in two different forms (explicit register positions; compact right-shift/XOR-0x44), plus the
 *     linear-recurrence signature of each polynomial.
 *   - GFSK: a synthesized constant-envelope FSK / Gaussian-filtered FSK waveform whose bits we know. */
#include <stdint.h>
#include <string.h>
#include "check.h"
#include "hb_ble.h"

/* ---- references ---- */
static uint16_t crc_bitwise(const uint8_t *d, int n, uint16_t crc) {      /* MSB-first, poly x^16 + x^12 + x^5 + 1 */
    for (int i = 0; i < n; i++) for (int b = 7; b >= 0; b--) {
        int inbit = (d[i] >> b) & 1, top = (crc >> 15) & 1;
        crc = (uint16_t)(crc << 1);
        if (inbit ^ top) crc ^= 0x1021;
    }
    return crc;
}
static void ble_spec_stream_positions(int channel, int *bits, int nbits) {   /* register positions 0..6 as in the spec's figure */
    int pos[7], nw[7]; pos[0] = 1; for (int i = 0; i < 6; i++) pos[1 + i] = (channel >> (5 - i)) & 1;
    for (int t = 0; t < nbits; t++) {
        int o = pos[6]; bits[t] = o;
        nw[0] = o; nw[1] = pos[0]; nw[2] = pos[1]; nw[3] = pos[2]; nw[4] = pos[3] ^ o; nw[5] = pos[4]; nw[6] = pos[5];
        memcpy(pos, nw, sizeof pos);
    }
}
static void ble_spec_stream_compact(int channel, int *bits, int nbits) {
    unsigned l = 0x40u | (unsigned)(channel & 0x3F);
    for (int t = 0; t < nbits; t++) { int o = l & 1; bits[t] = o; l >>= 1; if (o) l ^= 0x44; }
}
static void whiten_stream(int channel, int *bits, int nbits) {              /* keystream of whiten_payload(), via whitening zeros */
    uint8_t z[64]; memset(z, 0, sizeof z); whiten_payload(z, nbits / 8, (uint8_t)channel);
    for (int t = 0; t < nbits; t++) bits[t] = (z[t / 8] >> (t % 8)) & 1;
}

int main(void) {
    /* ===================== CRC-16-CCITT ===================== */
    {
        const uint8_t digits[9] = { '1','2','3','4','5','6','7','8','9' };
        CHECK(crc16_ccitt(digits, 9, 0xFFFF) == 0x29B1, "CRC-16/CCITT-FALSE check value of \"123456789\": got %04X, want 29B1", crc16_ccitt(digits, 9, 0xFFFF));
        CHECK(crc16_ccitt(digits, 9, 0x0000) == 0x31C3, "CRC-16/XMODEM check value of \"123456789\": got %04X, want 31C3", crc16_ccitt(digits, 9, 0x0000));
        static const struct { int n; const char *s; uint16_t ccitt_false, xmodem; } v[] = {
            { 0, "", 0xFFFF, 0x0000 }, { 1, "\x41", 0xB915, 0x58E5 },
            { 32, "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f", 0x23B3, 0xD2FF },
            { 10, "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00", 0xE139, 0x0000 },
            { 10, "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", 0xA6E1, 0x47D8 },
            { 23, "\xa5\x4d\xca\x18\x25\x30\xbb\x1d\x6d\x13\x2c\xde\xd6\x23\x7b\x2e\xd9\x1e\x3f\x72\x1f\xcb\x19", 0xB3A4, 0x75F2 },
            { 64, "\x71\x17\x44\x94\xd6\x49\x3c\x9d\x5c\x34\x60\xbe\x31\x20\x1e\x69\xfe\xda\xa0\xee\xe8\xb9\x99\x7f\x5c\x7c\x29\x99\xfd\xaf\xe5\x93\x25\x3c\xd6\x54\xaf\x4d\xfa\xd7\x14\x27\xa0\xae\xb3\xfe\xe9\x23\x2f\x8a\xf2\x21\x1f\x9e\xe4\x91\xc5\xb1\x0b\xec\xb5\x56\x3b\xfc", 0xB012, 0x66C8 },
        };
        for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
            CHECK(crc16_ccitt((const uint8_t *)v[i].s, v[i].n, 0xFFFF) == v[i].ccitt_false, "vector %u init FFFF: got %04X want %04X", i, crc16_ccitt((const uint8_t *)v[i].s, v[i].n, 0xFFFF), v[i].ccitt_false);
            CHECK(crc16_ccitt((const uint8_t *)v[i].s, v[i].n, 0x0000) == v[i].xmodem, "vector %u init 0000: got %04X want %04X", i, crc16_ccitt((const uint8_t *)v[i].s, v[i].n, 0), v[i].xmodem);
        }
        /* bit-by-bit definition on random data, all init values we use */
        uint8_t buf[200];
        int bad = 0;
        for (int t = 0; t < 300; t++) {
            int n = (int)(lcg_next() % 200); for (int i = 0; i < n; i++) buf[i] = (uint8_t)(lcg_next() >> 4);
            uint16_t init = (t & 1) ? 0xFFFF : (uint16_t)lcg_next();
            if (crc16_ccitt(buf, n, init) != crc_bitwise(buf, n, init)) bad++;
        }
        CHECK(bad == 0, "crc16_ccitt differs from the bit-by-bit division in %d of 300 random messages", bad);
        /* chaining: the third argument lets a message be fed in pieces */
        for (int i = 0; i < 40; i++) buf[i] = (uint8_t)(lcg_next() >> 3);
        CHECK(crc16_ccitt(buf + 17, 23, crc16_ccitt(buf, 17, 0xFFFF)) == crc16_ccitt(buf, 40, 0xFFFF), "chained CRC == one-shot CRC");
        /* TX appends the CRC (big-endian); the receiver's CRC over data+CRC is zero, and any single-bit error is caught */
        uint16_t c = crc16_ccitt(buf, 40, 0xFFFF); buf[40] = (uint8_t)(c >> 8); buf[41] = (uint8_t)c;
        CHECK(crc16_ccitt(buf, 42, 0xFFFF) == 0, "receive side: CRC over payload || CRC is 0");
        int undetected = 0;
        for (int bit = 0; bit < 42 * 8; bit++) { buf[bit / 8] ^= (uint8_t)(1u << (bit % 8)); if (crc16_ccitt(buf, 42, 0xFFFF) == 0) undetected++; buf[bit / 8] ^= (uint8_t)(1u << (bit % 8)); }
        CHECK(undetected == 0, "every single-bit error in a 42-byte packet is detected (%d missed)", undetected);
    }

    /* ===================== whitening vs the Bluetooth specification ===================== */
    {
        /* the two independent spec formulations agree for all 40 channels */
        int bad = 0;
        for (int ch = 0; ch < 40; ch++) { int a[254], b[254]; ble_spec_stream_positions(ch, a, 254); ble_spec_stream_compact(ch, b, 254); if (memcmp(a, b, sizeof a)) bad++; }
        CHECK(bad == 0, "the two independent spec formulations (register positions / compact XOR 0x44) agree on all 40 channels (%d differ)", bad);
        /* the spec stream follows o[t+7] = o[t]^o[t+4] (x^7 + x^4 + 1) */
        { int a[300]; ble_spec_stream_positions(37, a, 300); int v = 0; for (int t = 0; t + 7 < 300; t++) if (a[t + 7] != (a[t] ^ a[t + 4])) v++; CHECK(v == 0, "spec stream satisfies o[t+7] = o[t]^o[t+4] (x^7 + x^4 + 1)"); }
        /* whiten_payload() produces exactly the spec keystream, data bits LSB first, on every channel */
        bad = 0;
        for (int ch = 0; ch < 40; ch++) {
            uint8_t d[32], o[32]; int s[256];
            for (int i = 0; i < 32; i++) o[i] = d[i] = (uint8_t)(lcg_next() >> 1);
            ble_spec_stream_positions(ch, s, 256);
            whiten_payload(d, 32, (uint8_t)ch);
            for (int i = 0; i < 32; i++) { int ks = 0; for (int b = 0; b < 8; b++) ks |= s[i * 8 + b] << b; if (d[i] != (uint8_t)(o[i] ^ ks)) bad++; }
            whiten_payload(d, 32, (uint8_t)ch);                              /* applying it again de-whitens */
            if (memcmp(d, o, 32)) bad++;
        }
        CHECK(bad == 0, "whiten_payload == spec keystream XOR data (LSB first) and self-inverse on channels 0..39 (%d mismatches)", bad);
        /* golden bytes of the spec keystream for channel 37 (advertising channel), from the register-position formulation */
        { uint8_t z[4] = { 0 }; whiten_payload(z, 4, 37);
          CHECK(z[0] == 0x8D && z[1] == 0xD2 && z[2] == 0x57 && z[3] == 0xA1, "channel 37 keystream starts 8D D2 57 A1 (got %02X %02X %02X %02X)", z[0], z[1], z[2], z[3]); }
        /* maximal-length register: the keystream repeats with period 127 on every channel */
        int bad_period = 0;
        for (int ch = 0; ch < 40; ch++) {
            int ks[400]; whiten_stream(ch, ks, 384);
            int period = 0; for (int p = 1; p < 200 && !period; p++) { int same = 1; for (int t = 0; t + p < 384; t++) if (ks[t] != ks[t + p]) same = 0; if (same) period = p; }
            if (period != 127) bad_period++;
        }
        CHECK(bad_period == 0, "keystream period is 127 on all 40 channels (%d channels differ)", bad_period);
        /* every channel starts a different keystream */
        int dup = 0;
        for (int a = 0; a < 40; a++) for (int b = a + 1; b < 40; b++) { int x[64], y[64]; whiten_stream(a, x, 64); whiten_stream(b, y, 64); if (!memcmp(x, y, sizeof x)) dup++; }
        CHECK(dup == 0, "the 40 channels give 40 distinct keystreams (%d duplicate pairs)", dup);
        /* only the low 6 bits of the channel argument are used (positions 1..6 hold a 6-bit index) */
        { uint8_t a[16] = { 0 }, b[16] = { 0 }; whiten_payload(a, 16, 37); whiten_payload(b, 16, 37 | 0xC0); CHECK(!memcmp(a, b, 16), "bits above the 6-bit channel index are ignored"); }
    }

    /* ===================== GFSK bit decision (HB[95]) ===================== */
    {
        enum { SPS = 8, NB = 200 };
        int bits[NB]; for (int i = 0; i < NB; i++) bits[i] = (lcg_next() >> 6) & 1;
        /* (1) ideal 2-FSK, modulation index 0.5: phase step +-pi*h/SPS per sample, positive step = bit 1 */
        for (int amp_i = 0; amp_i < 2; amp_i++) {
            double A = amp_i ? 30000.0 : 8000.0; double ph = 0; fm_demod_t d = { 0, 0 };
            int wrong = 0;
            for (int b = 0; b < NB; b++) for (int s = 0; s < SPS; s++) {
                ph += (bits[b] ? 1.0 : -1.0) * 3.14159265358979323846 * 0.5 / SPS;
                int32_t i = (int32_t)lround(A * cos(ph)), q = (int32_t)lround(A * sin(ph));
                uint8_t dec = gfsk_decide_bit(&d, i, q);
                if (b > 0 && s == SPS / 2 && dec != bits[b]) wrong++;
            }
            CHECK(wrong == 0, "ideal FSK (h=0.5), amplitude %.0f: %d of %d mid-bit decisions wrong", A, wrong, NB - 1);
        }
        /* (2) Gaussian-filtered (BT = 0.5) NRZ -> frequency -> phase, as BLE does; sample at bit centres */
        {
            double g[4 * SPS + 1]; double sigma = sqrt(log(2.0)) / (2.0 * 3.14159265358979323846 * 0.5) * SPS, sum = 0;   /* BT = 0.5, in samples */
            for (int k = 0; k <= 4 * SPS; k++) { double x = k - 2 * SPS; g[k] = exp(-x * x / (2 * sigma * sigma)); sum += g[k]; }
            for (int k = 0; k <= 4 * SPS; k++) g[k] /= sum;
            static double nrz[(NB + 8) * SPS], f[(NB + 8) * SPS];
            for (int n = 0; n < (NB + 8) * SPS; n++) { int bi = n / SPS - 4; nrz[n] = (bi >= 0 && bi < NB) ? (bits[bi] ? 1.0 : -1.0) : -1.0; }
            for (int n = 0; n < (NB + 8) * SPS; n++) { double a = 0; for (int k = 0; k <= 4 * SPS; k++) { int m = n + k - 2 * SPS; if (m >= 0 && m < (NB + 8) * SPS) a += g[k] * nrz[m]; } f[n] = a; }
            double ph = 0; fm_demod_t d = { 0, 0 }; int wrong = 0, counted = 0;
            for (int n = 0; n < (NB + 8) * SPS; n++) {
                ph += f[n] * 3.14159265358979323846 * 0.5 / SPS;
                int32_t i = (int32_t)lround(25000.0 * cos(ph)), q = (int32_t)lround(25000.0 * sin(ph));
                uint8_t dec = gfsk_decide_bit(&d, i, q);
                int bi = n / SPS - 4;
                if (bi >= 1 && bi < NB && n % SPS == SPS / 2) { counted++; if (dec != bits[bi]) wrong++; }
            }
            CHECK(wrong == 0, "Gaussian FSK (BT 0.5, h 0.5): %d of %d bit-centre decisions wrong", wrong, counted);
        }
        /* (3) sign convention: reversing the rotation direction flips every decision; exactly zero deviation decides 0 */
        { fm_demod_t d = { 0, 0 }; int ones = 0; double ph = 0; for (int n = 0; n < 40; n++) { ph -= 0.2; ones += gfsk_decide_bit(&d, (int32_t)lround(20000 * cos(ph)), (int32_t)lround(20000 * sin(ph))); } CHECK(ones == 0, "negative frequency deviation -> 0 (got %d ones)", ones); }
        { fm_demod_t d = { 0, 0 }; int ones = 0; for (int n = 0; n < 40; n++) ones += gfsk_decide_bit(&d, 20000, 0); CHECK(ones == 0, "zero deviation (unmodulated carrier) decides 0 (ties go to 0)"); }
        /* fm_demod_process() returns cross >> 16, and the bit decision is > 0: a weak positive deviation is shifted down to 0.
         * With h = 0.5 at 8 samples/bit (0.196 rad/sample) a 1 survives only if A^2 sin(0.196) / 65536 >= 1, i.e. A >= ~580 counts. */
        { fm_demod_t d = { 0, 0 }; int ones = 0; double ph = 0; for (int n = 0; n < 40; n++) { ph += 0.196; ones += gfsk_decide_bit(&d, (int32_t)lround(400 * cos(ph)), (int32_t)lround(400 * sin(ph))); }
          CHECK(ones == 0, "amplitude 400 (1.2%% of full scale): positive deviation is lost in the >>16 -> every '1' decided as 0 (%d ones)", ones);
          fm_demod_t e = { 0, 0 }; int ones2 = 0; ph = 0; for (int n = 0; n < 40; n++) { ph += 0.196; ones2 += gfsk_decide_bit(&e, (int32_t)lround(800 * cos(ph)), (int32_t)lround(800 * sin(ph))); }
          CHECK(ones2 > 30, "amplitude 800 is above the dead zone (%d ones of 40)", ones2); }
    }
    TEST_DONE("test_D_ble");
}
