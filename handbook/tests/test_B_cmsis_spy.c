/* HB[35], HB[36]: arm_rfft_q15 / arm_cfft_q15 production listings (src/hb_cmsis_q15_ref.c).  STATUS: syntax-only.
 * CMSIS-DSP itself is not available, so the listing is compiled against a test-only stub header and spy functions that record
 * their arguments.  What this CAN check: the listing compiles as C11, and the arguments its comments describe are what is passed:
 *   arm_rfft_init_q15(&inst, LAB0_N, ifftFlagR = 0, bitReverseFlag = 1);   arm_cfft_q15(&len256, buf, 0, 1);
 * and that the output buffer is 2 * LAB0_N q15 values.  What it cannot check: the numerical behaviour of the library. */
#include <stdint.h>
#include "check.h"
#include "hb_B_arm_math_stub.h"
#define LAB0_N 256     /* [harness] LAB0_N comes from Lab 0 (Chapter 1); any power-of-2 length works here */

/* ---- spies ---- */
static struct { int calls; uint32_t len, ifft, brev; const arm_rfft_instance_q15 *S; } spy_init;
static struct { int calls; const arm_rfft_instance_q15 *S; q15_t *src, *dst; } spy_rfft;
static struct { int calls; const arm_cfft_instance_q15 *S; q15_t *buf; uint8_t ifft, brev; } spy_cfft;
arm_status arm_rfft_init_q15(arm_rfft_instance_q15 *S, uint32_t n, uint32_t ifft, uint32_t brev) { spy_init.calls++; spy_init.S = S; spy_init.len = n; spy_init.ifft = ifft; spy_init.brev = brev; return 0; }
void arm_rfft_q15(const arm_rfft_instance_q15 *S, q15_t *src, q15_t *dst) { spy_rfft.calls++; spy_rfft.S = S; spy_rfft.src = src; spy_rfft.dst = dst; }
void arm_cfft_q15(const arm_cfft_instance_q15 *S, q15_t *p, uint8_t ifft, uint8_t brev) { spy_cfft.calls++; spy_cfft.S = S; spy_cfft.buf = p; spy_cfft.ifft = ifft; spy_cfft.brev = brev; }
const arm_cfft_instance_q15 arm_cfft_sR_q15_len256 = { 256 };   /* [harness] CMSIS-DSP's own precomputed instance, stood in for */

#include "../src/hb_cmsis_q15_ref.c"   /* the listing under test, verbatim */

int main(void) {
    lab0_q15_init();
    CHECK(spy_init.calls == 1, "lab0_q15_init calls arm_rfft_init_q15 once");
    CHECK(spy_init.len == LAB0_N, "fftLen = LAB0_N, got %u", (unsigned)spy_init.len);
    CHECK(spy_init.ifft == 0, "ifftFlagR = 0 (forward), got %u", (unsigned)spy_init.ifft);
    CHECK(spy_init.brev == 1, "bitReverseFlag = 1 (normal order) as the comment says, got %u", (unsigned)spy_init.brev);
    CHECK(spy_init.S == &lab0_rfft_q15, "init targets the file-static instance");
    lab0_transform_q15();
    CHECK(spy_rfft.calls == 1 && spy_rfft.S == &lab0_rfft_q15, "lab0_transform_q15 calls arm_rfft_q15 with the instance");
    CHECK(spy_rfft.src == lab0_buf_q15 && spy_rfft.dst == lab0_fft_out_q15, "src = lab0_buf_q15, dst = lab0_fft_out_q15");
    CHECK(sizeof lab0_buf_q15 == LAB0_N * sizeof(q15_t), "input buffer is LAB0_N q15 samples");
    CHECK(sizeof lab0_fft_out_q15 == 2u * LAB0_N * sizeof(q15_t), "output buffer is 2 * LAB0_N q15 values (the comment's point)");
    q15_t iq[2 * LAB0_N];
    iq_transform_q15(iq);
    CHECK(spy_cfft.calls == 1 && spy_cfft.S == &arm_cfft_sR_q15_len256 && spy_cfft.buf == iq, "iq_transform_q15 transforms the caller's buffer in place with the len256 instance");
    CHECK(spy_cfft.ifft == 0 && spy_cfft.brev == 1, "arm_cfft_q15 flags (ifftFlag, bitReverseFlag) = (0, 1), got (%u, %u)", spy_cfft.ifft, spy_cfft.brev);
    TEST_DONE("test_B_cmsis_spy");
}
