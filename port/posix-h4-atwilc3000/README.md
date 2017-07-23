# POSIX H4 Port for Atmel ATWILC3000

This port allows to use the ATWILC3000 connected via UART with BTstack running on a POSIX host system.

It first downloads the hci_581_active_uart.hex firmware from the [GitHub atwilc3000/firmware](https://github.com/atwilc3000/firmware) repo, before BTstack starts up.

Please note that it does not detect if the firmware has already been downloaded, so you need to reset the ATWILC3000 before starting an example.

Tested with the official ATWILC3000 SHIELD on OS X.
