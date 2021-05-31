# BTstack Port for MSP432P401 Launchpad with CC256x

This port is for the the TI MSP432P401R Launchpad with TI's CC256x Bluetooth Controller using TI's DriverLib (without RTOS). 
For easy development, Ozone project files are generated as well.

As the MSP432P401 does not have support for hardware RTS/CTS, this port makes use of Ping Pong DMA transfer mode
(similar to circular DMA on other MCUs) to use two adjacent receive buffers and raise RTS until a completed buffer is processed. 

## Hardware

[TI MSP432P401R LaunchPad](https://www.ti.com/tool/MSP-EXP432P401R)

As Bluetooth Controller, there are two BoosterPacks that can be use:
1. [BOOST-CC2564MODA CC2564B BoosterPack](https://www.ti.com/tool/BOOST-CC2564MODA) (USD 20)
2. [Evaluation Module (EM) Adapto](https://www.ti.com/tool/TIDM-LPBP-EMADAPTER) (USD 20) with one of the CC256x modules:
    - [CC2564B Dual-mode Bluetooth® Controller Evaluation Module](https://store.ti.com/cc2564modnem.aspx) (USD 20)
    - [CC2564C Dual-mode Bluetooth® Controller Evaluation Module](https://store.ti.com/CC256XCQFN-EM-CC2564C-Dual-Mode-Bluetooth-Controller-Evaluation-Module-P51277.aspx) (USD 60)

The CC2564B Booster pack is around USD 20 while thhe EM Adapter with the CC2564C module is around USD 80.

The project in the BTstack repo `port/msp432p401lp-cc256x' is configured for the EM Adapter + newer CC2564C module.

When using the CC2564B (either as BOOST-CC2564MODA or CC2564B Dual-mode Bluetooth® Controller Evaluation Module), the *bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c* must be used as cc256x_init_script. See Makefile variable INIT_SCRIPT.

When using the CC2564B Booster Pack, please use uncomment the defines for the GPIO definition (search for `BOOST-CC2564MODA`)

When using the EM Adapter Booster Pack, please make sure to solder a 32.768 kHz quarz oscillator as explained in 4.7 of the [EM Wireless Booster Pack User Guide](http://www.ti.com/lit/ug/swru338a/swru338a.pdf). If you don't have an oscillator of that size, you might solder one upside done (turtle-on-back style) to the unused upper right pad and wire GCC, VCC, and clock with thin wires.


## Software

To build all examples, you need the regular ARM GCC toolcahin installed. Run make

	$ make

All examples and the .jdebug Ozone project files are placed in the 'gcc' folder.


## Flash And Run The Examples

The Makefile builds different versions: 
- example.elf: .elf file with all debug information
- example.bin: .bin file that can be used for flashing

There are different options to flash and debug the MSP432P401R LaunchPad. If all but the jumpers for power (the left three) are removed on J101, an external JTAG like SEGGER's J-Link can be connected via J8 'MSP432 IN'.

## Run Example Project using Ozone

When using an external J-Link programmer, you can flash and debug using the cross-platform [SEGGER Ozone Debugger](https://www.segger.com/products/development-tools/ozone-j-link-debugger/). It is included in some J-Link programmers or can be used for free for evaluation usage.

Just start Ozone and open the .jdebug file in the build folder. When compiled with `ENABLE_SEGGER_RTT`, the debug output shows up in the Terminal window of Ozone. 


## Debug output

All debug output is send via SEGGER RTT or via USART2. To get the console from USART2, remove `ENABLE_SEGGER_RTT` from btstack_config.h and open a terminal to the virtual serial port of the Launchpad at 115200.

In btstack_config.h resp. in example/btstack_config.h of the generated projects, additional debug information can be disabled/enabled via ENABLE_LOG_INFO.

Also, the full packet log can be enabled in main.c  by uncommenting the hci_dump_init(..) line. The output can then be converted into .pklg files for OS X PacketLogger or WireShark by running tool/create_packet_log.py


## GATT Database
In BTstack, the GATT Database is defined via the .gatt file in the example folder. The Makefile contains rules to update the .h file when the .gatt was modified.
