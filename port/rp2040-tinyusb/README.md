# BTstack Port for Raspberry Pi Pico with TinyUSB

This port targets a Raspberry Pi Pico (RP2040) with a USB Bluetooth dongle on
the RP2040's native USB controller. TinyUSB runs in USB host mode; the current
`hci_transport_usb_tinyusb.c` discovers standard Bluetooth USB HCI interfaces
and transfers HCI command, event, and ACL packets.

## Hardware

Connect a USB Bluetooth dongle to the Pico USB data pins through a suitable
host adapter and provide 5 V VBUS power to the dongle. The RP2040 USB port
cannot also serve as a USB CDC console while it is in host mode; this project
therefore uses the Pico UART console on GPIO 0 (TX) and GPIO 1 (RX).

## Build

The port requires the Raspberry Pi Pico SDK and TinyUSB. Set `PICO_SDK_PATH`
and `PICO_TINYUSB_PATH` to their respective roots, then configure and build
from this directory:

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

## RP2040 TinyUSB host limitation

Under sustained Bluetooth ACL traffic, for example with the SPP streamer, the
RP2040 TinyUSB host controller driver can panic with:

```
panic("Data Seq Error \n")
hcd_rp2040_irq()
```

The panic occurs in TinyUSB's shared non-interrupt endpoint (EPX) scheduler,
before the BTstack transport receive callback runs. It is therefore not caused
by BTstack HCI packet reassembly.

TinyUSB master still handles `USB_INTS_ERROR_DATA_SEQ_BITS` by panicking. See
[hcd_rp2040.c](https://github.com/hathach/tinyusb/blob/master/src/portable/raspberrypi/rp2040/hcd_rp2040.c).
Relevant upstream reports are [TinyUSB issue #2776](https://github.com/hathach/tinyusb/issues/2776), which remains open and describes an RP2040 host-mode data-sequence limitation, and [issue #3533](https://github.com/hathach/tinyusb/issues/3533). Issue #3533 is closed without an associated upstream PR, but points to a more robust RP2040 HCD implementation in the [RP6502 project](https://github.com/picocomputer/rp6502/tree/main/src/tinyusb_rp6502).
