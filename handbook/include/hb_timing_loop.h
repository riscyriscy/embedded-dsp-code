/* Source: Embedded DSP Handbook, Chapter 8 (2. Symbol timing recovery): timing_loop_t, timing_loop_update().
 * MIT License. */
#ifndef HB_TIMING_LOOP_H
#define HB_TIMING_LOOP_H

#include <stdint.h>

typedef struct {
    int32_t integrator;
    int32_t kp, ki;  // proportional/integral gains, tuned for loop bandwidth
                     // (Q16, mu units per detector unit: TIMING_GAIN_Q16)
} timing_loop_t;

/* ---- function prototypes / shared state ---- */
int32_t timing_loop_update(timing_loop_t *l, int32_t error);

#endif /* HB_TIMING_LOOP_H */
