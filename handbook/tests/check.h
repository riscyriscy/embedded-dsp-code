/* Minimal test helpers (no framework needed). */
#ifndef CHECK_H
#define CHECK_H
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static int g_checks = 0, g_failed = 0;
#define CHECK(cond, ...) do { g_checks++; if (!(cond)) { g_failed++; \
    printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define CHECK_NEAR(got, want, tol, name) do { double g_ = (double)(got), w_ = (double)(want); g_checks++; \
    if (!(fabs(g_ - w_) <= (tol))) { g_failed++; \
    printf("  FAIL %s:%d: %s: got %g, want %g (+-%g)\n", __FILE__, __LINE__, name, g_, w_, (double)(tol)); } } while (0)
#define TEST_DONE(name) do { printf("%s: %d checks, %d failed -> %s\n", name, g_checks, g_failed, g_failed ? "FAIL" : "PASS"); \
    return g_failed ? 1 : 0; } while (0)
/* Small deterministic PRNG so every run is reproducible (not for cryptography). */
static uint32_t lcg_state = 12345u;
static inline uint32_t lcg_next(void) { lcg_state = lcg_state * 1664525u + 1013904223u; return lcg_state >> 8; }
#endif
