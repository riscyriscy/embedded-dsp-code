/* TEST-ONLY shim: just enough of CMSIS-DSP's arm_math.h to compile Lab 0 on a host.
 * NOT part of the book's code and NOT a CMSIS replacement: a naive O(N^2) DFT that
 * reproduces arm_rfft_fast_f32()'s packed output layout:
 *   out[0] = Re X[0] (DC), out[1] = Re X[N/2] (Nyquist), then (re, im) pairs for k = 1..N/2-1.
 * On a real target, use ARM's CMSIS-DSP instead. */
#ifndef SHIM_ARM_MATH_H
#define SHIM_ARM_MATH_H
#include <stdint.h>
#include <math.h>
#include <stdio.h>
typedef struct { uint16_t fftLen; } arm_rfft_fast_instance_f32;
static inline int arm_rfft_fast_init_f32(arm_rfft_fast_instance_f32 *S, uint16_t len) { S->fftLen = len; return 0; }
static inline void arm_rfft_fast_f32(const arm_rfft_fast_instance_f32 *S, float *in, float *out, uint8_t ifft) {
    (void)ifft;
    int N = S->fftLen;
    for (int k = 0; k <= N / 2; k++) {
        double re = 0.0, im = 0.0;
        for (int n = 0; n < N; n++) {
            double a = -2.0 * M_PI * k * n / N;
            re += in[n] * cos(a);
            im += in[n] * sin(a);
        }
        if (k == 0) out[0] = (float)re;
        else if (k == N / 2) out[1] = (float)re;
        else { out[2 * k] = (float)re; out[2 * k + 1] = (float)im; }
    }
}
#endif
