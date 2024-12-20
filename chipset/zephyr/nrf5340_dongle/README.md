# nRF5340 (Audio) DK Board as USB HCI Dongle Guide

This will turn the Nordic nRF5340 DK and Audio DK boards into an USB CDC HCI dongle that uses Nordic's SoftDevice Controller.

## Preconditions
- [Install Nordic's nRF Connect SDK, v2.8 or higher](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/installation.html) into ${NCS_ROOT}
- Copy the included files and folders to '${NCS_ROOT}/zephyr/samples/bluetooth/hci_uart/'
    - `sysbuild`
    - `Kconfig.sysbuild`
    - `overlay-usb.conf`
    - `usb.overlay`

## Build/Flash HCI Bridge on AppCore

You need to specify the board with the '-b' param in the west call.

For nRF5340 DK:

```sh
cd  ${NCS_ROOT}/zephyr/samples/bluetooth/hci_uart/
west build --pristine -b nrf5340dk/nrf5340/cpuapp -- -DDTC_OVERLAY_FILE=usb.overlay -DOVERLAY_CONFIG=overlay-usb.conf
west flash
```

For nRF5340 Audio DK (ADK)

```sh
cd  ${NCS_ROOT}/zephyr/samples/bluetooth/hci_uart/
west build --pristine -b nrf5340dk/nrf5340/cpuapp -- -DDTC_OVERLAY_FILE=usb.overlay -DOVERLAY_CONFIG=overlay-usb.conf
west flash
```

## Usage / Find Serial Port

On macOS, the nRF5340 ADK with the HCI bridge show up as 5 /dev/tty. To find the HCI one, list all /dev/tty.* devices,
then press and hold the RESET button and list all devices again. The missing one is the one provided by the HCI bridge.

On the nRF5340 DK, you can directly plug into the `nRF5340 USB` port and get only a single /dev/tty.

## HCI log

```sh
btmon -J nrf5340_xxaa_app -w <logfile>
```
