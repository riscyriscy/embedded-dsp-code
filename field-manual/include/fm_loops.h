/* Loop-gain quantization macro and ppm->NCO offset helper.
 * Source: Field Manual, Chapter 6. */
#ifndef FM_LOOPS_H
#define FM_LOOPS_H

#include "fm_qformat.h"
#include "fm_nco.h"

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

/* ---- function prototypes / shared state ---- */
uint32_t ppm_to_phase_inc_offset(float f_c, float ppm, float f_s);

#endif /* FM_LOOPS_H */
