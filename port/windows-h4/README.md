# BTstack Port for Windows Systems with Bluetooth Controller connected via Serial Port

The Windows-H4 port uses the native run loop and allows to use Bluetooth Controllers connected via Serial Port.

Make sure to manually reset the Bluetooth Controller before starting any of the examples.

The port provides both a regular Makefile as well as a CMake build file. It uses native Win32 APIs for file access and does not require the Cygwin or mingw64 build/runtine. All examples can also be build with Visual Studio 2022 (e.g. Community Edition).

## Configuration

Most Bluetooth Controllers connected via UART/H4 require some special configuration, e.g. to set the UART baud rate and/or to upload firmware patches during startup. The Windows-H4 port tries to detect the controller type automatically based on the reported manufacturer information. Here are some controller specific details:

## TI CC256x

The CC256x needs the correct init script to start up. The build is configured to use `bluetooth_init_cc2564C_1.5.c` by default.

Please pick the correct one for your controller if needed. The application verifies that the selected init script matches the detected chipset, but the init script is linked into the executable.

## Broadcom/Cypress/Infineon Controllers

Broadcom/Cypress/Infineon controllers require a PatchRAM file. The CMake build downloads the BCM43430 PatchRAM used e.g. on later Raspberry Pi models.

When running an example, provide the matching `.hcd` file in the current working directory.

Newer Infineon AIROC controllers like the CYW55xx series require AIROC Download Mode for PatchRAM upload. To enable it, provide `--airoc-download-mode` on the command line, for example:

    gatt_counter.exe -u COM3 --airoc-download-mode

After opening the serial port, BTstack asks you to reset the controller and starts the firmware download after a short countdown.

## Nordic Controller with HCI UART firmware

The main difference of Zephyr-based Nordic Bluetooth Controllers is that:
- the nRF52- and nRF54-Series use 1000000 as baud rate instead of 115200
- they don't have a public BD_ADDR

You can use these with the regular `windows-h4` port by setting the baudrate to `1000000`, e.g. like this

    gatt_counter.exe -u COM3 --baud 1000000

The fixed random static address is used automatically.

## Visual Studio 2022

Visual Studio can directly open the provided `port/windows-windows-h4/CMakeLists.txt` and allows to compile and run all examples.

## Visual Studio Code

The repository contains a shared VS Code workspace file for this port:

    port/windows-h4/btstack-windows-h4.code-workspace

Open this workspace file in Visual Studio Code. It shows the full BTstack repository
in the file explorer, but configures CMake to use `port/windows-h4/CMakeLists.txt`
and places the build directory in `port/windows-h4/build`.

### Setup

- Install the VS Code extensions `CMake Tools` and `C/C++`
- Install a C/C++ toolchain on Windows, e.g. Visual Studio Community or Build Tools
  with the `Desktop development with C++` workload
- Make sure Python is installed, as some generated headers are created during the build
- Open `port/windows-h4/btstack-windows-h4.code-workspace`

### Build an example

1. Open the Command Palette with `Ctrl+Shift+P`
2. Run `CMake: Scan for Kits`
3. Run `CMake: Select a Kit`
4. Pick your Windows toolchain, e.g. `Visual Studio ... - amd64`
5. Run `CMake: Configure`
6. Run `CMake: Select Build Target`
7. Pick an example, e.g. `gatt_counter`
8. Run `CMake: Build`

The executable will be created in:

    port/windows-h4/build/

### Run an example

Open a terminal in Visual Studio Code, change to the build directory, and start the
selected example, for example:

    > cd port\windows-h4\build
    > .\gatt_counter.exe

## mingw64 

It can also be compiles with a regular Unix-style toolchain like [mingw-w64](https://www.mingw-w64.org).
mingw64-w64 is based on [MinGW](https://en.wikipedia.org/wiki/MinGW), which '...provides a complete Open Source programming tool set which is suitable for the development of native MS-Windows applications, and which do not depend on any 3rd-party C-Runtime DLLs.'

We've used the Msys2 package available from the [downloads page](https://www.mingw-w64.org/downloads/) on Windows 10, 64-bit and use the MSYS2 MinGW 64-bit start menu item to compile 64-bit binaries.

In the MSYS2 shell, you can install everything with pacman:

    $ pacman -S git
    $ pacman -S cmake
    $ pacman -S make
    $ pacman -S mingw-w64-x86_64-toolchain
    $ pacman -S mingw-w64-x86_64-portaudio
    $ pacman -S python
    $ pacman -S winpty

### Compilation with CMake

With mingw64-w64 installed, just go to the port/windows-h4 directory and use CMake as usual

    $ cd port/windows-h4
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make

Note: When compiling with msys2-32 bit and/or the 32-bit toolchain, compilation fails
as `conio.h` seems to be mission. Please use msys2-64 bit with the 64-bit toolchain for now.

## Console Output

When running the examples in the MSYS2 shell, the console input (via btstack_stdin_support) doesn't work. It works in the older MSYS and also the regular CMD.exe environment. Another option is to install WinPTY and then start the example via WinPTY like this:

    $ winpty ./gatt_counter.exe

The packet log will be written to hci_dump.pklg
