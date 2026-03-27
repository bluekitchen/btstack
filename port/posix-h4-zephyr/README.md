# BTstack Port for POSIX Systems with Zephyr-based Controller

The main difference of Zephyr-based Nordic Bluetooth Controllers is that:
- the nRF52- and nRF54-Series use 1000000 as baud rate instead of 115200
- they don't have a public BD_ADDR.

You can use these with the regular posix-h4 port by setting the baudrate to 1000000, e.g. like this

	$ ./gatt_counter -u /dev/tty.usbmodem1234 --baud 1000000

The fixed random static address used automatically.
