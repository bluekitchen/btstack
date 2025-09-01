# BTstack Port for Linux Systems

While BTstack can directly work on Linux with most Bluetooth Controllers that are connected via UART (port/posix-h4) or USB (port/libusb), it might be convenient to use the Linux Bluetooth Subsystem in some cases, e.g. if the Bluetooth Controller uses other transports (such as SDIO) or the Controller is already fully configured by the distributions.


## Compilation

In addition to regular C build tools, you also need the Bluetooth development package installed.

	sudo apt install libbluetooth-dev

Now you can compile it as usual with CMake

	mkdir build
	cd build
	cmake ..
	make


## Running the examples

Please make sure that BlueZ is not installed or at least disabled

	sudo systemctl stop bluetooth
	sudo systemctl disable bluetooth
	sudo systemctl mask bluetooth

Also make sure that the chosen device (here, hci0) is down

	sudo hciconfig hci0 down

To check

    hciconfig hci0

    hci0:	Type: Primary  Bus: USB
	        BD Address: 00:1A:7D:DA:71:13  ACL MTU: 1021:8  SCO MTU: 64:1
	        DOWN 
	        RX bytes:566359 acl:0 sco:0 events:40 errors:0
	        TX bytes:2174059 acl:2694 sco:0 commands:329 errors:0


To access the Bluetooth Controller, you can either run the examples as root, or, set the necessary permissions for a compiled example, e.g.

	sudo setcap 'cap_net_raw,cap_net_admin+eip' gatt_counter

Now, you can run this example as a regular user

	$ ./gatt_counter
	Packet Log: /tmp/hci_dump.pklg
	BTstack counter 0001
	BTstack up and running on 00:1A:7D:DA:71:13.


## Status

When running gatt_counter, a basic LE Peripheral with a GATT Service, the first two connections fail, while the third and later work as expected. There's no difference when looking at the HCI Trace with btmon, just that no ACL packets are received in the failing attempts.
