/*
 * Embedded DSP Handbook, Chapter 18: Cellular IoT Connectivity / The software interface: AT commands and an onboard IP stack
 * Listing 101 of 119 (index 100, counting from 0 in book order), exactly as it appears in the book.
 * Status: reference-only | Built as: reference/hbD_100_at_command_sequence.txt | Checked by: AT commands for a cellular module (not C, module-specific); archived
 * MIT License. The compiled modules under field-manual/ and handbook/ contain this text, with includes and prototypes added where C needs them.
 */
// Illustrative AT command sequence for opening a socket and sending data
// (exact commands are module- and carrier-specific; check the module's AT command reference).
AT+CFUN=1              // enable full functionality (radio on)
AT+CEREG?               // check network registration status
AT+CGDCONT=1,"IP","<apn>"   // set the packet-data context (APN) for the carrier
AT+USOCR=6              // create a TCP socket
AT+USOCO=0,"<host>",<port>  // connect the socket
AT+USOWR=0,<len>        // write <len> bytes to the socket
AT+USOCL=0              // close the socket
