# BTstack Port for Zephyr RTOS

## Overview

This port targets any platform supported by Zephyr that either contains a built-in Bluetooth Controller
or is connected to an external Controller via one of the supported Zephyr HCI Transports drivers (see `zephyr/drivers/bluetooth/hci`)

## Status

Tested with the following Nordic boards:

| Board | Zephyr target |
|-------|---------------|
| nRF52 DK (PCA10040) | `nrf52dk/nrf52832` |
| nRF52840 DK (PCA10056) | `nrf52840dk/nrf52840` |
| nRF5340 DK (PCA10095) | `nrf5340dk/nrf5340/cpuapp` |
| nRF54L15 DK | `nrf54l15dk/nrf54l15/cpuapp` |
| nRF54LM20 DK, nRF54LM20A variant | `nrf54lm20dk/nrf54lm20a/cpuapp` |

Tested with the following non-Nordic development kits using an external
Bluetooth Controller:

| Board | Zephyr target | Bluetooth Controller |
|-------|---------------|----------------------|
| Ezurio IF310 DK | `if310` | Infineon CYW55310 over HCI UART |

For Zephyr/Nordic Controllers, BTstack uses the Zephyr vendor command to read the
Controller's fixed static random address instead of accessing Nordic FICR registers directly.

The board-specific Kconfig fragments for tested Nordic boards are generated and
committed. Nordic SoftDevice Controller defaults live in `nordic_sdc.conf`.
After changing Nordic board defaults, update `nordic_sdc.conf` and run:

```sh
python3 generate_board_configs.py
```

## Build Environment

BTstack's Zephyr port can be built either with plain upstream Zephyr or with Nordic's
nRF Connect SDK (NCS). Use NCS for Nordic boards. Use upstream Zephyr main for
the IF310 DK.

### 1. Build Environment Preconditions

For plain Zephyr, follow the upstream Zephyr getting started
[guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)
until you are able to build an example. By default, `env-zephyr.sh` expects this workspace
at `~/zephyrproject`.

For Nordic boards, you can alternatively use an NCS installation managed by
`nrfutil sdk-manager`.

### 2. Prepare the build environment

For a plain Zephyr workspace:

```sh
source env-zephyr.sh
```

If your Zephyr workspace is not in `~/zephyrproject`, set one of these before
sourcing `env-zephyr.sh`:

```sh
export BTSTACK_ZEPHYR_ROOT=/path/to/zephyrproject
source env-zephyr.sh

export BTSTACK_ZEPHYR_BASE=/path/to/zephyrproject/zephyr
source env-zephyr.sh
```

For Nordic nRF Connect SDK via `nrfutil sdk-manager`:

```sh
source env-ncs.sh
```

`env-ncs.sh` uses the latest installed NCS version by default. Set
`BTSTACK_NCS_VERSION` to select a specific version:

```sh
export BTSTACK_NCS_VERSION=v3.3.1
source env-ncs.sh
```

The script uses `nrfutil sdk-manager list` to find the installed NCS directory and
then activates the matching toolchain with `nrfutil sdk-manager toolchain env`.
If discovery is not possible, set `BTSTACK_NCS_ROOT` explicitly:

```sh
export BTSTACK_NCS_ROOT=/opt/nordic/ncs/v3.3.1
source env-ncs.sh
```

If you use `direnv`, keep a local `.envrc` in this folder and source the script
you want. For example:

```sh
source ./env-ncs.sh
```

## Building and Running on Ezurio IF310 DK

The IF310 DK contains an RP2040 MCU connected to an Infineon CYW55310 Bluetooth
Controller over HCI UART. It currently uses upstream Zephyr main, not NCS.

### 1. Prepare Plain Zephyr

```sh
source env-zephyr.sh
```

### 2. Build Example

```sh
west build -b if310 -d build-if310 -p always
```

To build a different example, e.g. the `gatt_streamer_server`, set the
`EXAMPLE` environment variable:

```sh
EXAMPLE=gatt_streamer_server west build -b if310 -d build-if310 -p always
```

### 3. Flash Example

```sh
west flash -d build-if310
```

The IF310 build downloads the CYW55310 PatchRAM HCD file automatically if it is
missing. The filename, pinned Ezurio `ifx_flasher` URL, and SHA256 are defined in
`chipset/bcm/btstack_cyw55310_patchram.cmake`.

## External Bluetooth Controller Requirements

Boards using an external Bluetooth Controller via Zephyr's HCI UART transport
need both Kconfig and devicetree support.

Kconfig requirements:

```conf
CONFIG_BT=y
CONFIG_BT_HCI_RAW=y
CONFIG_SERIAL=y
CONFIG_UART_INTERRUPT_DRIVEN=y
```

Infineon AIROC controllers that need PatchRAM download also require:

```conf
CONFIG_BT_AIROC_CUSTOM=y
CONFIG_AIROC_CUSTOM_FIRMWARE_HCD_BLOB="path-to-firmware.hcd"
CONFIG_AIROC_AUTOBAUD_MODE=y
```

The devicetree must select the HCI transport node with `zephyr,bt-hci`, enable an
H:4 UART transport, and configure hardware flow control when the board wiring
requires it. IF310 uses UART0 with hardware flow control and a reset GPIO:

```dts
chosen {
	zephyr,bt-hci = &bt_hci;
};

&uart0 {
	status = "okay";
	current-speed = <115200>;
	hw-flow-control;

	bt_hci: bt_hci {
		status = "okay";
		compatible = "zephyr,bt-hci-uart";

		cyw55310 {
			status = "okay";
			compatible = "infineon,bt-hci-uart";
			bt-reg-on-gpios = <&gpio0 13 GPIO_ACTIVE_HIGH>;
			fw-download-speed = <921600>;
			hci-operation-speed = <921600>;
		};
	};
};
```

## Persistent BTstack TLV Storage

BTstack can use Zephyr flash storage for its TLV database. The application needs
flash-map support:

```conf
CONFIG_FLASH=y
CONFIG_FLASH_MAP=y
```

The board devicetree must provide a `zephyr,code-partition` chosen node and a
`storage_partition`. IF310 reserves the last 32 KiB of the RP2040 external flash
for this purpose.

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

## Building and Running on nRF54L Series

The following nRF54 DK targets have been tested with NCS:

```sh
west build -b nrf54l15dk/nrf54l15/cpuapp -d build-nrf54l15dk
west build -b nrf54lm20dk/nrf54lm20a/cpuapp -d build-nrf54lm20dk
```

To build a different example, e.g. the `gatt_streamer_server`, set the `EXAMPLE`
environment variable:

```sh
EXAMPLE=gatt_streamer_server west build -b nrf54lm20dk/nrf54lm20a/cpuapp -d build-nrf54lm20dk
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
