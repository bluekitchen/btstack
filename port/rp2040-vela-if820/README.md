# BTstack Port for Ezurio Vela IF820 Series

The Vela IF820 Development kit and USB dongle use an  Raspberry Pi RP2040 MCU as a combined USB-to-UART adapter and SWD programmer. 

This port uses the RP2040 of the development kit as the main target with a connected Vela IF820 / CYW20820 Bluetooth Controller. 

In addition, the console is routed over USB CDC for easy integration and an USB audio interface is provided for sound output.

## Hardware

As this port targets the RP2040 on the development kit or the USB dongle, the Vela IF820 in module format cannot be used.

[More info on the Vela IF820 series](
https://www.ezurio.com/wireless-modules/bluetooth-modules/bluetooth-5-modules/vela-if820-series-bluetooth-5-2-dual-mode-modules)

## Loading HCI Firmware into the Vela IF820/CYW20820
To use the Bluetooth Controller with BTstack, it needs to be flashed with the HCI Firmware provided by Ezurio.
Please use their [Flasher tool with the provided HCI firmware](https://github.com/Ezurio/Vela_IF820_Firmware).

To flash the HCI firmware, hold the "Module Recovery" button while pressing and releasing "Module Reset". 
Then start the Windows GUI and select the correct HCI firmware.

## Software

The port for the RP2040 requires the official [pico-sdk](https://github.com/raspberrypi/pico-sdk).
Please follow the [Getting Started Guide](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf) and make sure to point `PICO_SDK_PATH` to the root folder of the SDK.

The current `pico-sdk` v2.2.0 uses TinyUSB v0.18, which has an issue with opening the virtual soundcard more than once.
Please update TinyUSB in `lib/tinyusb` to the current master branch or at least commit 5b200c4:

    $ cd ${PICO_SDK_PATH}/lib/tinyusb
    $ git checkout 5b200c4

After pointing `PICO_SDK_PATH` to the SDK, all examples can be compiled like this: 
     
    $ mkdir build
    $ cd build
    $ cmake -DCMAKE_BUILD_TYPE=Release ..
    $ make

All examples are placed in the `build` folder. Please make sure to specify the Release build type.
By default, pico-sdk uses Debug built type and the generated code isn't fast enough for the SBC Codec in the 
A2DP demos.

You can compile a single example by calling make with the name of the target .elf file, e.g. `make gatt_counter.elf`

## Flash And Run The Examples

By default, different versions of each example are build including:
- example.elf: .elf file with all debug information
- example.uf2: binary file that can be used to flash via the mass storage interface.

### Flash using MSC Interface
Hold the BOOTSEL button then press and release the "Debug Reset" button to enter bootloader mode. The RP2040 should 
get mounted by the OS. You can now copy any .uf2 into the root folder of the virtual RP2040 device. 
Press "Debug Reset" to start it.

### Flash using Picotool
If you still have the picoprobe firmware installed, hold the BOOTSEL button then press and release the "Debug Reset"
button to enter bootloader mode. You can now comfortably load any .elf file with: `picotool load gatt_streamer_server.elf`.
After that press the "Debug Reset" button once or run `picotool reboot`. 

The port contains the custom Reset logic to enter bootloader mode and you can directly load a new target with
`picotool load -f a2dp_sink_demo.elf`

## Flash and Debug using SWD Interface
As the Vela IF820 has a TagConnect 2030 interface, the RP2040 can be programmed and debugged with a suitable adapter 
and any SWD programmer, e.g. SEGGER J-Link or Raspberry Pi's Picoprobe. 

## A2DP Sink Demo Example
The most interesting example for this port is the A2DP Sink Demo as it allows to stream music in Hifi quality to your computer.

After flashing the demo onto the RP2040, you can connect from a mobile phone to the `A2DP Sink Demo xx:xx:xx:xx:xx:xx`
device and play music. 
The music is available via the USB soundcard implemented on the RP2040 as part of this port.

On macOS, you can start `Quicktime Player.app` and select `New Audio Recording` from the File menu. 
The RP2040 shows up as "BTstack Audio" interface.
Another option is to use `Audacity`, enable `Software playthrough of interface` in `Settings->Recording` and start recording.

## Console / Debug Output

All debug output is sent over the USB CDC. You can interact with the BTstack examples with a regular terminal program 
like [picocom](https://github.com/npat-efault/picocom)

## GATT Database
In BTstack, the GATT Database is defined via the `.gatt` file in the example folder. The `CMakeLists.txt` contains rules
to update the `.h` file when the `.gatt` was modified.

## HCI Traces

To enable HCI traces, you can uncomment the `WANT_HCI_DUMP=1` line in `CMakeLists.txt`. If you capture the output into
a `log.txt` file, you can use `tool/create_packet_log.py log.txt` to convert it into `log.pklg` which can be opened and
analyzed with [Wireshark](https://www.wireshark.org).

## Uninstall
To restore the Vela IF820 dev kit to its original form, or to update the firmware on the CYW20820 module, you can 
re-flash `picoprobe` either with `picotool` or by entering the recovery mode (hold BOOSEL, press and release Debug Reset).
