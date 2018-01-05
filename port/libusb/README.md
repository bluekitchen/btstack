# BTstack Port for POSIX Systems with libusb library

The quickest way to try BTstack is on a Linux or OS X system with an
additional USB Bluetooth dongle. It requires
[pkg-config](http://www.freedesktop.org/wiki/Software/pkg-config/)
and [libusb-1.0](http://libusb.info) or higher to be
installed.

On Linux, the USB Bluetooth donle is usually not accessible to a regular user. You can:
- run the examples as root
- add a udev rule for your dongle to extend access rights to user processes

To add an udev rule, please create `/etc/udev/rules.d/btstack.rules` and add this

	# Match all devices from CSR
	SUBSYSTEM=="usb", ATTRS{idVendor}=="0a12", MODE="0666"

	# Match DeLOCK Bluetooth 4.0 dongle
	SUBSYSTEM=="usb", ATTRS{idVendor}=="0a5c", ATTRS{device}=="21e8", MODE="0666"

	# Match Asus BT400
	SUBSYSTEM=="usb", ATTRS{idVendor}=="0b05", ATTRS{device}=="17cb", MODE="0666"

On OS X, it’s necessary to tell the OS to only use the internal
Bluetooth. For this, execute:

    sudo nvram bluetoothHostControllerSwitchBehavior=never

and then reboot to activate the change.

