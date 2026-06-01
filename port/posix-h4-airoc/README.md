# BTstack Port for POSIX Systems with modern Infineon (CYW) H4 Bluetooth Controller

This port has been archive as `port/archive/posix-h4-airoc`.

You can use modern Infineon AIROC Controllers that require download mode with the regular `posix-h4` port by
passing `--airoc-download-mode`, e.g. like this

	$ ./gatt_counter -u /dev/tty.usbserial-1234 --airoc-download-mode
