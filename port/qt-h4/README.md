# BTstack Port for QT with H4 Bluetooth Controller

Uses libusb Library on macOS and Linux and WinUSB on Windows.
Windows is supported with the MinGW Kit.

Windows with MSVC or Embedded (bare metal) platforms not supported yet.

## Configuration

Most Bluetooth Bluetooth Controllers connected via UART/H4 require some special configuration, e.g. to set the UART baud rate, and/or require firmware patches during startup. In this port, we've tried to do most of these automatically based on information gathered from the Bluetooth Controller. Here's some Controller specific details:

## TI CC256x
The CC2564x needs the correct init script to start up. The Makfile already has entries for most silicon revisions:

- CC2560:  bluetooth_init_cc2564_2.14.c
- CC2564B: bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c
- CC2564C: bluetooth_init_cc2564C_1.5.c

Please pick the correct one. The main.c verifies that the correct script is loaded, but the init script is linked to the executable.

## Broadcom BCM/CYW 43430

The correct firmware file needs to be provided in the current working directory. The Makefile downloads the one for the BCM43430 e.g. found on later Raspberry Pi editions. Please see the separate port/raspi, too.

## Compilation

On all platforms, you'll need Qt Python 3 installed.
On macOS/Linux [libusb-1.0](http://libusb.info) or higher is required, too.

When everything is ready, you can open the provided CMakelists.txt project in Qt Creator and run any of the provided examples.
See Qt documentation on how to compile on the command line or with other IDEs


## Running the examples

BTstack's HCI USB transport will try to find a suitable Bluetooth module and use it. 

On start, BTstack will try to find a suitable Bluetooth module. It will also print the path to the packet log as well as the USB path.

	$ ./le_counter
	Packet Log: /tmp/hci_dump.pklg
	BTstack counter 0001
	USB Path: 06
	BTstack up and running on 00:1A:7D:DA:71:13.

