# BTstack Port for Zephyr RTOS running on Nordic nRF5 Series

## Overview

This port targets the bare Nordic nRF5-Series chipsets with the BLE Link Layer provided by the Zephyr project.

## Status

Working with nRF52 pca10040 dev board. Public BD ADDR is set to 11:22:33:44:55:66 since the default 00:00:00:00:00:00 is filtered by iOS.

## Getting Started

To integrate BTstack into Zephyr, please move the BTstack project into the Zephyr root folder 'zephyr'. Please use the Zephry '1.9-branch' for now. In the master branch, Zephyr switched the build system to CMake and this port hasn't been update for that yet.

Then integrate BTstack:

	cd /path/to/zephy/btstack/port/nrf5-zephyr
	./integrate_btstack.sh

Now, the BTstack examples can be build from the Zephyr examples folder in the same way as other examples, e.g.:

	cd /path/to/zephyr/samples/btstack/le_counter
	make

to build the le_counter example for the pca10040 dev kit using the ARM GCC compiler.

You can use `make flash` or `./flash_nrf52_pca10040.sh` to download it onto the board.

All examples that provide a GATT Server use the GATT DB in the .gatt file. Therefore you need to run ./update_gatt_db.sh in the example folder after modifying the .gatt file.

This port does not support Data Sources aside from the HCI Controller.

## TODO
- printf is configured by patching `drivers/serial/uart_nrf5.c' to use 115200 (default: 100000). There should be a better way to set baud rate.
- enable/configure DLE for max packet size for LE Streamer

