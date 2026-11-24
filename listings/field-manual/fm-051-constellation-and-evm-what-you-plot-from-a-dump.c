/*
 * Embedded DSP Field Manual, Chapter 10: Testing the Chain / Constellation and EVM: what you plot from a dump
 * Listing 52 of 61 (index 51, counting from 0 in book order), exactly as it appears in the book.
 * Status: compiled | Built as: include/fm_ber.h src/fm_ber.c | Checked by: builds clean; printf-only UART dump helpers, not behaviourally tested
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
void dump_iq_buffer_uart(int32_t *i_samples, int32_t *q_samples, int count) {
    for (int n = 0; n < count; n++)
        printf("%ld,%ld\n", (long)i_samples[n], (long)q_samples[n]);
}
