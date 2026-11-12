// Phase error after one hour of a 1 kHz tone at fs = 48 kHz, for four phase representations.
// Total phase in turns = completed wraps + accumulator; ideal = 1000 Hz * 3600 s = 3.6e6 turns.
#include <math.h>
#include <stdint.h>
#include <stdio.h>
int main(void) {
    const long N = 48000L * 3600L;
    const double ideal = 1000.0 * 3600.0;
    // uint32 accumulator, phase_inc rounded to nearest
    uint32_t inc = (uint32_t)llround(1000.0 / 48000.0 * 4294967296.0), acc = 0;
    long wraps = 0;
    for (long i = 0; i < N; i++) { uint32_t p = acc; acc += inc; if (acc < p) wraps++; }
    printf("uint32 accumulator:        %+9.3f deg\n", (wraps + acc / 4294967296.0 - ideal) * 360.0);
    // float32 accumulator in turns, wrapped by subtraction
    float finc = 1000.0f / 48000.0f, facc = 0.0f; wraps = 0;
    for (long i = 0; i < N; i++) { facc += finc; if (facc >= 1.0f) { facc -= 1.0f; wraps++; } }
    printf("float32 turns, wrapped:    %+9.3f deg\n", (wraps + (double)facc - ideal) * 360.0);
    // float64 accumulator in turns, wrapped by subtraction
    double dinc = 1000.0 / 48000.0, dacc = 0.0; wraps = 0;
    for (long i = 0; i < N; i++) { dacc += dinc; if (dacc >= 1.0) { dacc -= 1.0; wraps++; } }
    printf("float64 turns, wrapped:    %+9.3f deg\n", (wraps + dacc - ideal) * 360.0);
    // float32 radians, never wrapped
    float rinc = (float)(2.0 * M_PI * 1000.0 / 48000.0), racc = 0.0f;
    long stuck = -1;
    for (long i = 0; i < N; i++) { float before = racc; racc += rinc; if (stuck < 0 && racc == before) stuck = i; }
    double want = 2.0 * M_PI * ideal;
    printf("float32 radians, never wrapped:\n");
    printf("  final phase %.6g rad, ideal %.6g rad\n", (double)racc, want);
    printf("  spacing of floats there: %.2g rad, increment %.4f rad\n", (double)(nextafterf(racc, INFINITY) - racc), (double)rinc);
    printf("  phase stopped growing after %.0f s (sample %ld)\n", stuck / 48000.0, stuck);
    return 0;
}
