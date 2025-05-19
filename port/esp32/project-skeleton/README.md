# Standalone BTstack project skeleton for the Espressif Platform

This can be used as a basis for custom projects - just copy this folder and adapt it as needed.
The file `main/CMakeLists.txt` contains the main component. We have provided a trivial `your_project.c` file.
You can replace it with one of the official examples.

## Setup

- Follow [Espressif IoT Development Framework (ESP-IDF) setup](https://github.com/espressif/esp-idf) to install XTensa and RISC-V toolchains and the ESP-IDF.
- Currently used for testing: ESP-IDF v5.4.1

## Usage

Make sure `IDF_PATH` and `BTSTACK_ROOT` are set appropriately.

Choose a target and build. Bluetooth Classic is only supported on the ESP32, newer SoCs like ESP32-C3, ESP32-S3, etc only support Bluetooth LE.

```sh
$ idf.py set-target <desired target>
$ idf.py build
```

To upload the binary to your device, run:
```sh
	idf.py -p PATH-TO-DEVICE flash
```

To get debug output, run:
```sh
	idf.py monitor
```
You can quit the monitor with `CTRL-]`.
