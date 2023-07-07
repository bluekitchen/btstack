# BTstack Port for the Maxim MAX32665 EvKit-V1 

This port uses the [MAX32665/6 ARM Cortex M4F Board](https://www.analog.com/en/design-center/evaluation-hardware-and-software/evaluation-boards-kits/max32666evkit.html). All EvKits come with an external CMSIS-DAP programmer used to flash the boards.

## Software 

The [Analog Devices MSDK](https://github.com/Analog-Devices-MSDK/msdk) is a free development kit whch contains all the drivers neccessary for the board to operate.



## Toolchain Setup

Please follow the directions given in the [MSDK README.md](https://github.com/Analog-Devices-MSDK/msdk#readme)





## Build

Check that MAXIM_PATH points to the root directory where the tool chain installed.
Then, go to the port/max32665-evkit folder and run "make" command in terminal to generate example projects in the example folder.

In each example folder, e.g. port/max323630-fthr/example/spp_and_le_streamer, you can run "make" again to build an .elf file in the build folder which is convenient for debugging using Eclipse or GDB.



## Eclipse

Toolchain and Eclipse guide can be found in README.pdf file where the Maxim Toolchain is installed. Please note that this port was done using Makefiles.

## Flashing MAX32665 ARM Processor

There are two ways to program the board. The simplest way is drag and drop the generated .bin file to the DAPLINK mass storage drive. Once the file is copied to the mass storage device, the DAPLINK should program and then run the new firmware.

Alternatively, OpenOCD can be used to flash and debug the device. A suitable programming script can be found in the scripts folder.


## Usage

The project is designed to connect over HCI via the H4 transport (UART) to another MAX32 BLE capable device (MAX32665 is BLE capable). 

For the controller. Build and flash the BLE5_ctr project found in the MSDK t under [Examples/MAX32665/BLE5_ctr](https://github.com/Analog-Devices-MSDK/msdk/tree/main/Examples/MAX32665/BLE5_ctr) to another MAX32665 EV Kit or FTHR. For comvenience, the ELF file can also be found in this directory under BLE5_ctr_bin.

Once this code is flashed to a seconday board, flash an example to the primary EVKIT running the btstack host software. 

To connect the boards to each other, use a jumper wire to connect UART3 from one to another. UART3 is located on the EVKIT on JP9 and JP10. Once connected together and both boards are flashed, reset the two boards to start the example.
## Debugging

OpenOCD can also be used for developing and especially for debugging. Eclipse or GDB via OpenOCD could be used for step by step debugging.

## Debug output

printf messages are redirected to UART0. UART0 is accessible via the on board USB to serial converter. 

Additional debug information can be enabled by uncommenting ENABLE_LOG_INFO in the src/btstack_config.h header file and a clean rebuild.

Debug output is available both on the host and controller. 
