/* TEST-ONLY STUB (group B).  Declares just enough of CMSIS-DSP's arm_math.h for src/hb_cmsis_q15_ref.c to compile on a host.
 * It implements nothing: tests/test_B_cmsis_spy.c supplies recording "spy" functions.  Signatures follow CMSIS-DSP's documented
 * prototypes (arm_rfft_init_q15 / arm_rfft_q15 / arm_cfft_q15). */
#ifndef HB_B_ARM_MATH_STUB_H
#define HB_B_ARM_MATH_STUB_H
#include <stdint.h>
typedef int16_t q15_t;
typedef int arm_status;
typedef struct { uint32_t fftLenReal; uint8_t ifftFlagR; uint8_t bitReverseFlagR; } arm_rfft_instance_q15;
typedef struct { uint16_t fftLen; } arm_cfft_instance_q15;
arm_status arm_rfft_init_q15(arm_rfft_instance_q15 *S, uint32_t fftLenReal, uint32_t ifftFlagR, uint32_t bitReverseFlag);
void arm_rfft_q15(const arm_rfft_instance_q15 *S, q15_t *pSrc, q15_t *pDst);
void arm_cfft_q15(const arm_cfft_instance_q15 *S, q15_t *p1, uint8_t ifftFlag, uint8_t bitReverseFlag);
#endif
