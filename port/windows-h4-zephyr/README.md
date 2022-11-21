# BTstack Port for Windows Systems with Zephyr-based Controller

The main difference to the regular windows-h4 port is that that the Zephyr Contoller uses 1000000 as baud rate.
In addition, the port defaults to use the fixed static address stored during production.

The port provides both a regular Makefile as well as a CMake build file. It uses native Win32 APIs for file access and does not require the Cygwin or mingw64 build/runtine. All examples can also be build with Visual Studio 2022 (e.g. Community Edition).

## Prepare Zephyr Controller

Please follow [this](https://devzone.nordicsemi.com/blogs/1059/nrf5x-support-within-the-zephyr-project-rtos/) blog post about how to compile and flash `samples/bluetooth/hci_uart` to a connected nRF5 dev kit.

In short: you need to install an arm-none-eabi gcc toolchain and the nRF5x Command Line Tools incl. the J-Link drivers, checkout the Zephyr project, and flash an example project onto the chipset:

  * Install [J-Link Software and documentation pack](https://www.segger.com/jlink-software.html).
  * Get nrfjprog as part of the [nRFx-Command-Line-Tools](http://www.nordicsemi.com/eng/Products/Bluetooth-low-energy/nRF52-DK). Click on Downloads tab on the top and look for your OS.
  * [Checkout Zephyr and install toolchain](https://www.zephyrproject.org/doc/getting_started/getting_started.html). We recommend using the [arm-non-eabi gcc binaries](https://launchpad.net/gcc-arm-embedded) instead of compiling it yourself. At least on OS X, this failed for us.

  * In *samples/bluetooth/hci_uart*, compile the firmware for nRF52 Dev Kit

      $ make BOARD=nrf52_pca10040

   * Upload the firmware

      $ make flash

   * For the nRF51 Dev Kit, use `make BOARD=nrf51_pca10028`.

## Configure serial port

To set the serial port of your Zephyr Controller, you can either update config.device_name in main.c or
always start the examples with the correct `-u COMx` option.

## Visual Studio 2022

Visual Studio can directly open the provided `port/windows-windows-h4-zephyr/CMakeLists.txt` and allows to compile and run all examples.

## mingw64 

It can also be compiles with a regular Unix-style toolchain like [mingw-w64](https://www.mingw-w64.org).
mingw64-w64 is based on [MinGW](https://en.wikipedia.org/wiki/MinGW), which '...provides a complete Open Source programming tool set which is suitable for the development of native MS-Windows applications, and which do not depend on any 3rd-party C-Runtime DLLs.'

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
