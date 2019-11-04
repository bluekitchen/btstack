# BTstack Port for Raspberry Pi 3 with BCM4343 Bluetooth/Wifi Controller

Tested with Raspberry Pi 3 Model B V1.2, Raspberry Pi 3 Model B+, and Raspberry Pi Zero W V1.1.

With minor fixes, the port should also work with older Raspberry Pi models that use the [RedBear pHAT](https://redbear.cc/product/rpi/iot-phat.html). See TODO at the end.

## Raspberry Pi 3 / Zero W Setup

There are various options for setting up the Raspberry Pi, have a look at the Internet. Here's what we did:

### Install Raspian Stretch Lite:

- Insert empty SD Card
- Download Raspian Stretch Lite from https://www.raspberrypi.org/downloads/raspbian/
- Install Etcher from https://etcher.io
- Run Etcher:
  - Select the image you've download before
  - Select your SD card
  - Flash!

### Configure Wifi

Create the file wpa_supplicant.conf in the root folder of the SD Card with your Wifi credentials:

	ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
	network={
	    ssid="YOUR_NETWORK_NAME"
	    psk="YOUR_PASSWORD"
	    key_mgmt=WPA-PSK
	}

Alternatively, just plug-it in via Ethernet - unless you have a Raspberry Pi Zero W.

### Enable SSH

Create an empty file called 'ssh' in the root folder of the SD Card to enable SSH.

### Boot

If everything was setup correctly, it should now boot up and join your Wifi network. You can reach it via mDSN as 'raspberrypi.local' and log in with user: pi, password: raspberry.

### Disable bluez

By default, bluez will start up using the the BCM4343. To make it available to BTstack, you can disable its system services:

	$ sudo systemctl disable hciuart
    $ sudo systemctl disable bthelper
	$ sudo systemctl disable bluetooth

and if you don't want to restart, you can stop them right away. Otherwise, please reboot here.

	$ sudo systemctl stop hciuart
    $ sudo systemctl stop bthelper
	$ sudo systemctl stop bluetooth

If needed, they can be re-enabled later as well.

## Compilation

The Makefile assumes cross-compilation using the regular GCC Cross Toolchain for gnueabihf: arm-linux-gnueabihf-gcc. This should be available in your package manager. Read on for a heavy, but easy-to-use approach.

### Compile using Docker

For non-Linux users, we recommend to use a [Raspberry Pi Cross-Compiler in a Docker Container](https://github.com/sdt/docker-raspberry-pi-cross-compiler).
Please follow the installation instructions in the README. 

Then, setup a shared folder in Docker that contains the BTstack repository.
Now, go to the BTstack repository and 'switch' to the Raspberry Pi Cross-Compiler container:

	$ rpxc bash

The default images doesn't have a Python installation, so we manually install it:

	$ sudo apt-get install python

Change to the port/raspi folder inside the BTstack repo:

	$ cd btstack/port/raspi

and compile as usual:

	$ make

For regular use, it makes sense to add Python permanently to the Docker container. See documentation at GitHub.

## Running the examples

Copy one of the examples to the Rasperry Pi and just run them. BTstack will power cycle the Bluetooth Controller on models without hardware flowcontrol, i.e., Pi 3 A/B. With flowcontrol, e.g Pi Zero W and Pi 3 A+/B+, the firmware will only uploaded on first start. 

	pi@raspberrypi:~ $ ./le_counter
	Packet Log: /tmp/hci_dump.pklg
	Hardware UART without flowcontrol
	Phase 1: Download firmware
	Phase 2: Main app
	BTstack counter 0001
	BTstack up and running at B8:27:EB:27:AF:56

## Bluetooth Hardware Overview

Model                    | Bluetooth Controller                                    | UART Type | UART Flowcontrol | BT_REG_EN   | HCI Transport | Baudrate
-------------------------|---------------------------------------------------------|-----------|------------------|-------------|---------------|----------
Older                    | None                                                    |           |                  |             |               |
Pi 3 Model A, Model B    | [CYW43438](http://www.cypress.com/file/298076/download) | Hardware  | No               | 128         | H5            | 921600
Pi 3 Model A+, Model B + | [CYW43455](http://www.cypress.com/file/358916/download) | Hardware  | Yes              | 129 (See 1) | H4            | 921600 (See 2)
Pi Zero W                | [CYW43438](http://www.cypress.com/file/298076/download) | Hardware  | Yes              | 45          | H4            | 921600

1. Model A+/B+ have BT_REG_EN AND WL_REG_EN on the same (virtual) GPIO 129. A Bluetooth Controller power cycle also shuts down Wifi (temporarily). BTstack avoids a power cycle on A+/B+.
2. Model A+/B+ support 3 mbps baudrate. Not enabled/activated yet.

## TODO
- Raspberry Pi Zero W: Check if higher baud rate can be used, 3 mbps does not work.
- Raspberry + RedBear IoT pHAT (AP6212A = BCM4343) port: IoT pHAT need to get detected and the UART configured appropriately.
