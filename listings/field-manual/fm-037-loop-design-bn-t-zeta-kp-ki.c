/*
 * Embedded DSP Field Manual, Chapter 6: Loop Tuning and Bring-Up / Loop design: Bn*T, zeta, Kp, Ki
 * Listing 38 of 57 (index 37, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/fm_loops.h | Checked by: tests/test_nco_qpsk.c (FLOAT_TO_Q16; COSTAS_KP_Q16 / COSTAS_KI_Q16 against the closed form)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Plain Q16 conversion, for a gain already in the units its loop consumes.
#define FLOAT_TO_Q16(f) ((int32_t)lrintf((f) * 65536.0f))

// NCO phase words per radian: 2^32 / 2*pi.
#define PHASE_WORDS_PER_RAD (4294967296.0 / 6.283185307179586)

// kd: detector counts per radian (2a for costas_phase_error); sps: samples per
// symbol. Round-to-nearest for positive gains; a constant expression, so these
// can initialize a struct.
#define COSTAS_KP_Q16(kp, kd) \
    ((int32_t)((kp) * PHASE_WORDS_PER_RAD / (kd) * 65536.0 + 0.5))
#define COSTAS_KI_Q16(ki, kd, sps) \
    ((int32_t)((ki) * PHASE_WORDS_PER_RAD / ((kd) * (sps)) * 65536.0 + 0.5))

// Kp = 0.05192, Ki = 0.001385 from the worked example above. Chapter 10's
// decision samples have a = 5600, so Kd = 11200, at 2 samples/symbol:
costas_loop_t costas = { .phase = 0, .freq = 0,
                         .kp = COSTAS_KP_Q16(0.05192, 11200.0),   // = 207671349
                         .ki = COSTAS_KI_Q16(0.001385, 11200.0, 2) };  // = 2769885
