# BTstack Port for the Maxim MAX32630FTHR ARM Cortex-M4F

This port uses the [MAX32630FTHR ARM Cortex M4F Board](https://www.maximintegrated.com/en/products/microcontrollers/MAX32630FTHR.html) with the onboard TI CC2564B Bluetooth controller. It usually comes with the [DAPLINK Programming Adapter](https://developer.mbed.org/teams/MaximIntegrated/wiki/MAXREFDES100HDK). 
The DAPLINK allows to upload firmware via a virtual mass storage device (like mbed), provides a virtual COM port for a console, and enables debugging via the SWD interface via OpenOCD.

The port uses non-blocking polling UART communication with hardware flow control for Bluetooth controller. It was tested and achieved up to 1.8 Mbps bandwidth between two Max32630FTHR boards.

## Prerequisites

### Maxim SDK

The Maxim has dropped the support for MAX32630 and is not recommended for new designs and it is also dropped from the latest SDK.

#### Maxim SDK Mirror

The required minimum set of files from the SDK are included in the port files [port/max32630-fthr/maxim](https://github.com/bluekitchen/btstack/tree/develop/port/max32630-fthr/maxim) for convenience and to support linux build.

>#### (For reference) ARM Cortex Toolchain - a.k.a. the old Maxim SDK
>The [ARM Cortex Toolchain](https://download.analog.com/sds/exclusive/SFW0001500A/ARMCortexToolchain.exe) is free software that provides peripheral libraries, linker files, initial code and some board files. It also provides Eclipse Neon and Maxim modified OpenOCD to program the microcontroller together with various examples for Maxim Cortex M4F ARM processors.

>For debugging, OpenOCD can be used. The regular OpenOCD does not support Maxim ARM microcontrollers yet, but a modified OpenOCD for use with Maxim devices can be found in the Maxim ARM Toolchain.

>Toolchain and Eclipse guide can be found in README.pdf file where the Maxim Toolchain is installed. Please note that this port was done using Makefiles.

### ARM Toolchain Setup

Download and extract arm toolchain. Record extract folder as `TOOLCHAIN_PATH`.

Windows msys:
[arm-gnu-toolchain-14.2.rel1-mingw-w64-x86_64-arm-none-eabi.zip](https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-mingw-w64-x86_64-arm-none-eabi.zip)

WSL Linux:
[arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz](https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz)

### Python - 3.12 or newer

Download and install python and make sure the alias `python` is accessible in the build terminal.

### MSYS2 setup - (windows only)
The [ARM Cortex Toolchain](https://download.analog.com/sds/exclusive/SFW0001500A/ARMCortexToolchain.exe) contains msys1.0, but it is complete (lacking unzip) and it is recommended that to use a more recent msys2.

Download and install msys2 - https://github.com/msys2/msys2-installer/releases/
- Install with option to Run msys2 after completion.
- Record install folder as `MSYS_PATH`
- Install required packages make and unzip.
  `pacman -S make unzip`
- close msys2

### Environment Variables

The `BTSTACK_ROOT`, `MAXIM_PATH`, and `PATH` must point to the previously setup tools as absolute paths.

The `BTSTACK_ROOT` is provided as relative path for examples in `port/max32630-fthr/example/project/Makefile`, but must be provided otherwise.

If not provided the `MAXIM_PATH` is set based on `BTSTACK_ROOT`.

Windows msys:

Run scripts/setenv.bat or set manually
```batch
# TOOLCHAIN_PATH=%EXTRACT_PATH%
# MSYS_BIN=%MSYS_PATH%\usr\bin
set PATH=%TOOLCHAIN_PATH%/bin;%MSYS_BIN%;%PATH%
```

WSL Linux:
```bash
PATH="${TOOLCHAIN_PATH}/bin:${PATH}"
```

## Build

The examples can be compiled using `make`. The build is implemented as a combination of several make and python scripts. For details see the scripts `port/max32630-fthr`, `port/max323630-fthr/scripts`, and `port/max32630-fthr/example/template` folders.

Running the make script `port/max32630-fthr/Makefile` will create Makefiles for the projects. And the projects are built running make scripts `port/max32630-fthr/example/Makefile` or `port/max32630-fthr/example/project/Makefile`.

```bash
cd %BTSTACK_ROOT%/port/max32630-fthr
.../port/max32630-fthr$ make
> Creating example projects:
> project - a2dp_sink_demo
> project - a2dp_source_demo
> project - ancs_client_demo
> ...
.../port/max32630-fthr$ cd example
# build a make target default(all), dual, general, classic, or ble
.../port/max32630-fthr/example$ make
# or build individual project e.g. gatt_counter
.../port/max32630-fthr/example$ cd gatt_counter
.../port/max32630-fthr/example/gatt_counter$ make
```

In each example/project folder the make target is to build .elf file in the build folder which is convenient for debugging using Eclipse or GDB. 

For flashing via the virtual USB drive, the make also builds .bin file relative to the project ../bin/project.bin e.g. which is for the examples `example/bin`.

## Flashing Max32630 ARM Processor

A firmware binary can be flashed either by copying the .bin file to the DAPLINK mass storage drive, or by using OpenOCD on the command line, or from Eclipse CDT.

The simplest way is drag and drop the generated .bin file to the DAPLINK mass storage drive. Once the file is copied to the mass storage device, the DAPLINK should program and then run the new firmware.

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
 
