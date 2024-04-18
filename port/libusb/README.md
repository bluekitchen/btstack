# BTstack Port for POSIX Systems with libusb Library

## Compilation
The quickest way to try BTstack is on a Linux or OS X system with an
additional USB Bluetooth dongle. It requires
[pkg-config](http://www.freedesktop.org/wiki/Software/pkg-config/)
and [libusb-1.0](http://libusb.info) or higher to be
installed.

On a recent Debian-based system, all you need is:

	sudo apt-get install gcc git cmake ninja-build pkg-config libusb-1.0 portaudio19-dev


When everything is ready, you compile all examples with make:

	make

or using CMake + Ninja:

    mkdir build
    cd build
    cmake -G Ninja
    ninja

## Environment Setup

### Linux

On Linux, the USB Bluetooth dongle is usually not accessible to a regular user.

You can add a udev rule for your dongle to extend access rights to user processes.

For this, create `/etc/udev/rules.d/btstack.rules` and add this

	# Match all devices from CSR
	SUBSYSTEM=="usb", ATTRS{idVendor}=="0a12", MODE="0666"

	# Match all devices from Realtek
	SUBSYSTEM=="usb", ATTRS{idVendor}=="0bda", MODE="0666"

	# Match Cypress Semiconductor / Broadcom BCM20702A, e.g. DeLOCK Bluetooth 4.0 dongle
	SUBSYSTEM=="usb", ATTRS{idVendor}=="0a5c", ATTRS{idProduct}=="21e8", MODE="0666"

	# Match Asus BT400
	SUBSYSTEM=="usb", ATTRS{idVendor}=="0b05", ATTRS{idProduct}=="17cb", MODE="0666"

	# Match Laird BT860 / Cypress Semiconductor CYW20704A2
	SUBSYSTEM=="usb", ATTRS{idVendor}=="04b4", ATTRS{idProduct}=="f901", MODE="0666"

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

### Broadcom/Cypress/Infineon Controllers
During startup BTstack queries the Controlle for the Local Name, which is set to the Controller type (e.g. 'BCM20702A).
The chipset support uses this information to look for a local PatchRAM file of that name and uploads it.

### Realtek Controllers
During startup, the libusb HCI transport implementations reports the USB Vendor/Product ID, which is then forwarded to the Realtek chipset support.
The chipset support contains a mapping between USB Product ID and ( Patch, Configuration ) files. If found, these are
uploaded.


## Running the examples

BTstack's HCI USB transport will try to find a suitable Bluetooth module and use it. 

On start, BTstack will try to find a suitable Bluetooth module. It will also print the path to the packet log as well as the USB path.

	$ ./le_counter
	Packet Log: /tmp/hci_dump.pklg
	BTstack counter 0001
	Packet Log: /tmp/hci_dump_6.pklg
	USB device 0x0a12/0x0001, path: 06
    Local version information:
    - HCI Version    0x0006
    - HCI Revision   0x22bb
    - LMP Version    0x0006
    - LMP Subversion 0x22bb
    - Manufacturer 0x000a
	BTstack up and running on 00:1A:7D:DA:71:01.

If you want to run multiple examples at the same time, it helps to fix the path to the used Bluetooth module by passing -u usb-path to the executable.

Example running le_streamer and le_streamer_client in two processes, using CSR Bluetooth dongles at USB path 6 and 4:

	./le_streamer -u 6
	Specified USB Path: 06
	Packet Log: /tmp/hci_dump_6.pklg
	USB device 0x0a12/0x0001, path: 06
    Local version information:
    - HCI Version    0x0006
    - HCI Revision   0x22bb
    - LMP Version    0x0006
    - LMP Subversion 0x22bb
    - Manufacturer 0x000a
	BTstack up and running on 00:1A:7D:DA:71:01.
	To start the streaming, please run the le_streamer_client example on other device, or use some GATT Explorer, e.g. LightBlue, BLExplr.

	$ ./le_streamer_client -u 4
	Specified USB Path: 04
	Packet Log: /tmp/hci_dump_4.pklg
	USB device 0x0a12/0x0001, path: 04
    Local version information:
    - HCI Version    0x0006
    - HCI Revision   0x22bb
    - LMP Version    0x0006
    - LMP Subversion 0x22bb
    - Manufacturer 0x000a
	BTstack up and running on 00:1A:7D:DA:71:02.
	Start scanning!
