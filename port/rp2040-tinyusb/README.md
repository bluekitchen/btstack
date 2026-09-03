# BTstack Port for Raspberry Pi Pico with TinyUSB

This port targets a Raspberry Pi Pico (RP2040) with a USB Bluetooth dongle on
the RP2040's native USB controller. TinyUSB runs in USB host mode; the current
`hci_transport_usb_tinyusb.c` is a transport skeleton that initializes and
services the TinyUSB host stack, but does not yet implement Bluetooth USB HCI
endpoint discovery or transfers.

## Hardware

Connect a USB Bluetooth dongle to the Pico USB data pins through a suitable
host adapter and provide 5 V VBUS power to the dongle. The RP2040 USB port
cannot also serve as a USB CDC console while it is in host mode; this project
therefore uses the Pico UART console on GPIO 0 (TX) and GPIO 1 (RX).

## Build

The port requires the Raspberry Pi Pico SDK. Set `PICO_SDK_PATH` to the SDK
root, then configure and build from this directory:

```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

Each supported BTstack example is built as its own target. For example:

```
make gatt_counter.elf
```

The generated `.uf2` file can be flashed using the Pico BOOTSEL mass-storage
interface.
