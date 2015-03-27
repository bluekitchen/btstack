# Welcome to BTstack

BTstack is BlueKitchen's implementation of the official Bluetooth stack. It is well suited for small, resource-constraint devices 
such as 8 or 16 bit embedded systems as it is highly configurable and comes with an ultra small memory footprint. 
A minimal configuration for an SPP server on a MSP430 can run in 32 kB FLASH and only 4 kB of RAM.

It connects to the Bluetooth modules via different Bluetooth HCI transport layers (e.g., HCI H4 UART and 
H5 the "Tree-Wire" protocol). The various platforms can be easily targeted by providing the necessary 
UART, CPU, and CLOCK implementations. 

On smaller embedded systems, a minimal run loop implementation allows to use BTstack without a Real Time OS (RTOS). 
If a RTOS is already provided, BTstack can be integrated and run as a single thread. 

On larger systems, BTstack provides a daemon that connects to a Bluetooth module. 
Multiple applications can communicate with this daemon over different inter-process communication methods.

BTstack supports both, the Central and the Peripheral Role of Bluetooth 4.0 Low Energy specification. 
It can be configures as both a single mode or a dual mode stack.

For starters, download the [BTstack Manual v1.1](http://bluekitchen-gmbh.com/docs/btstack-gettingstarted-1.1.pdf) 
and look for an Architecture overview and the Getting started example for MSP430.

BTstack is free for non-commercial use. For commercial use, <a href="mailto:contact@bluekitchen-gmbh.com">tell us</a?=> 
a bit about your project to get a quote.
The Serial Port Profile (SPP) and the Bluetooth 4.0 Low Energy Peripheral role (LE Peripheral) have been qualified with 
the Bluetooth SIG (QD ID 54558). This summer, we plan to qualify for Bluetooth Core 4.2,
together with LE Central, PAN/BNEP and HSP.

## Supported Protocols
* L2CAP
* RFCOMM
* SDP
* BNEP
* ATT
* SM

## Supported Profiles
* GAP
* IOP
* SPP 
* PAN
* GATT
* Coming soon: HSP, HFP, and more.
