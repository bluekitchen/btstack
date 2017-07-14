# BTstack port for STM32 Nucleo-L053R8 Board with an EM9304 Shield - EM9304 DVK

This port uses the STM32 Nucleo-L053R8 Board with EM's EM9304 Shield. 

The STM32CubeMX tool was used to provide the HAL, initialize the device, and create a basic Makefile. The Makefile has been exteneded to compile all BTstack LE examples. An examples can be uploaded onto the device by copying it to the Nucleo virtual mass storage drive.

## Hardware

In this port, the EM9304 is conencted via the SPI1 interface and configured for 8 Mhz. [Datasheet for the EM9304](http://www.emmicroelectronic.com/sites/default/files/public/products/datasheets/9304-ds_0.pdf)

## Software

To build all examples, go to the cubemx-l053r8-em9304 folder

	$ cd cubemx-l053r8-em9304

and run make

	$ make

All examples are placed in the 'build' folder.

## Debug output
printf is routed to USART2. To get the console output, open a terminal at 115200 for the virtual serial port of the Nucleo board.

Additional debug information can be enabled by uncommenting ENABLE_LOG_INFO in btstack_config.h.

Also, the full packet log can be enabled in port.c by uncommenting the hci_dump_open(..) line. The console output can then be converted into .pklg files for OS X PacketLogger or WireShark by running tool/create_packet_log.py

## GATT Database
In BTstack, the GATT Database is defined via the .gatt file in the example folder. During the build, the .gatt file is converted into a .h file with a binary representation of the GATT Database and useful defines for the application.

## Stability
When sending at full speed, it's possible that the EM9304 hangs up. This [patch (connection_event_overlay.emp)](https://bluekitchen-gmbh.com/files/em/patches/connection_event_overlay.emp) fixes this problems in our tests. It can be applied with the EM ConfigEditor available via the EM Technical Support.

## Multiple LE Peripheral/Central Roles
It should be possible to use the EM9304 in multiple roles. In the default configuration, it only supports a single connection. For multiple roles, it needs to be configured for more connections at least.

## Performace
With the patch, the LE Streamer example is able to send up to 10 packets per Connection Event to an iPhone SE with iOS 10.2, resulting in around 6.5 kB/s throughput for this direction.

## Image
![EM9304 DVK](EM9304DVK.jpg)
