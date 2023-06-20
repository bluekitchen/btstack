# BTstack Port for Zephyr RTOS

## Overview

This port targets any platform supported by Zephyr that either contains a built-in Bluetooth Controller 
or is connected to an external Controller via one of the supported Zephyr HCI Transports drivers (see `zephyr/drivers/bluetooth/hci`)

## Status

Tested with nRF52 DK (PCA10040) and nRF52840 DK (PC10056) boards only. It uses the fixed static random BD ADDR stored in NRF_FICR, which will not compile on non nRF SoCs.


## Building and Running

The first step needs to done once. Step two is needed every time to setup the environment.

### 1. Build Environment Preconditions

Follow the getting started [guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
until you are able to build an example.

Then update the `ZEPHYR_ROOT` variable in `env.sh` to point to your `zephyrproject`. Defaults to `~/zephyrproject`


### 2. Prepare the build environmet

To setup your environment to build a BTstack example, run the provided setup in `env.sh`.

```sh
source env.sh
```

### 3. Build Example

You can build an example using:
```sh
west build -b nrf52840dk_nrf52840
```

`nrf52840dk_nrf52840` selected the Nordic nRF52840 DK. For the older nRF52 DK with nRF52832, you can specify nrf52dk_nrf52832.
To get a list of all supported Zephyr targets, run `west boards`

To change zephyr platform settings use:
```sh
west build -b nrf52840dk_nrf52840 -t menuconfig
```

To build a different example, e.g. the `gatt_streamer_server`, set the EXAMPLE environment variable:
```sh
EXAMPLE=gatt_streamer_server west build -b nrf52840dk_nrf52840
```

### 4. Flash Example

To flash a connected board:
```sh
west flash
```


## TODO

- Read NRF_FICR on Nordic SoCs
- Allow/document use of Zephyr HCI Drivers


