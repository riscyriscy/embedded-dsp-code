/*
 * Embedded DSP Handbook, Chapter 21: Forward Error Correction (FEC) / Interleaving: spreading burst errors across the code's error-correcting reach
 * Listing 110 of 119 (index 109, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_fec.c include/hb_fec.h | Checked by: tests/test_D_fec.c: closed-form permutation, exact inverse for 6 geometries, burst spread >= cols-1 (44) for every burst <= rows
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Block interleaver: write encoded bits into a matrix row by row, transmit column by column.
// A burst error hitting several consecutive TRANSMITTED bits (one column) lands on bits that
// were far apart in the ORIGINAL bit stream (spread across different rows), which is exactly
// the property that turns a burst error into scattered, individually correctable errors.
void interleave_block(const uint8_t *bits_in, uint8_t *bits_out, int rows, int cols) {
    for (int c = 0; c < cols; c++) {
        for (int r = 0; r < rows; r++) {
            bits_out[c * rows + r] = bits_in[r * cols + c];   // read column-major, write sequentially
        }
    }
}

// De-interleave at the receiver: the exact inverse permutation, restoring original bit order
// before depuncturing and Viterbi decoding.
void deinterleave_block(const uint8_t *bits_in, uint8_t *bits_out, int rows, int cols) {
    for (int c = 0; c < cols; c++) {
        for (int r = 0; r < rows; r++) {
            bits_out[r * cols + c] = bits_in[c * rows + r];
        }
    }
}
