# BTstack Port for SAMV71 Ultra Xplained with ATWILC3000 SHIELD

This port uses the [SAMV71 Ultra Xplained Ultra](http://www.atmel.com/tools/atsamv71-xult.aspx) evaluation kit with an [ATWILC3000 SHIELD](http://www.microchip.com/DevelopmentTools/ProductDetails.aspx?PartNO=ATWILC3000-SHLD). The code is based on the Advanced Software Framework (ASF) (previously know as Atmel Software Framework). It uses the GCC Makefiles provided by the ASF. OpenOCD is used to upload the firmware to the device.

## Create Example Projects

To create all example projects in the example folder, you can run:

    $ make

## Compile Example

In one of the example folders:

    $ make

To upload the firmware:

    $ make flash

You need to connect the the Debug USB to your computer.

## Debug output
printf is routed to USART1, which is connected to the virtual serial port. To get the console output, open a terminal at 115200.

In btstack_config.h, additional debug information can be enabled by uncommenting ENABLE_LOG_INFO.

Also, the full packet log can be enabled in the main() function on main.c by uncommenting the hci_dump_init(..) line. The console output can then be converted into .pklg files for OS X PacketLogger or WireShark by running tool/create_packet_log.py

## TODOs
    - Implement hal_flash_sector.h to persist link keys

## Issues
    - Bluetooth UART driver uses per-byte interrupts and doesn't work reliable at higher baud rates (921600 seems ok, 2 mbps already causes problems).
    - An older XDMA-based implementation only sends 0x00 bytes over the UART. It might be better to just read incoming data into two buffers, (e.g. using a two element linked list with XDMA), and raising RTS when one buffer is full.
