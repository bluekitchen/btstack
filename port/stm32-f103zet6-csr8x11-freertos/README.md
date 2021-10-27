# BTstack Port for STM32 F103ZET6 Board with CSR8X11, and support FreeRTOS

This port uses the STM32 F103ZET6 Board with CSR8X11 Bluetooth module.
STCubeMX was used to provide the HAL, initialize the device, and the Makefile. For easy development, Ozone project files are generated as well.

## Hardware

STM32 Development kit and adapter for CSR8X11 module:

## Software

To build all examples, run make

	$ make

All examples and the .jedbug Ozone project files are placed in the 'build' folder.


## Flash And Run The Examples

The Makefile builds different versions: 
- example.elf: .elf file with all debug information
- example.bin: .bin file that can be used for flashing

## Run Example Project using Ozone

## Debug output

All debug output can be either send via SEGGER RTT or via USART2. To get the console from USART2, connect PA2 (USART2 TX) of the Discovery board to an USB-2-UART adapter and open a terminal at 115200.

In src/btstack_config.h resp. in example/btstack_config.h of the generated projects, additional debug information can be enabled by uncommenting ENABLE_LOG_INFO.

Also, the full packet log can be enabled in src/port.c resp. btstack/port/stm32-f4discovery-cc256x/src/port.c by uncommenting the hci_dump_init(..) line. The console output can then be converted into .pklg files for OS X PacketLogger or WireShark by running tool/create_packet_log.py

## GATT Database
In BTstack, the GATT Database is defined via the .gatt file in the example folder. The Makefile contains rules to update the .h file when the .gatt was modified.


## Maintainer Notes - Updating The Port

The Audio BSP is from the STM32F4Cube V1.16 firmware and not generated from STM32CubeMX. To update the HAL, run 'generate code' in CubeMX. After that, make sure to re-apply the patches to the UART and check if the hal config was changed.

