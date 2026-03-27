# BTstack Port for POSIX Systems with Zephyr-based Controller

The main difference to the regular posix-h4 port is that that the Zephyr Controller uses 1000000 as baud rate.
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
always start the examples with the `-u /path/to/serialport` option.

## Compile Examples

    $ make

## Run example

Just run any of the created binaries, e.g.

    $ ./le_counter

The packet log will be written to /tmp/hci_dump.pklg

