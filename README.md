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
* HFP
* HSP
* SPP              
* PAN              
* GATT             

Coming next: HID, HOGP, A2DP, and more.

## Evaluation Platforms

#### Embedded Platforms:      
Status               | Platform
--------------       | ------ 
![buildstatus](http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=platform-ez430-rf2560) | [EZ430-RF256x Bluetooth Evaluation Tool for MSP430](http://www.ti.com/tool/ez430-rf256x)  
![buildstatus](http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=platform-msp-exp430f5438-cc2564b) | [MSP430F5438 Experimenter Board for MSP430](http://www.ti.com/tool/msp-exp430f5438) with [Bluetooth CC2564 Module Evaluation Board](http://www.ti.com/tool/cc2564modnem) 
![buildstatus](http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=platform-msp430f5229lp-cc2564b) | [MSP-EXP430F5529LP LaunchPad](http://www.ti.com/ww/en/launchpad/launchpads-msp430-msp-exp430f5529lp.html#tabs) with [Bluetooth CC2564 Module Evaluation Board](http://www.ti.com/tool/cc2564modnem) and [EM Adapter BoosterPack](http://www.ti.com/tool/boost-ccemadapter) with additional 32768Hz quartz oscillator   
![buildstatus](http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=platform-stm32-f103rb-nucleo) | [STM32 Nucleo development board NUCLEO-F103RB](http://www.st.com/web/catalog/tools/FM116/SC959/SS1532/LN1847/PF259875) with [Bluetooth CC2564 Module Evaluation Board](http://www.ti.com/tool/cc2564modnem) and [EM Adapter BoosterPack](http://www.ti.com/tool/boost-ccemadapter) with additional 32768Hz quartz oscillator
![buildstatus](http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=platform-pic32-harmony) | [Microchip's PIC32 Bluetooth Audio Development Kit](http://www.microchip.com/Developmenttools/ProductDetails.aspx?PartNO=DV320032)          
 | [RedBear Duo](https://github.com/redbear/WICED-SDK) with Broadcom BCM43438 A1 


#### Other Platforms:     
Status               | Platform
--------------       | ------ 
![buildstatus](http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=platform-posix-stlc2500d)| posix: Unix-based system talking to Bluetooth module via serial port   
![buildstatus](http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=platform-libusb)| libusb: Unix-based system talking via USB Bluetooth dongle
![buildstatus](http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=platform-daemon)| daemon: TCP and Unix domain named socket client-server architecture supporting multiple clients
![buildstatus](http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=java)| java: Java wrapper for daemon 
![buildstatus](http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=platform-ios)| iOS: daemon for iOS jailbreak devices, C client-server API
![buildstatus](http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=platform-mtk)| mtk: daemon for rooted Android devices, based on Mediatek MT65xx processor, Java and C client-server API
 |wiced: Broadcom platforms that support the WICED SDK

## Supported Chipsets
Chipsets             | Status
--------------       | ------ 
TI CC256x, WL183x    | complete incl. eHCIll support and SCO-over-HCI (chipset-cc256x)
CSR 8x10, 8x11       | H4 only (chipset-csr), SCO-over-HCI missing
STM STLC2500D        | working, no support for custom deep sleep management (chipset-stlc2500d)
TC35661              | working, BLE patches missing (chipset-tc3566x)
EM 9301 (LE-only)    | working, used on Arduino Shield (chipset-em9301)
CSR USB Dongles      | complete, incl. SCO-over-HCI 
Broadcom USB Dongles | complete, SCO-over-HCI not working
Broadcom BCM43438    | complete. UART baudrate limited to 3 mbps, SCO-over-HCI not working

## Source Tree Overview
Path				| Description
--------------------|---------------
binding	            | Language bindings for BTstack, e.g. Java client/server
chipset             | Support for individual Bluetooth chipsets
doc                 | Sources for BTstack documentation
example             | Example applications available for different ports
platform            | Support for special OSs and/or MCU architectures
port                | Complete port for a MCU + Chipset combinations
src                 | Bluetooth stack implementation
test                | Unit and PTS tests
tool                | Helper tools for BTstack

## Discussion and Community Support
[BTstack Google Group](http://groups.google.com/group/btstack-dev)
