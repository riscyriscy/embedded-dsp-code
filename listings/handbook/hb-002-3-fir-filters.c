/*
 * Embedded DSP Handbook, Chapter 2: FIR and IIR Filters / 1. FIR Filters
 * Listing 3 of 119 (index 2, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: include/hb_fir_float.h src/hb_fir_float.c | Checked by: tests/test_A_fir_float.c (15 checks): impulse response, vs double convolution over ~19 ring wraps, boxcar -13 dB side lobe, linear phase
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
#define FIR_TAPS 32

typedef struct {
    float coeffs[FIR_TAPS];
    float history[FIR_TAPS];
    int idx;
} fir_filter_t;

void fir_init(fir_filter_t *f, const float *coeffs) {
    for (int i = 0; i < FIR_TAPS; i++) {
        f->coeffs[i] = coeffs[i];
        f->history[i] = 0.0f;
    }
    f->idx = 0;
}

float fir_process(fir_filter_t *f, float input) {
    f->history[f->idx] = input;

    float acc = 0.0f;
    int idx = f->idx;
    for (int i = 0; i < FIR_TAPS; i++) {
        acc += f->coeffs[i] * f->history[idx];
        idx = (idx == 0) ? FIR_TAPS - 1 : idx - 1;
    }

    f->idx = (f->idx + 1) % FIR_TAPS;
    return acc;
}
