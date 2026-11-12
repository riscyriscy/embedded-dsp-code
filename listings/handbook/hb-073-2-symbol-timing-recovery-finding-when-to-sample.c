/*
 * Embedded DSP Handbook, Chapter 8: Digital Demodulation In Depth / 2. Symbol timing recovery: finding when to sample
 * Listing 74 of 119 (index 73, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_timing_loop.h src/hb_timing_loop.c | Checked by: tests/test_C_ch8_timing.c: negative feedback, 64-bit products, clock-offset tracking +-1000 ppm
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
typedef struct {
    int32_t integrator;
    int32_t kp, ki;  // proportional/integral gains, tuned for loop bandwidth
                     // (Q16, mu units per detector unit: TIMING_GAIN_Q16)
} timing_loop_t;

// error is gardner_error()'s output, positive when the sampling instant is
// late. Returns the adjustment to ADD to t->mu (larger mu is later), so it is
// minus the PI output: late means move earlier. The Q16 products are far
// wider than 32 bits, so they are formed in 64 bits; adding half before the
// shift rounds to nearest (a bare >> 16 floors and biases the integrator
// downward).
int32_t timing_loop_update(timing_loop_t *l, int32_t error) {
    int64_t di = (int64_t)l->ki * error + (1 << 15);
    int64_t dp = (int64_t)l->kp * error + (1 << 15);
    l->integrator += (int32_t)(di >> 16);
    return -((int32_t)(dp >> 16) + l->integrator);   // added to t->mu
}
