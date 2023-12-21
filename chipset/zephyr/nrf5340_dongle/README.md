# nRF5340 (Audio) DK Board as USB HCI Dongle Guide

This will turn the Nordic nRF5340 DK and Audio DK boards into an USB CDC HCI dongle.

If using the NRF Connect SDK ( sdk-nrf ) it is important to follow the order in this document, as 'west flash' 
will also program the NetCore with zephyr-ll which needs to be replaced afterwards with the Packetcraft Link Layer.

## Preconditions
Copy the included files to '${ZEPHYR_ROOT}/zephyr/examples/bluetooth/hci_uart/'

## Build/Flash HCI Bridge on AppCore

You need to specify the board with the '-b' param in the west call.

For nRF5340 DK:

```sh
cd  ${ZEPHYR_ROOT}/zephyr/samples/bluetooth/hci_uart/
west build --pristine -b nrf5340dk_nrf5340_cpuapp -- -DDTC_OVERLAY_FILE=usb.overlay -DOVERLAY_CONFIG=overlay-usb.conf
west flash
```

For nRF5340 Audio DK (ADK)

```sh
cd  ${ZEPHYR_ROOT}/zephyr/samples/bluetooth/hci_uart/
west build --pristine -b nrf5340_audio_dk_nrf5340_cpuapp -- -DDTC_OVERLAY_FILE=usb.overlay -DOVERLAY_CONFIG=overlay-usb.conf
west flash
```

## Flash Packetcraft Link Layer on NetCore

```sh
nrfjprog --program ble5-ctr-rpmsg_3424.hex --chiperase --coprocessor CP_NETWORK -r --verify
```

the corresponding hex file is in the [nrfConnectSDK](https://github.com/nrfconnect/sdk-nrf/blob/main/lib/bin/bt_ll_acs_nrf53/bin)

## Usage / Find Serial Port

On macOS, the nRF5340 with the HCI bridge show up as 5 /dev/tty. To find the HCI one, list all /dev/tty.* devices,
then press and hold the RESET button and list all devices again. The missing one is the one provided by the HCI bridge.

## HCI log

```sh
btmon -J nrf5340_xxaa_app -w <logfile>
```
