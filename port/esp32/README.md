# BTstack Port for the Espressif ESP32 Platform

Status: Experimental/initial port. Only LE Counter example provided. Timers are not supported. 

## Setup

- Follow [Espressif IoT Development Framework (IDF) setup](https://github.com/espressif/esp-idf) to install xtensa toolchain and esp-idf.
- In port/esp32, configure serial port for firmware upload as described before
	
## Usage

In port/esp32, run

	make

to compile the LE Counter example

Run 
	
	make flash

to upload it to your device.

# Acknowledgments

First HCI Reset was sent to Bluetooth chipset [Matt Kelly](https://github.com/mattkelly)
