# BTstack Port for the Maxim MAX32630FTHR ARM Cortex-M4F

This port uses the [MAX32630FTHR ARM Cortex M4F Board](https://www.maximintegrated.com/en/products/microcontrollers/MAX32630FTHR.html) with the onboard TI CC2564B Bluetooth controller. It usually comes with the [DAPLINK Programming Adapter](https://developer.mbed.org/teams/MaximIntegrated/wiki/MAXREFDES100HDK). 
The DAPLINK allows to upload firmware via a virtual mass storage device (like mbed), provides a virtual COM port for a console, and enables debugging via the SWD interface via OpenOCD.

The port uses non-blocking polling UART communication with hardware flow control for Bluetooth controller. It was tested and achieved up to 1.8 Mbps bandwidth between two Max32630FTHR boards.

## Software 

The [Maxim ARM Toolchain](https://www.maximintegrated.com/en/products/microcontrollers/MAX32630.html/tb_tab2) is free software that provides peripheral libraries, linker files, initial code and some board files. It also provides Eclipse Neon and Maxim modified OpenOCD to program the microcontroller together with various examples for Maxim Cortex M4F ARM processors.

For debugging, OpenOCD can be used. The regular OpenOCD does not support Maxim ARM microcontrollers yet, but a modified OpenOCD for use with Maxim devices can be found in the Maxim ARM Toolchain.

## Toolchain Setup

In the Maxim Toolchain installation directory, there is a setenv.sh file that sets the MAXIM_PATH. MAXIM_PATH needs to point to the root directory where the tool chain installed. If you're lucky and have a compatible ARM GCC Toolchain in your PATH, it might work without calling setenv.sh script.

## Usage

The examples can be compiled using GNU ARM Toolchain. A firmware binary can be flashed either by copying the .bin file to the DAPLINK mass storage drive, or by using OpenOCD on the command line, or from Eclipse CDT.

## Build

Checkt that MAXIM_PATH points to the root directory where the tool chain installed.
Then, go to the port/max32630-fthr folder and run "make" command in terminal to generate example projects in the example folder.

In each example folder, e.g. port/max323630-fthr/example/spp_and_le_streamer, you can run "make" again to build an .elf file in the build folder which is convenient for debugging using Eclipse or GDB.

For flashing via the virtual USB drive, the "make release" command will generate .bin file in the build folder.

## Eclipse

Toolchain and Eclipse guide can be found in README.pdf file where the Maxim Toolchain is installed. Please note that this port was done using Makefiles.

## Flashing Max32630 ARM Processor

There are two ways to program the board. The simplest way is drag and drop the generated .bin file to the DAPLINK mass storage drive. Once the file is copied to the mass storage device, the DAPLINK should program and then run the new firmware.

Alternatively, OpenOCD can be used to flash and debug the device. A suitable programming script can be found in the scripts folder.

## Debugging

OpenOCD can also be used for developing and especially for debugging. Eclipse or GDB via OpenOCD could be used for step by step debugging.

## Debug output

printf messages are redirected to UART2. UART2 is accessible via the DAPLINK Programming Adapter as a virtual COM port at 115200 baud with no flow control. If this doesn't work for you, you can connect P3_1 (UART TX) of the MAX32630FTHR board to a USB-to-UART adapter.

Additional debug information can be enabled by uncommenting ENABLE_LOG_INFO in the src/btstack_config.h header file and a clean rebuild.

## TODOs
  - Support for BTSTACK_STDIN
  - Add flash-openocd to Makefile template
  - Add Eclipse CDT projects for max32630fthr
  - Implement hal_led.h to control LED on board
 
