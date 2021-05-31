# BTstack Port for POSIX Systems with Dialog Semiconductor DA14585 Controller

This port allows to use the DA14585 connected via UART with BTstack running on a POSIX host system.

It first downloads the hci_581.hex firmware from the 6.0.8.509 SDK, before BTstack starts up.

Please note that it does not detect if the firmware has already been downloaded, so you need to reset the DA14585 before starting an example.

For production use, the HCI firmware could be flashed into the OTP and the firmware download could be skipped.

Tested with the official DA14585 Dev Kit Basic on OS X.
