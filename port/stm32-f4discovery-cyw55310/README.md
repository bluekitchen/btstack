# BTstack Port for STM32 F4 Discovery Board with Ezurio Vela IF310 Devkit

This port uses the STM32 F4 Discovery Board with Ezurio's Vela IF310 Development Kit based on the Infineon AIROC CYW55310.

STCubeMX was used to provide the HAL, initialize the device, and create an initial CMakeLists.txt for CMake. The CMake project can be built from the command line as desrbied below or in modern IDEs, e.g. JetBrains' CLion or Microsoft's Visual Studio Code with the STM32CubeIDE Extension. In addition, SEGGER Ozone project files are generated for debugging.


## Hardware

- [STM32 F4 Discovery Board](http://www.st.com/en/evaluation-tools/stm32f4discovery.html)

- [Vela IF310 - Bluetooth® Classic and LE Audio Development Kit](https://www.ezurio.com/wireless-modules/bluetooth-modules/vela-if310-bluetooth-6-classic-and-le-module)

- 10-pin Female-to-Female Jumper Wires

- USB-to-UART adapter for the console output, if you're not using SEGGER RTT.

### Setup

- Make sure the mini dip switches on the IF310 development kit are set as below. Next to each each switch is a signal multiplexer IC with an easy to read/find silkscreen symbol.

| Switch                 | IC   | Position |
|------------------------|------|----------|
| FTDI 3V3 Level Shifter | U8   | OFF / EN |
| FTDI vs RP2040 Switch  | U9   | ON / L   |
| Ext. TDM vs. SMARC     | U6   | OFF / H  |
| TDM1 vs. RP2040        | U20  | OFF / H  |

- Connect F4 Discovery board with the IF310 Development Kit using Female-to-Female Dupont Wires.

| STM32            | PIN  | Vela IF310 DK   | CYW55310    | Comment    | Color  |
|------------------|------|-----------------|-------------|------------|--------|
| GND              | GND  | BT_REG_ON_3V3-GND | GND         | Right pin  | Gray   |
| VDD              | VDD  | BT_REG_ON_3V3-3V3 | VDD         | Left pin   | Purple |
| USART2_TX        | PA2  |                 | N.C.        | Console TX |        |
| USART2_RX        | PA3  |                 | N.C         | Console RX |        |
| Bluetooth Enable | PE14 | BT_REG_ON_3V3-REG_ON | BT_REG_ON   | Middle pin | Yellow |
| USART3_TX        | PD8  | FTDI_3.3-TX     | BT_UART_RX  |            | Black  |
| USART3_RX        | PD9  | FTDI_3.3-RX     | BT_UART_TX  |            | Brown  |
| USART3_CTS       | PD11 | FTDI_3.3-CTS    | BT_UART_RTS |            | Red    | 
| USART3_RTS       | PD12 | FTDI_3.3-RTS    | BT_UART_CTS |            | Orange |

- Connect the F4 Discovery board to your desktop via the USB Mini connector.
- If you don't use SEGGER RTT for the console, connect USART2 of the STM32 via pins PA2 & PA3 to an USB-to-UART adapter that is connected to your desktop.


## Development on the Command Line


### Build

The build system uses CMake with the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)

To install dependencies:
- macOS
    - `brew install cmake gcc-arm-embedded ninja`
- Debian/Ubuntu
    - `sudo apt-get install gcc-arm-none-eabi binutils-arm-none-eabi gdb-arm-none-eabi openocd`
- Windows
    - `winget install Kitware.CMake Ninja-build.Ninja python3 Git.Git Arm.GnuArmEmbeddedToolchain`

To build all examples with `Ninja`, run `cmake` in the port folder:
 
    $ cd port
    $ cd stm32-f4discovery-cyw55310   
    $ cmake -G Ninja -B build
    $ cmake --build build

All examples are built in Release mode and placed together with Ozone project files (.jdebug) into the `build` folder.
    
If you want to build only a single example, you can pass the name of the example at the end after a double-dash, e.g.
    
    $ cmake --build build -- gatt_counter


### Flash using various tools

There are different options to flash and debug the F4 Discovery board. The F4 Discovery boards comes with an on-board [ST-Link programmer and debugger](https://www.st.com/en/development-tools/st-link-v2.html).

Please note that the ST-Link/V2 programmer on the F4 Discovery board does not provide a Virtual COM Port like newer versions of SEGGER's J-Link OB. 
In addition, there are currently no drivers for AMR64 (December 2025). 

Popular options are: 
- [OpenOCD](https://openocd.org) - command line
- [stlink](https://github.com/stlink-org/stlink) - command line
- [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html) - cross-platform GUI

As an alternative, the ST-Link programmer can be replaced by an [SEGGER J-Link OB](https://www.segger.com/products/debug-probes/j-link/models/other-j-links/st-link-on-board/). 

Finally, the STM32 can be programmed with any ARM Cortex JTAG or SWD programmer via the SWD header.



### Flash and Debug with Ozone

When using an external J-Link programmer or after installing J-Link OB on the F4 Discovery board, you can flash and debug using the cross-platform [SEGGER Ozone Debugger](https://www.segger.com/products/development-tools/ozone-j-link-debugger/). It is included in some J-Link programmers or can be used for free for evaluation usage.

Just start Ozone and open the `EXAMPLE.jdebug` file in the build folder for the example you want to run. 

When compiled with "ENABLE_SEGGER_RTT" enabled in `port/btstack_config.h`, the debug console shows up in the Terminal window of Ozone. You might need to enable "Capture RTT" via the Terminal window context menu. 


## Configuration

### Debug Console

All debug console can be either send via SEGGER RTT or via USART2. 
To get the console from USART2, connect PA2 (USART2 TX) of the Discovery board to an USB-2-UART adapter and open a terminal at 115200.

In `port/btstack_config.h` additional debug information can be enabled by uncommenting `ENABLE_LOG_INFO.`
Also, the full packet log can be enabled in `port/btstack_config.h` by uncommenting `ENABLE_HCI_DUMP`.

The console output can then be converted into `.pklg` files for OS X PacketLogger or WireShark by running `tool/create_packet_log.py`.


### PatchRAM

The port downloads the latest PatchRAM for the Vela IF310 / CYW55310 from the BlueKitchen Web server.


### GATT Database

In BTstack, the GATT Database is defined via the .gatt file in the example folder. 
The CMakeLists.txt contains rules to update the .h file when the .gatt was modified.
