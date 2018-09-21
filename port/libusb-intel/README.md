# BTstack Port for POSIX Systems with Intel Wireless 8260/8265 Controllers

Same as port/libusb, but customized for Intel Wireless 8260 and 8265 Controllers.
These controller require firmware upload and configuration to work. Firmware and config is downloaded from the Linux firmware repository.

## Compilation

Requirements:
- [pkg-config](http://www.freedesktop.org/wiki/Software/pkg-config/)
- [libusb-1.0](http://libusb.info)

On a recent Debian-based system, all you need is:

	apt-get install gcc git libusb-1.0 pkg-config


When everything is ready, you compile all examples with:

	make

## Environment

On Linux, the USB Bluetooth dongle is usually not accessible to a regular user. You can either:
- run the examples as root
- add a udev rule for your dongle to extend access rights to user processes

To add an udev rule, please create `/etc/udev/rules.d/btstack.rules` and add this

	# Match all devices from CSR
	SUBSYSTEM=="usb", ATTRS{idVendor}=="0a12", MODE="0666"

	# Match DeLOCK Bluetooth 4.0 dongle
	SUBSYSTEM=="usb", ATTRS{idVendor}=="0a5c", ATTRS{device}=="21e8", MODE="0666"

	# Match Asus BT400
	SUBSYSTEM=="usb", ATTRS{idVendor}=="0b05", ATTRS{device}=="17cb", MODE="0666"

	# Match Laird BT860 / Cypress Semiconductor CYW20704A2
	SUBSYSTEM=="usb", ATTRS{idVendor}=="04b4", ATTRS{device}=="f901", MODE="0666"

	# Match Intel Wireless 8260 8265
	SUBSYSTEM=="usb", ATTRS{idVendor}=="8027", ATTRS{device}=="0a2b, MODE="0666"

On macOS, the OS will try to use a plugged-in Bluetooth Controller if one is available. 
It's best to to tell the OS to always use the internal Bluetooth Contoller. 

For this, execute:

    sudo nvram bluetoothHostControllerSwitchBehavior=never

and then reboot to activate the change. 

## Running the examples

BTstack's HCI USB transport will try to find a suitable Bluetooth module and use it. 

On start, BTstack will try to find a suitable Bluetooth module. It will also print the path to the packet log as well as the USB path.

	$ ./le_counter
	Packet Log: /tmp/hci_dump.pklg
	USB Path: 03-01-04-03
	Firwmare ./ibt-12-16.sfi
	Firmware upload complete
	Firmware operational
	Done 1
	BTstack counter 0001
	USB Path: 03-01-04-03
BTstack up and running on F8:34:41:D5:BE:6F.

If you want to run multiple examples at the same time, it helps to fix the path to the used Bluetooth module by passing -u usb-path to the executable.

Example running le_streamer and le_streamer_client in two processes, using Bluetooth dongles at USB path 6 and 4:

	./le_streamer -u 6
	Specified USB Path: 06
	Packet Log: /tmp/hci_dump_6.pklg
	USB Path: 06
	BTstack up and running on 00:1A:7D:DA:71:13.
	To start the streaming, please run the le_streamer_client example on other device, or use some GATT Explorer, e.g. LightBlue, BLExplr.

	$ ./le_streamer_client -u 4
	Specified USB Path: 04
	Packet Log: /tmp/hci_dump_4.pklg
	USB Path: 04
	BTstack up and running on 00:1A:7D:DA:71:13.
	Start scanning!
