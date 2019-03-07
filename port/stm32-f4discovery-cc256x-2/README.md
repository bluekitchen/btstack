# BTstack port for STM32 F4 Discovery Board with CC256x

This port uses the STM32 F4 Discovery Board with TI's CC256XEM ST Adapter Kit that allows to plug in a CC256xB or CC256xC Bluetooth module.
STCubeMX was used to provide the HAL and initialize the device. With an extern Segger J-Link or by updating the built-in ST-Link to J-Link OB, SEGGER RTT can be used instead of an UART console.

## Hardware

STM32 Development kit and adapter for CC256x module:
- [STM32 F4 Discovery Board](http://www.st.com/en/evaluation-tools/stm32f4discovery.html)
- [CC256xEM Bluetooth Adatper Kit for ST](https://store.ti.com/CC256XEM-STADAPT-CC256xEM-Bluetooth-Adapter-Kit-P45158.aspx)

CC256x Bluetooth module:
- [CC2564B Dual-mode Bluetooth® Controller Evaluation Module](https://store.ti.com/cc2564modnem.aspx)
- [CC2564C Dual-mode Bluetooth® Controller Evaluation Module](https://store.ti.com/CC256XCQFN-EM-CC2564C-Dual-Mode-Bluetooth-Controller-Evaluation-Module-P51277.aspx)

The module with the older CC2564B is around USD 20, while the one with the new CC2564C costs around USD 60. The projects are configured for the CC2564B. When using the CC2564C, *bluetooth_init_cc2564C_1.0.c* should be use as cc256x_init_script, see ./create_examples.py.

## Software

..

## Run Example Project

..

## Debug output

printf is routed to USART2. To get the console output, connect PA2 (USART2 TX) of the Discovery board to an USB-2-UART adapter and open a terminal at 115200.

In src/btstack_config.h resp. in example/btstack_config.h of the generated projects, additional debug information can be enabled by uncommenting ENABLE_LOG_INFO.

Also, the full packet log can be enabled in src/port.c resp. btstack/port/stm32-f4discovery-cc256x/src/port.c by uncommenting the hci_dump_open(..) line. The console output can then be converted into .pklg files for OS X PacketLogger or WireShark by running tool/create_packet_log.py
