# BTstack Port for Windows Systems with Zephyr-based Controller

The main difference to the regular posix-h4 port is that that the Zephyr Contoller uses 1000000 as baud rate.
In addition, the port defaults to use the fixed static address stored during production.

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

## Toolchain

The port requires a Unix-like toolchain. We successfully used [mingw-w64](https://mingw-w64.org/doku.php) to compile and run the examples. mingw64-w64 is based on [MinGW](https://en.wikipedia.org/wiki/MinGW), which '...provides a complete Open Source programming tool set which is suitable for the development of native MS-Windows applications, and which do not depend on any 3rd-party C-Runtime DLLs.'

We've used the Msys2 package available from the [downloads page](https://mingw-w64.org/doku.php/download) on Windows 10, 64-bit and use the MSYS2 MinGW 64-bit start menu item to compile 64-bit binaries.

In the MSYS2 shell, you can install everything with pacman:

    $ pacman -S git
    $ pacman -S make
    $ pacman -S mingw-w64-x86_64-toolchain
    $ pacman -S python
    $ pacman -S winpty

## Compile Examples

    $ make

Note: When compiling with msys2-32 bit and/or the 32-bit toolchain, compilation fails
as `conio.h` seems to be mission. Please use msys2-64 bit with the 64-bit toolchain for now.

## Run example

Just run any of the created binaries, e.g.

    $ ./le_counter

The packet log will be written to /tmp/hci_dump.pklg

