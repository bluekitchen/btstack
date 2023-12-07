These samples are meant to be used with the [PTS Dongel](https://bluekitchen-gmbh.com/bluetooth-pts-with-nordic-nrf52840-usb-dongle/) or an equivalent setup like
the one described in here using a nrf5340dk.

# nrf5340dk as HCI dongle
for this a working [Zephyr](https://www.zephyrproject.org/) build environment is required, where the setup of Zephyr is beyond the scope of this document.

The nrf5340 is a dual core SOC for which the network core handles the low level radio control and the application core handles the actual application.
So to make a working dongle the network core and the application core need to be programmed.

### network core / Packetcraft LL
for nrf5340 the latest netcore firmware is located at [sdk-nrf](https://github.com/nrfconnect/sdk-nrf/tree/main/lib/bin/bt_ll_acs_nrf53/bin)
to program it:
```sh
nrfjprog --program ble5-ctr-rpmsg_<version number>.hex --chiperase --coprocessor CP_NETWORK -r
```

### application core
the `hci_uart` sample is used here over USB CDC
build using:
```sh
west build -b nrf5340dk_nrf5340_cpuapp -- -DDTC_OVERLAY_FILE=usb.overlay -DOVERLAY_CONFIG=overlay-usb.conf
```
with `usb.overlay` specifying to use USB CDC instead of a physical UART
```c
/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/ {
	chosen {
		zephyr,bt-c2h-uart = &cdc_acm_uart0;
	};
};

&zephyr_udc0 {
	cdc_acm_uart0: cdc_acm_uart0 {
		compatible = "zephyr,cdc-acm-uart";
	};
};
```
and `overlay-usb.conf` to enable USB
```make
CONFIG_USB_DEVICE_STACK=y
CONFIG_USB_DEVICE_PRODUCT="Zephyr HCI UART sample"
CONFIG_USB_CDC_ACM=y
CONFIG_USB_DEVICE_INITIALIZE_AT_BOOT=n
```
