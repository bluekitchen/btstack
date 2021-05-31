# BTstack Port for STM32WB55 Nucleo Boards using FreeRTOS

This port supports the Nucleo68 and the USB dongle of the [P-NUCLEO-WB55 kit](https://www.st.com/en/evaluation-tools/p-nucleo-wb55.html). Both have 1 MB of Flash memory.

The STM32Cube_FW_WB_V1.3.0 provides the HAL and WPAN, and initializes the device and the initial Makefile.
For easy development, Ozone project files are generated as well.

## Hardware

In this port, the Nucelo68 or the USB Dongle from the P-NUCLEO-WB55 can be used.
Development was done using FUS v1.0.1 and v1.0.2 and Full BLE Stack v1.3.1.
See STM32Cube_FW_WB_V1.3.0/Projects/STM32WB_Copro_Wireless_Binaries/Release_Notes.html for update instructions.

Note: Segger RTT currently doesn't work as output stops after CPU2 (radio controller) has started up.

### Nucleo68

The debug output is sent over USART1 and is available via the ST-Link v2.

### USB Dongle

To flash the dongle, SWD can be used via the lower 6 pins on CN1:
  - 3V3
  - PB3 - SWO (semi hosting not used)
  - PA14 - SCLK
  - PA13 - SWDIO
  - NRST
  - GND

The debug output is sent over USART1 and is available via PB6.

## Software

FreeRTOS V10.2.0 is used to run stack, you can get this example version by checking out official repo:

	$ cd Middlewares
	$ git submodule add https://github.com/aws/amazon-freertos.git
	$ git submodule update
	& cd amazon-freertos && git checkout v1.4.8

Or by specifying path to FreeRTOS

	$ make FREERTOS_ROOT=path_to_freertos

To build all examples, run make

	$ make

All examples and the .jedbug Ozone project files are placed in the 'build' folder.

## Flash And Run The Examples

The Makefile builds different versions: 
- example.elf: .elf file with all debug information
- example.bin: .bin file that can be used for flashing

### Nucleo68

There are different options to flash and debug the Nucleo68 board. The Nucleo68 boards comes with an on-board [ST-Link programmer and debugger](https://www.st.com/en/development-tools/st-link-v2.html). As an alternative, the ST-Link programmer can be replaced by an [SEGGER J-Link OB](https://www.segger.com/products/debug-probes/j-link/models/other-j-links/st-link-on-board/). Finally, the STM32 can be programmed with any ARM Cortex JTAG or SWD programmer via the SWD jumper.

### USB Dongle

Please use any ARM Cortex SWD programmer via the SWD interface desribed in the hardware section.

## Run Example Project using Ozone

When using an external J-Link programmer or after installing J-Link OB on the Nucleo68 board, you can flash and debug using the cross-platform [SEGGER Ozone Debugger](https://www.segger.com/products/development-tools/ozone-j-link-debugger/). It is included in some J-Link programmers or can be used for free for evaluation usage.

Just start Ozone and open the .jdebug file in the build folder. When compiled with "ENABLE_SEGGER_RTT", the debug output shows up in the Terminal window of Ozone. Note: as mentioned before, Segger RTT currently stops working when CPU2 has started up.

## Debug output

All debug output can be either send via SEGGER RTT or via USART1. To get the console from USART1, simply connect your board under STLink-v2 to your PC or connect PB6 (USART1 TX) of the Nucleo board to an USB-2-UART adapter and open a terminal at 115200.

In src/btstack_config.h resp. in example/btstack_config.h of the generated projects, additional debug information can be enabled by uncommenting ENABLE_LOG_INFO.

Also, the full packet log can be enabled in src/btstack_port.c by uncommenting the hci_dump_init(..) line. The console output can then be converted into .pklg files for OS X PacketLogger or WireShark by running tool/create_packet_log.py

## GATT Database
In BTstack, the GATT Database is defined via the .gatt file in the example folder. During the build, the .gatt file is converted into a .h file with a binary representation of the GATT Database and useful defines for the application.
