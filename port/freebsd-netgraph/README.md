# BTstack Port for FreeBSD Systems

## Overview
This port assumes that FreeBSD provides an ng_hci netgraph node for a connected Bluetooth Controller. 
In most cases, these are Bluetooth USB dongles or built-in Bluetooth Controller connected via USB.

For Bluetooth Controllers connected via UART, the POSX-H4 port might be a better option als 

## Implementation details
In FreeBSD 13.2, the hci node is connected to a l2cap node and a btsock_hci_raw node. In order to take control, this 
port create a custom netgraph ng_socket node and connect to the 'acl' and 'raw' hooks of the hci node. The OS Bluetooth
functionality will be interrupted.

## Compilation

BTstack's FeeeBSD port does not have additional dependencies. To compile the cmake project with Make

    mkdir build
    cd build
    cmake ..
    make

or using Ninja:

    mkdir build
    cd build
    cmake -G Ninja ..
    ninja

## Running the examples

As the port needs to reconfigure the Bluetooth netgraph node, it needs to run with root privileges.
It tries to connect to 'ubt0hci' by default. If your Bluetooth Controller is different, you can select it with '-u node'
On start, BTstack prints the path to the packet log and prints the information on the detected Buetooth Controller.

	$ sudo gatt_counter
	Packet Log: /tmp/hci_dump.pklg
	BTstack counter 0001
	BTstack up and running on 00:1A:7D:DA:71:13.

## ToDO
- drop privileges after startup
- auto-detect ng_hci node
- support for profiles that require SCO: HFP & HSP
