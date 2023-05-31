# BTstack Port for POSIX Systems with NXP/Marvel H4 Bluetooth Controller

## Configuration
Most Bluetooth Bluetooth Controllers connected via UART/H4 require some special configuration, e.g. to set the UART baud rate, and/or require firmware patches during startup.
In this port, we've show how a NXP/Marvell Controller can be configured for use with BTstack. It's unclear if the required firmware file for older Controllers,
e.g. 88W8997, can be detected during the firmware upload. This port selects the firmware for NXP 88W8997. 
For newer Controllers, e.g. IW416 or IW612, the firmware can be selected automatically.

## Compilation

BTstack's posix-h4-nxp port does not have additional dependencies. You can directly run cmake and then your default build system. E.g. with Ninja:

	mkdir build
    cd build
    cmake -G Ninja ..
    ninja
 
## Running the examples

Please reset the Controller first. On start, BTstack prints the path to the packet log.

	$ ./gatt_counter
	Packet Log: /tmp/hci_dump.pklg
	BTstack counter 0001
	BTstack up and running on 00:1A:7D:DA:71:13.

## Issues
- NXP 88W8997 does not support SCO Flow Control which causes glitches when sending audio

## ToDo
- increase baud rate for firmware upload
- skip firmware upload if firmware already present
- increase baud rate for application
