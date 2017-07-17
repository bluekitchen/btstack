# BTstack Port for the Maxim MAX32630FTHR ARM Cortex-M4F

This port uses the MAX32630FTHR ARM Cortex M4F Board onboard TI's CC2564B Bluetooth controller.

Maxim ARM Toolchain is free software that provides peripheral libraries, linker files, initial code and some board files.
The toolchain also provides Eclipse Neon and Maxim modified OpenOCD to program the microcontroller.
It also provides various examples for Maxim Cortex M4F ARM processors. Maxim released OpenOCD was used to flash and debug the max32630fthr port.

The port uses non-blocking polling UART communication with hardware flow control for Bluetooth controller. It was tested and achieved up to 1.8Mbps bandwidth between two Max32630FTHR boards.

## Hardware

- [MAX32630FTHR Board] (https://www.maximintegrated.com/en/products/digital/microcontrollers/MAX32630FTHR.html)

## Software

You need to install Maxim ARM Toolchain. The Libraries and the toolchain can be downloaded from [Maxim Toolchains] (https://www.maximintegrated.com/en/products/digital/microcontrollers/MAX32630.html/tb_tab2) page. Regular openocd does not support Maxim ARM microcontrollers yet. Maxim modified OpenOCD can be found in the toolchain.

In the Maxim Toolchain installation directory, there is a setenv.sh file that sets the MAXIM_PATH. This file needs to run at the beginning and set the enviromental variables.

## Usage

The example can be compiled using GNU ARM Toolchain and the firmware binary can be flashed using OpenOCD or eclipse.

## Build

Go to port/max32630fthr folder and run "make" command in terminal. You can also open the project using Eclipse and build it.
MAXIM_PATH needs to point to the root directory where the tool chain installed. Or makefile could be modified to point to the libraries.

"make" command will build and generate .elf file which is convenient for debugging using Eclipse or GDB.
$ make

"make release" command will generate .bin file that can be used to program using Daplink adapter.

## Eclipse

Toolchain and Eclipse guide can be found in README.pdf file where the Maxim Toolchain installed.
Please note that this development was not made with eclipse.

## Flashing Max32630 ARM Processor

There are two ways to program the board. The simplest way is drag and drop the max3263x.bin file to DAPLINK mass storage drive. Once the file is copied to the mass storage device, it should program and then run the new firmware.

OpenOcd also can be used for developing and especially for debugging. Eclipse or GDB could be used for step by step debugging.

The programming script can be found in the scripts folder.
To flash the microcontroller, 'flash_Max32630_hdk.sh' script needs to be run in the max32630fthr directory.


## Debug output

printf messages are redirected to UART2.
To get the console output, connect P3_1 (UART TX) of the MAX32630FTHR board to a USB-to-UART adapter and open a terminal at 115200 baudrate with no hardware/software flow control.

In src/btstack_config.h resp. in example/btstack_config.h of the generated projects, additional debug information can be enabled by uncommenting ENABLE_LOG_INFO.


## Modifications to the GATT Database

In BTstack, the GATT Database is defined via the .gatt file in the example folder. Before it can be used, run the following command in max32630fthr directory.

$ make generate_header_file

or

$ python ../../tool/compile_gatt.py spp_and_le_streamer.gatt spp_and_le_streamer.h


## Notes

 - bluetooth_init_cc2564B_1.6_BT_Spec_4.1.c file added as default. If "include ${BTSTACK_ROOT}/chipset/cc256x/Makefile.inc" is enabled in the Makefile, it will automatically download the bts files and convert it to a c file.

## TODOs
  - Add Eclipse project for max32630fthr
  - link to sources instead of copying into every example project
  - support link-key-db for on flash memory
