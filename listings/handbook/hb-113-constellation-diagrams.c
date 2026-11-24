/*
 * Embedded DSP Handbook, Chapter 22: Test and Validation Methodology / Constellation diagrams
 * Listing 114 of 119 (index 113, counting from 0 in book order), exactly as it appears in the book.
 * Status: tested | Built as: src/hb_testkit.c include/hb_testkit.h | Checked by: tests/test_D_testkit.c: stdout captured and compared byte-for-byte (negative values, INT32_MIN/MAX, count 0)
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Dump a captured I/Q buffer over UART for host-side constellation plotting.
// (Simple text format; a binary format is more efficient for large capture buffers.)
void dump_iq_buffer_uart(int32_t *i_samples, int32_t *q_samples, int count) {
    for (int n = 0; n < count; n++) {
        printf("%ld,%ld\n", (long)i_samples[n], (long)q_samples[n]);  // routed to UART/semihosting
    }
}
