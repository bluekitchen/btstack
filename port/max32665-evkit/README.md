# BTstack Port for the Maxim MAX32665 EvKit-V1 

This port uses the [MAX32665/6 ARM Cortex M4F Board](https://www.analog.com/en/design-center/evaluation-hardware-and-software/evaluation-boards-kits/max32666evkit.html). All EvKits come with an external CMSIS-DAP programmer used to flash the boards.

## Software 

The [Analog Devices MSDK](https://github.com/Analog-Devices-MSDK/msdk) is a free development kit that includes all the necessary drivers for the board's operation.


## Toolchain Setup

Please follow the directions given in the [MSDK README.md](https://github.com/Analog-Devices-MSDK/msdk#readme)





## Build

Ensure that the `MAXIM_PATH` points to the root directory where MSDK installed.
Then, navigate to the port/max32665-evkit folder and run the make command in the terminal to generate example projects in the example folder.

In each example folder, e.g. port/max323630-fthr/example/spp_and_le_streamer, you can run make again to build an .elf file. The build folder will contain this file, making it conveniently accessible for debugging with Eclipse or GDB.


## Flashing MAX32665
There are two methods to program the board. The easiest is to drag and drop the generated .bin file to the DAPLINK mass storage drive. Once the file is copied, the DAPLINK should program and then run the new firmware.

Alternatively, OpenOCD can be used to flash and debug the device. A suitable programming script is available in the scripts folder.

## Usage

The project is designed to connect over HCI via the H4 transport (UART) to another MAX32 BLE capable device (MAX32665 is BLE capable).

For the controller, build and flash the BLE5_ctr project found in the MSDK under [Examples/MAX32665/BLE5_ctr](https://github.com/Analog-Devices-MSDK/msdk/tree/main/Examples/MAX32665/BLE5_ctr) to another MAX32665 EV Kit or FTHR. For comvenience, the ELF file can also be found in this directory under BLE5_ctr_bin.

After this code is flashed onto a secondary board, flash an example onto the primary EVKIT running the BTstack host software.

To connect the boards, use a jumper wire to link UART3 from one board to the other. UART3 can be found on the EVKIT at JP9 and JP10. After the connection and flashing are complete, reset both boards to start the example.

The HCI uart can be found and modified in the MSDK under Libraries/Boards/MAX32665/EvKit_V1/Include/board.h
## Debugging

OpenOCD can be used for both development and debugging. Step-by-step debugging can be achieved with either Eclipse or GDB via OpenOCD.

## Debug output

printf messages are redirected to UART0, which can be accessed via the onboard USB to serial converter.

 

Additional debugging information can be enabled by uncommenting ENABLE_LOG_INFO in the src/btstack_config.h header file and performing a clean rebuild.

 

Debug output is available on both the host and controller.