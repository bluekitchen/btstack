# BTstack Port for POSIX Systems with H4 Bluetooth Controller

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

BTstack's POSIX-H4 does not have additional dependencies. You can directly run make.

	make


## Running the examples

On start, BTstack prints the path to the packet log and prints the information on the detected Buetooth Controller.

	$ ./le_counter
	Packet Log: /tmp/hci_dump.pklg
	BTstack counter 0001
	BTstack up and running on 00:1A:7D:DA:71:13.

Please note that BTstack will increase the baudrate. Before starting again, you should reset or power-cycle the Bluetooth Controller.
