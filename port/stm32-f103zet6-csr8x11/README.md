# BTstack Port for STM32 F4 Discovery Board with CC256x

This port uses the STM32 F4 Discovery Board with TI's CC256XEM ST Adapter Kit that allows to plug in a CC256xB or CC256xC Bluetooth module.
STCubeMX was used to provide the HAL, initialize the device, and the Makefile. For easy development, Ozone project files are generated as well.

## Hardware

STM32 Development kit and adapter for CC256x module:
- [STM32 F4 Discovery Board](http://www.st.com/en/evaluation-tools/stm32f4discovery.html)
- [CC256xEM Bluetooth Adatper Kit for ST](https://store.ti.com/CC256XEM-STADAPT-CC256xEM-Bluetooth-Adapter-Kit-P45158.aspx)

CC256x Bluetooth module:
- [CC2564B Dual-mode Bluetooth® Controller Evaluation Module](https://store.ti.com/cc2564modnem.aspx)
- [CC2564C Dual-mode Bluetooth® Controller Evaluation Module](https://store.ti.com/CC256XCQFN-EM-CC2564C-Dual-Mode-Bluetooth-Controller-Evaluation-Module-P51277.aspx)

The module with the older CC2564B is around USD 20, while the one with the new CC2564C costs around USD 60. The projects are configured for the CC2564B. When using the CC2564C, *bluetooth_init_cc2564C_1.4.c* should be used as cc256x_init_script.

## Software

To build all examples, run make

	$ make

All examples and the .jedbug Ozone project files are placed in the 'build' folder.


## Flash And Run The Examples

The Makefile builds different versions: 
- example.elf: .elf file with all debug information
- example.bin: .bin file that can be used for flashing

There are different options to flash and debug the F4 Discovery board. The F4 Discovery boards comes with an on-board [ST-Link programmer and debugger](https://www.st.com/en/development-tools/st-link-v2.html). As an alternative, the ST-Link programmer can be replaced by an [SEGGER J-Link OB](https://www.segger.com/products/debug-probes/j-link/models/other-j-links/st-link-on-board/). Finally, the STM32 can be programmed with any ARM Cortex JTAG or SWD programmer via the SWD jumper.

## Run Example Project using Ozone

When using an external J-Link programmer or after installing J-Link OB on the F4 Discovery board, you can flash and debug using the cross-platform [SEGGER Ozone Debugger](https://www.segger.com/products/development-tools/ozone-j-link-debugger/). It is included in some J-Link programmers or can be used for free for evaluation usage.

Just start Ozone and open the .jdebug file in the build folder. When compiled with "ENABLE_SEGGER_RTT", the debug output shows up in the Terminal window of Ozone. 


## Debug output

All debug output can be either send via SEGGER RTT or via USART2. To get the console from USART2, connect PA2 (USART2 TX) of the Discovery board to an USB-2-UART adapter and open a terminal at 115200.

In src/btstack_config.h resp. in example/btstack_config.h of the generated projects, additional debug information can be enabled by uncommenting ENABLE_LOG_INFO.

Also, the full packet log can be enabled in src/port.c resp. btstack/port/stm32-f4discovery-cc256x/src/port.c by uncommenting the hci_dump_init(..) line. The console output can then be converted into .pklg files for OS X PacketLogger or WireShark by running tool/create_packet_log.py

## GATT Database
In BTstack, the GATT Database is defined via the .gatt file in the example folder. The Makefile contains rules to update the .h file when the .gatt was modified.


## Maintainer Notes - Updating The Port

The Audio BSP is from the STM32F4Cube V1.16 firmware and not generated from STM32CubeMX. To update the HAL, run 'generate code' in CubeMX. After that, make sure to re-apply the patches to the UART and check if the hal config was changed.

