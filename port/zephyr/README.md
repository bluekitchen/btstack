# BTstack Port for Zephyr RTOS

## Overview

This port targets any platform supported by Zephyr that either contains a built-in Bluetooth Controller
or is connected to an external Controller via one of the supported Zephyr HCI Transports drivers (see `zephyr/drivers/bluetooth/hci`)

## Status

Tested with nRF52 DK (PCA10040), nRF52840 DK (PC10056) and nRF5340 DK (PCA10095) boards only. It uses the fixed static random BD ADDR stored in NRF_FICR/NRF_FICR_S, which will not compile on non nRF SoCs.

## Build Environment
The first step needs to done once. Step two is needed every time to setup the environment.

### 1. Build Environment Preconditions

Follow the getting started [guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
until you are able to build an example.

Then update the `ZEPHYR_ROOT` variable in `env.sh` to point to your `zephyrproject`. Defaults to `~/zephyrproject`


### 2. Prepare the build environment

To setup your environment to build a BTstack example, run the provided setup in `env.sh`.

```sh
source env.sh
```

## Building and Running on nRF52840

### 1. Build Example

You can build an example using:
```sh
west build -b nrf52840dk/nrf52840
```

`nrf52840dk/nrf52840` selected the Nordic nRF52840 DK. For the older nRF52 DK with nRF52832, you can specify nrf52dk/nrf52832.
To get a list of all supported Zephyr targets, run `west boards`

To change zephyr platform settings use:
```sh
west build -b nrf52840dk/nrf52840 -t menuconfig
```

To build a different example, e.g. the `gatt_streamer_server`, set the EXAMPLE environment variable:
```sh
EXAMPLE=gatt_streamer_server west build -b nrf52840dk/nrf52840
```

### 2. Flash Example

To flash a connected board:
```sh
west flash
```

## Building and Running on nRF5340

The nrf5340 is a dual core SoC, where one core is used for Bluetooth HCI functionality and
the other for the high level application logic. So both cores need to be programmed separately.
Using the nRF5340-DK for example allows debug output on both cores to separate UART ports.
For the nRF5340 a network core firmware is required, which one depends on the use-case.
With 2. an options is given.

### 1. Building the application
build using:
```sh
west build -b nrf5340dk/nrf5340/cpuapp
```
with debug:
```sh
west build -b nrf5340dk/nrf5340/cpuapp -- -DOVERLAY_CONFIG=debug_overlay.conf
```

### 2. Using zephyr network core image
the `hci_rgmsg` application needs to be loaded first to the network core.
Configure network core by selecting the appropriate config file, for example `nrf5340_cpunet_iso-bt_ll_sw_split.conf`.
additionally it's required to increase the main stack size from
```sh
CONFIG_MAIN_STACK_SIZE=512
```
to
```sh
CONFIG_MAIN_STACK_SIZE=4096
```
then the network core image can be compiled and flashed
```sh
west build -b nrf5340dk/nrf5340/cpunet -- -DCONF_FILE=nrf5340_cpunet_iso-bt_ll_sw_split.conf
west flash
```
or with debugging
```sh
west build -b nrf5340dk/nrf5340/cpunet -- -DCONF_FILE=nrf5340_cpunet_iso-bt_ll_sw_split.conf -DOVERLAY_CONFIG=debug_overlay.conf
west flash
```

## Build and Running on STM32 Nucleo 144 board with Ezurio M.2 Adapter and M.2 Bluetooth Controller

Ezurio's [Wi-Fi M.2 2230 to STM32 Nucleo Adapter Card](https://www.ezurio.com/product/wi-fi-m-2-2230-to-stm32-nucleo-adapter-card)
allows to connect any Bluetooth/Wifi M.2 Card to most STM32 Nucleo-144 boards. M.2 modules are available from multiple module 
vendors, e.g. Ezurio, u-blox, and Embedded Artists.

The adapter board takes care of 3.3V and 1.8V translation and routes SDIO for Wifi and UART for Bluetooth to the Nucleo board.
On most Nucleo-144 boards, the Bluetooth UART ends up on the same STM32, namely: PD3-PD6. 
See `port/zephyr/boards/shields/ezurio_m2_nucleo_144_adapter.overlay` for details.

We tested CYW55513 and CYW55573 M.2 modules with the following boards:

| Nucleo board  | Working |
|---------------|---------|
| nucleo_f439zi | x       | 
| nucleo_h753zi | x       | 
| nucleo_h563zi | x       | 
| nucleo_h755zi |         | 

The PatchRAM .hcd files can be placed in the project folder and specified by the `CONFIG_AIROC_CUSTOM_FIRMWARE_HCD_BLOB`
option in the corresponding `boards/$BOARD.conf` file.

### 1. Build Example

```sh
west build -b nucleo_f439zi --shield ezurio_m2_nucleo_144_adapter
west build -b nucleo_h755zi_q/stm32h755xx/m7 --shield ezurio_m2_nucleo_144_adapter
```

## TODO

- Allow/document use of Zephyr HCI Drivers
