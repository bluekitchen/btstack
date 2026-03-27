# BTstack Port for POSIX Systems with H4 Bluetooth Controller

## Configuration
Most Bluetooth Bluetooth Controllers connected via UART/H4 require some special configuration, e.g. to set the UART baud rate, and/or require firmware patches during startup. 
In this port, we've tried to do most of these automatically based on information gathered from the Bluetooth Controller. Here's some Controller specific details:

## TI CC256x
The CC2564x needs the correct init script to start up. The Makfile already has entries for most silicon revisions:

- CC2560:  bluetooth_init_cc2564_2.14.c
- CC2564B: bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c
- CC2564C: bluetooth_init_cc2564C_1.5.c

Please pick the correct one. The main.c verifies that the correct script is loaded, but the init script is linked to the executable.

## Broadcom/Cypress/Infineon Controllers

The correct firmware file needs to be provided in the current working directory. 
The Makefile / CMake build downloads the one for the BCM43430 e.g. found on later Raspberry Pi editions. 

## Nordic Controller with HCI UART firmware

We maintain an enhanced / configured version of the official HCI UART example here:
https://github.com/bluekitchen/hci_uart_iso_timesync/tree/main

The main difference of Zephyr-based Nordic Bluetooth Controllers is that:
- the nRF52- and nRF54-Series use 1000000 as baud rate instead of 115200
- they don't have a public BD_ADDR.

You can use these by setting the baudrate to 1000000, e.g. like this

	$ ./gatt_counter -u /dev/tty.usbmodem1234 --baud 1000000

The fixed random static address used automatically.

## Compilation

BTstack's POSIX-H4 does not have additional dependencies. You can directly run make

	$ make

or with CMake/Ninja

    $ mkdir build
    $ cd build
    $ cmake -G Ninja ..
    $ ninja

## Command-line options

All examples provide the following command line options:

    --help               | -h            		    print (this) help.
    --logfile            | -l  LOGFILE   		    set file to store debug output and HCI trace.
    --logformat          | -f  btsnoop|bluez|pklg	set file format to store debug output in.
    --reset-tlv          | -r            		    reset bonding information stored in TLV.
    --tty                | -u  TTY       		    set path to Bluetooth Controller.
    --bd-addr            | -m  BD_ADDR   		    set random static Bluetooth address.
    --baudrate           | -b  BAUDRATE  		    set initial baudrate.
    --airoc-download-mode| -d                       enable AIROC Download Mode for newer CYW55xx Controller

## Infineon AIROC Controller

Newer Infineon Airoc (tm) Controllers like the CYW55xx series accept PatchRAM upload only in a so-called 'Download Mode' 
or 'Auto-Baud Mode' which is entered by asserting CTS (low) and starting/resetting the controller via BT_REG_EN.
The PatchRAM name is retrieved via a custom HCI Command from the Controller and loaded from the current folder.

To enable this mode, please provide '--airoc-download-mode' on the command line.

As this doing a power cycle is usually not possible on a desktop system, this port request the user to press the "RESET"
button while showing a countdown.

## Running the examples

On start, BTstack prints the path to the packet log and prints the information on the detected Buetooth Controller.

	$ ./le_counter
	Packet Log: /tmp/hci_dump.pklg
	BTstack counter 0001
	BTstack up and running on 00:1A:7D:DA:71:13.

Please note that BTstack will increase the baudrate. Before starting again, you should reset or power-cycle the Bluetooth Controller.
