**_Note: Major API Changes. For older projects, you may use the [v0.9 branch](https://github.com/bluekitchen/btstack/tree/v0.9). 
Please see [Migration notes](https://github.com/bluekitchen/btstack/blob/master/doc/manual/docs/appendix/migration.md)_**

# Welcome to BTstack

BTstack is [BlueKitchen's](http://bluekitchen-gmbh.com) implementation of the official Bluetooth stack. 
It is well suited for small, resource-constraint devices 
such as 8 or 16 bit embedded systems as it is highly configurable and comes with an ultra small memory footprint. 
A minimal configuration for an SPP server on a MSP430 can run in 32 kB FLASH and only 4 kB of RAM.

It connects to the Bluetooth modules via a different Bluetooth HCI transport layers (e.g., HCI H4 UART and 
H5 the "Tree-Wire" protocol, HCI H2 USB). Various platforms can be easily targeted by providing the necessary 
UART, CPU, and CLOCK implementations. 

On smaller embedded systems, a minimal run loop implementation allows to use BTstack without a Real Time OS (RTOS). 
If a RTOS is already provided, BTstack can be integrated and run as a single thread. 

On larger systems, BTstack provides a daemon that connects to a Bluetooth module. 
Multiple applications can communicate with this daemon over different inter-process communication methods.

BTstack supports both, the Central and the Peripheral Role of Bluetooth 4.2 Low Energy specification. 
It can be configures as both a single mode or a dual mode stack.

BTstack is free for non-commercial use. For commercial use, <a href="mailto:contact@bluekitchen-gmbh.com">tell us</a> 
a bit about your project to get a quote.
It has been qualified with the the Bluetooth SIG for GAP, IOP, HFP, HSP, SPP, PAN profiles and 
GATT, SM of the Bluetooth 4.2 LE Central and Peripheral roles (QD ID 25340).

## Documentation
- [HTML](http://bluekitchen-gmbh.com/btstack/)
- [PDF](http://bluekitchen-gmbh.com/btstack.pdf)

## Discussion and Community Support
[BTstack Google Group](http://groups.google.com/group/btstack-dev)

## Supported Protocols and Profiles

Protocols: L2CAP, RFCOMM, SDP, BNEP, ATT, SM (incl. LE Secure Connections).

Profiles: GAP, IOP, HFP, HSP, SPP, PAN, GATT.

Coming next: A2DP, AVRCP, HID, HOGP, BLE, and more.

For information on MFi/iAP2 support, please <a href="mailto:contact@bluekitchen-gmbh.com">contact us</a>.

## Evaluation Platforms

#### Embedded Platforms:      
Status               | Platform
--------------       | ------ 
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-ez430-rf2560-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-ez430-rf2560-master) | [EZ430-RF256x Bluetooth Evaluation Tool for MSP430](http://www.ti.com/tool/ez430-rf256x)
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-msp-exp430f5438-cc2564b-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-msp-exp430f5438-cc2564b-master) | [MSP430F5438 Experimenter Board for MSP430](http://www.ti.com/tool/msp-exp430f5438) with [Bluetooth CC2564 Module Evaluation Board](http://www.ti.com/tool/cc2564modnem)
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-msp430f5229lp-cc2564b-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-msp430f5229lp-cc2564b-master)     | [MSP-EXP430F5529LP LaunchPad](http://www.ti.com/ww/en/launchpad/launchpads-msp430-msp-exp430f5529lp.html#tabs) with [Bluetooth CC2564 Module Evaluation Board](http://www.ti.com/tool/cc2564modnem) and [EM Adapter BoosterPack](http://www.ti.com/tool/boost-ccemadapter) with additional 32768Hz quartz oscillator
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-stm32-f103rb-nucleo-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-stm32-f103rb-nucleo-master)         | [STM32 Nucleo development board NUCLEO-F103RB](http://www.st.com/web/catalog/tools/FM116/SC959/SS1532/LN1847/PF259875) with [Bluetooth CC2564 Module Evaluation Board](http://www.ti.com/tool/cc2564modnem) and [EM Adapter BoosterPack](http://www.ti.com/tool/boost-ccemadapter) with additional 32768Hz quartz oscillator
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-pic32-harmony-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-pic32-harmony-master)                     | [Microchip's PIC32 Bluetooth Audio Development Kit](http://www.microchip.com/Developmenttools/ProductDetails.aspx?PartNO=DV320032)
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-wiced-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-wiced-master)                                     | [RedBear Duo](https://github.com/redbear/WICED-SDK) with Broadcom BCM43438 A1


#### Other Platforms:     
Status               | Platform
--------------       | ------ 
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-posix-h4-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-posix-h4-master) | posix: Unix-based system talking to Bluetooth module via serial port   
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-libusb-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-libusb-master)   | libusb: Unix-based system talking via USB Bluetooth dongle
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-daemon-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-daemon-master)   | daemon: TCP and Unix domain named socket client-server architecture supporting multiple clients
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=java-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/java-master)          | java: Java wrapper for daemon 
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-ios-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-ios-master)      | iOS: daemon for iOS jailbreak devices, C client-server API
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-mtk-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-mtk-master)     | mtk: daemon for rooted Android devices, based on Mediatek MT65xx processor, Java and C client-server API
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-wiced-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-wiced-master)    | wiced: Broadcom platforms that support the WICED SDK

## Supported Chipsets

Chipset              | Type      | HCI Transport   | SCO over HCI (2) | BTstack folder | Comment 
-------------------- |-----------| ----------------|------------------|----------------|---------
Broadcom UART        | Dual mode | H4, H5          | No (didn't work) | bcm            | Max UART baudrate 3 mbps
Broadcom USB Dongles | Dual mode | USB             | No (didn't work) | bcm            | 
CSR UART             | Dual mode | H4, H5          | No (didn't work) | csr            | 
CSR USB Dongles      | Dual mode | USB             | Yes              | csr            |
EM 9301              | LE        | SPI             | n.a.             | em9301         | Custom HCI SPI implementation
Nordic nRF           | LE        | H4              | n.a.             |                | Requires custom HCI firmware
STM STLC2500D        | Classic   | H4              | No (didn't try)  | stlc2500d      | Custom deep sleep management not supported
Toshiba TC35661      | Dual mode | H4              | No (didn't try)  | tc3566         | HCI version not tested.
TI CC256x, WL183x    | Dual mode | H4, H5, eHCILL  | Yes              | cc256x         | Also WL185x, WL187x, and WL189x

[More infos on supported chipsets](https://bluekitchen-gmbh.com/btstack/chipsets/)

## Source Tree Overview
Path				| Description
--------------------|---------------
chipset             | Support for individual Bluetooth chipsets
doc                 | Sources for BTstack documentation
example             | Example applications available for all ports
platform            | Support for special OSs and/or MCU architectures
port                | Complete port for a MCU + Chipset combinations
src                 | Bluetooth stack implementation
test                | Unit and PTS tests
tool                | Helper tools for BTstack
