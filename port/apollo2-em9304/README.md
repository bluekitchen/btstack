# BTstack port for Ambiq Apollo2 with EM9304

This port uses the Ambiq Apollo2 EVB with their EM9304 (AM_BLE) shield.
HAL and BSP from Ambiq Suite 1.2.11 were used together with the regular ARM GCC.
Upload is possible via the internal J-Link interface or the 10-pin Mini ARM-JTAG Interface.

## Hardware

Ambiq Apollo2 EVB + AM_BLE Shield 
- http://ambiqmicro.com/apollo-ultra-low-power-mcus/apollo2-mcu/

## Software

AmbiqSuite:
- http://ambiqmicro.com/apollo-ultra-low-power-mcus/apollo2-mcu/

Please clone BTstack as AmbiqSuite/third-party/bstack folder into the AmbiqSuite.

## Create Example Projects

To create example GCC projects, go to the Apollo2-EM9304 folder

	$ cd port/apollo2-em9304

and run make

	$ ./create_examples.py

All examples are placed in the boards/apollo2_evb_am_ble/examples folder with btstack_ prefix.


## Compile & Run Example Project

Go to to the gcc folder of one of the example folders and run make

    $ run make

To upload, please follow the instructions in the Apollo Getting Started documents.

## Debug output

printf is routed over the USB connector of the EVB at 115200.

In port/apollo2-em9304/btstack_config.h additional debug information can be enabled by uncommenting ENABLE_LOG_INFO.

Also, the full packet log can be enabled in src/btstack_port.c by uncommenting the hci_dump_open(..) line. The console output can then be converted into .pklg files for OS X PacketLogger or WireShark by running tool/create_packet_log.py
