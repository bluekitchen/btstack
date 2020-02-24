# BTstack Port for QT with USB Bluetooth Dongle

Uses libusb Library on macOS and Linux and WinUSB on Windows.
Windows is supported with the MinGW Kit.

Windows with MSVC or Embedded (bare metal) platforms not supported yet.

## Compilation

On all platforms, you'll need Qt Python 3 installed.
On macOS/Linux [libusb-1.0](http://libusb.info) or higher is required, too.

When everything is ready, you can open the provided CMakelists.txt project in Qt Creator and run any of the provided examples.
See Qt documentation on how to compile on the command line or with other IDEs

## Environment Setup

## Windows

To allow WinUSB to access an USB Bluetooth dongle, you need to install a special device driver to make it accessible to user space processes.

It works like this:

-  Download [Zadig](http://zadig.akeo.ie)
-  Start Zadig
-  Select Options -> “List all devices”
-  Select USB Bluetooth dongle in the big pull down list
-  Select WinUSB in the right pull down list
-  Select “Replace Driver”

### Linux

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

### macOS

On macOS, the OS will try to use a plugged-in Bluetooth Controller if one is available. 
It's best to to tell the OS to always use the internal Bluetooth Contoller. 

For this, execute:

    sudo nvram bluetoothHostControllerSwitchBehavior=never

and then reboot to activate the change. 

Note: if you get this error,

	libusb: warning [darwin_open] USBDeviceOpen: another process has device opened for exclusive access
	libusb: error [darwin_reset_device] ResetDevice: device not opened for exclusive access

and you didn't start another instance and you didn't assign the USB Controller to a virtual machine,
macOS uses the plugged-in Bluetooth Controller. Please configure NVRAM as explained and try again after a reboot.


## Running the examples

BTstack's HCI USB transport will try to find a suitable Bluetooth module and use it. 

On start, BTstack will try to find a suitable Bluetooth module. It will also print the path to the packet log as well as the USB path.

	$ ./le_counter
	Packet Log: /tmp/hci_dump.pklg
	BTstack counter 0001
	USB Path: 06
	BTstack up and running on 00:1A:7D:DA:71:13.

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
