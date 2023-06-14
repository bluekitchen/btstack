# Bluetooth: btstack

## Overview

This sample demonstrates Bluetooth functionality utilizing btstack.

## Requirements

* A board with Bluetooth LE support

## Building and Running

first step needs only be done once.
Step two every time.

### 1. Build environment preconditions
Follow the getting started [guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
till you can build samples.
Then update 'env.sh' accordingly.

### 2. Prepare the build environmet
before beeing able to build this sample the environment needs to be prepared with:
```sh
source env.sh
```

### 3. Building samples
You can build using:
```sh
west build -b nrf52840dk_nrf52840
```
to change zephyr platform settings use:
```sh
west build -b nrf52840dk_nrf52840 -t menuconfig
```
to build a different example:
```sh
EXAMPLE=gatt_streamer_server west build -b nrf52840dk_nrf52840
```
