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

## Console Output

Console output is disabled by default. Add one of these configuration fragments when debug output is needed:

- `debug-segger.conf` enables console output via SEGGER RTT
- `debug-usb.conf` enables console output via USB CDC ACM on boards whose devicetree routes `zephyr,console` to a CDC ACM UART; the IF310 board definition provides this routing

For example, to build IF310 with no console output:

```sh
west build -b if310
```

To build IF310 with SEGGER RTT console output:

```sh
west build -b if310 -- -DEXTRA_CONF_FILE=debug-segger.conf
```

To build IF310 with USB CDC console output:

```sh
west build -b if310 -- -DEXTRA_CONF_FILE=debug-usb.conf
```

When a board already needs another fragment, pass both as a semicolon-separated list, for example:

```sh
west build -b nrf52840dk/nrf52840 -- -DEXTRA_CONF_FILE="nordic_bt_ctlr.conf;debug-segger.conf"
```

## Building and Running on nRF52840

### 1. Build Example

You can build an example using:
```sh
west build -b nrf52840dk/nrf52840 -- -DEXTRA_CONF_FILE=nordic_bt_ctlr.conf
```

`nrf52840dk/nrf52840` selected the Nordic nRF52840 DK. For the older nRF52 DK with nRF52832, you can specify nrf52dk/nrf52832.
To get a list of all supported Zephyr targets, run `west boards`

Nordic Bluetooth Controller options shared by Nordic SoCs are kept in `nordic_bt_ctlr.conf`. When building with the Nordic SoftDevice Controller, also include `nordic_sdc.conf`:
```sh
west build -b nrf52840dk/nrf52840 -- -DEXTRA_CONF_FILE="nordic_bt_ctlr.conf;nordic_sdc.conf"
```

To change zephyr platform settings use:
```sh
west build -d build -t menuconfig
```

To build a different example, e.g. the `gatt_streamer_server`, set the EXAMPLE environment variable:
```sh
EXAMPLE=gatt_streamer_server west build -b nrf52840dk/nrf52840 -- -DEXTRA_CONF_FILE=nordic_bt_ctlr.conf
```

### 2. Flash Example

To flash a connected board:
```sh
west flash
```

## Persistent BTstack TLV Storage

BTstack automatically uses Zephyr flash-backed TLV storage when all of the following are true:

- `CONFIG_FLASH=y`
- `zephyr,code-partition` is defined in devicetree
- A fixed partition with the node label `storage_partition` exists

When these requirements are met, the BTstack Zephyr port splits `storage_partition` into two equal flash banks for its TLV backend. If any requirement is missing, it falls back to non-persistent TLV storage.

Boards that provide TLV storage should reserve a dedicated `storage_partition` outside the application image and keep `zephyr,code-partition` pointing at the application flash region. The IF310 board definition is an RP2040 example:

- `boards/if310.conf` enables Zephyr flash support
- `boards/ezurio/if310/if310.dts` reserves the final 32 KiB of flash as `storage_partition`

The STM32 Nucleo board overlays under `boards/` provide additional fixed-partition examples.

## External Bluetooth Controllers

BTstack uses Zephyr's HCI driver layer to talk to an external Bluetooth Controller. For this port, the Bluetooth Host is provided by BTstack, while Zephyr provides the HCI Transport.

### Kconfig Requirements

The common BTstack requirements are:

```conf
CONFIG_BT=y
CONFIG_BT_HCI_RAW=y
```

For a UART H4 controller such as the IF310 module, the board fragment also needs the UART backend used by Zephyr's HCI transport:

```conf
CONFIG_SERIAL=y
CONFIG_UART_INTERRUPT_DRIVEN=y
```

Controller-specific setup belongs in the board fragment as well. The IF310 board uses Infineon AIROC PatchRAM download at boot:

```conf
CONFIG_BT_AIROC_CUSTOM=y
CONFIG_AIROC_CUSTOM_FIRMWARE_HCD_BLOB="CYW55500A1_001.002.032.0233.0117.hcd"
CONFIG_AIROC_AUTOBAUD_MODE=y
```

Those AIROC options are specific to the CYW55xxx/CYW43xxx controller family. Other external controllers will use their own Zephyr driver Kconfig, if any.

### Devicetree Requirements

The selected controller must be exposed through Zephyr's `zephyr,bt-hci` chosen node:

```dts
/ {
	chosen {
		zephyr,bt-hci = &bt_hci;
	};
};
```

For a UART H4 controller, the UART node must be enabled, pinned out, contain a `zephyr,bt-hci-uart` child node and have hardware flow control is required.
Here's the snippet from the IF310 board definition for the RP2040 connected to a CYW55310:

```dts
&uart0 {
	status = "okay";
	current-speed = <115200>;
	hw-flow-control;
	pinctrl-0 = <&bt_uart0_default>;
	pinctrl-names = "default";

	bt_hci: bt_hci {
		status = "okay";
		compatible = "zephyr,bt-hci-uart";

		cyw55310 {
			status = "okay";
			compatible = "infineon,cyw43xxx-bt-hci";
			bt-reg-on-gpios = <&gpio0 13 GPIO_ACTIVE_HIGH>;
			fw-download-speed = <921600>;
			hci-operation-speed = <921600>;
		};
	};
};
```

For the Infineon binding, `current-speed` should match the controller's boot ROM baud rate; `fw-download-speed` and `hci-operation-speed` 
may then raise the baud rate for PatchRAM transfer and normal HCI traffic. 


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
west build -b nrf5340dk/nrf5340/cpunet -- -DCONF_FILE=nrf5340_cpunet_iso-bt_ll_sw_split.conf -DEXTRA_CONF_FILE=nordic_bt_ctlr.conf
west flash
```
or with debugging
```sh
west build -b nrf5340dk/nrf5340/cpunet -- -DCONF_FILE=nrf5340_cpunet_iso-bt_ll_sw_split.conf -DEXTRA_CONF_FILE=nordic_bt_ctlr.conf -DOVERLAY_CONFIG=debug_overlay.conf
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
