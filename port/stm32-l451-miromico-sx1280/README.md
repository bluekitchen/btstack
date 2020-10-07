# Experimental port for Semtech SX1280 2.4 GHz Multi Protocol Controller

## Overview

This port targets the Semtech SX1280 radio controller. The Host Stack and the Controller (incl. Link Layer) run on a STM32 MCU, with the SX1280 connected via SPI.

It uses the SX1280 C-Driver from Semtech to communicate with the SX1280. The main modification was to provide an optimized polling SPI driver that is faster then the one provided by STM32CubeMX.

## Status

Only tested with the Miromico FMLR-80-P-STL4E module so far. On this module, the 52 Mhz clock for the SX1280 is controlled by the MCU.

Segger RTT is used for debug output, so a Segger J-Link programmer is required.

Uses 32.768 kHz crytstal as LSE for timing

Support for Broadcaster and Peripheral roles.

The Makefile project compiles gatt_counter, gatt_streamer_server, hog_mouse and hog_keyboard examples.

## Limitation

### Advertising State:
- Only Connectable Advertising supported
- Only fixed public BD_ADDR of 33:33:33:33:33:33 is used
- Scan response not supported

### Connection State:
- Only a single packet is sent in each Connection Interval
- Encryption not implemented
- Some LL PDUs not supported

### Central Role:
- Not implemented

### Observer Role:
- Not implemented

### Low power mode - basically not implemented:
- MCU does not sleep
- SPI could be disabled during sleep
- 1 ms SysTick is used for host stack although 64-bit tick time is provided from LPTIM1

## Getting Started

For the FMLR-80-P-STL4E module, just run make. You can upload the EXAMPLE.elf file created in build folder,
e.g. with Ozone using the provided EXAMPLE.jdebug, and run it.

## TODO

### General
- multiple packets per connection interval
- indicate random address in advertising pdus
- allow to set random BD_ADDR via HCI command
- support other adv types
- support scan requests
- handle Encryption

## Low Power
- enter STANDY_RC mode when idle
- implement MCU sleep (if next wakeup isn't immediate)
- sleep after connection request until first packet from Central
- replace SysTick with tick counter based on LPTIM1
- disable SPI on MCU sleep

