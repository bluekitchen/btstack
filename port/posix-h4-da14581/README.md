# BTstack Port for POSIX Systems with Dialog Semiconductor DA14581 Controller

This port allows to use the DA14581 connected via UART with BTstack running on a POSIX host system.

It first downloads the hci_581_active_uart.hex firmware from the  DA14581_HCI_3.110.2.12 SDK packet, before BTstack starts up.

Please note that it does not detect if the firmware has already been downloaded, so you need to reset the DA14581 before starting an example.

For production use, the HCI firmware could be flashed into the OTP and the firmware download could be skipped.

Tested with the official DA14581 Dev Kit on OS X.
