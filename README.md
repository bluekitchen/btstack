# Welcome to BTstack

BTstack is [BlueKitchen's](https://bluekitchen-gmbh.com) implementation of the official Bluetooth stack.
It is well suited for small, resource-constraint devices
such as 8 or 16 bit embedded systems as it is highly configurable and comes with an ultra small memory footprint.

Targeting a variety of platforms is as simple as providing the necessary UART, CPU, and CLOCK implementations. BTstack is currently capable of connecting to Bluetooth-modules via: (H2) HCI USB, (H4) HCI UART + TI's eHCILL, and (H5) HCI Three-Wire UART.

On smaller embedded systems, a minimal run loop implementation allows to use BTstack without a Real Time OS (RTOS).
If a RTOS is already provided, BTstack can be integrated and run as a single thread.

On larger systems, BTstack provides a server that connects to a Bluetooth module.
Multiple applications can communicate with this server over different inter-process communication methods. As sockets are used for client/server communication, it's easy to interact via higher-level level languages, e.g. there's already a Java binding for use in desktop environments.

BTstack supports the Central and the Peripheral Role of Bluetooth 5 Low Energy specification incl. LE Secure Connections, LE Data Channels, and LE Data Length Extension. It can be configured to run as either single-mode stack or a dual-mode stack.

BTstack is free for non-commercial use. However, for commercial use, <a href="mailto:contact@bluekitchen-gmbh.com">tell us</a> a bit about your project to get a quote.

**Documentation:** [HTML](http://bluekitchen-gmbh.com/btstack/), [PDF](http://bluekitchen-gmbh.com/btstack.pdf)

**Third-party libraries (FOSS):** [List of used libraries and their licenses](https://github.com/bluekitchen/btstack/blob/master/3rd-party/README.md)

**Discussion and Community Support:** [BTstack Google Group](https://groups.google.com/group/btstack-dev)


### Supported Protocols and Profiles

**Protocols:** L2CAP (incl. Enhanced Retransmission Mode and LE Data Channels), RFCOMM, SDP, BNEP, AVDTP, AVCTP, ATT, SM (incl. LE Secure Connections and Cross-transport key derivation).

**Profiles:** A2DP, AVRCP incl. Browsing, GAP, GATT, HFP, HID, HSP, IOP, SPP, PAN, PBAP Client.

**GATT Service Servers:** Battery (BAS), Cycling Power (CPS), Cycling Speed and Cadence (CSCS), Device Information (DID), Heart Rate (HRS), HID over GATT (HIDS) Device , Mesh Provisioning, Mesh Proxy, Nordic SPP, Scan Parameters (SCPS), u-Blox SPP. 

**GATT Service Clients:**: ANCS, Battery (BAS), Device Information (DID), HID-over-GATT (HOGP) Host, Scan Parameters (SCPP).

GATT Services are in general easy to implement and require short development time. For more GATT Services please contact us, or follow the [implementation guidelines](https://bluekitchen-gmbh.com/btstack/profiles/#gatt-generic-attribute-profile).  

**In Development:** BLE Mesh and more.

It has been qualified with the Bluetooth SIG (QDID 166433) for A2DP 1.3.2, AVRCP 1.6.2, DID 1.3, HFP 1.8, HSP 1.2, PAN 1.0, PBAP Client 1.2, SPP 1.2 BR/EDR profiles, 
BAS 1.0, CPP 1.1, CPS 1.1, CSCP 1.0, CSCS 1.0, DIS 1.1, HIDS 1.0, HOGP 1.0, HRP 1.0, HRS 1.0, SCPP 1.0, SCPS 1.0 GATT profiles as well as and GAP, GATT, IOP, SM of the Bluetooth Core 5.2 specification. For information on MFi/iAP2 support, please <a href="mailto:contact@bluekitchen-gmbh.com">contact us</a>.

## Evaluation Platforms

#### Embedded Platforms:
Build Status               | Port | Platform
---------------------| -----| ------
[<img src="https://buildbot.bluekitchen-gmbh.com/btstack/badges/port-esp32-master.svg">](https://buildbot.bluekitchen-gmbh.com/btstack/#/builders/port-esp32-master) | [esp32](https://github.com/bluekitchen/btstack/tree/develop/port/esp32) | [Espressif ESP32](https://www.espressif.com/products/hardware/esp32/overview) 2.4 GHz Wi-Fi and Bluetooth Dual-Mode combo chip using [FreeRTOS](https://www.freertos.org)
[<img src="https://buildbot.bluekitchen-gmbh.com/btstack/badges/port-max32630-fthr-master.svg">](https://buildbot.bluekitchen-gmbh.com/btstack/#/builders/port-max32630-fthr) | [max32630-fthr](https://github.com/bluekitchen/btstack/tree/develop/port/max32630-fthr) | [MAX32630FTHR ARM Cortex M4F Board](https://www.maximintegrated.com/en/products/digital/microcontrollers/MAX32630FTHR.html) with onboard [Panasonic PAN1326 module](https://na.industrial.panasonic.com/products/wireless-connectivity/bluetooth/multi-mode/series/pan13261316-series/CS467) containing  [TI CC2564B Bluetooth controller](https://www.ti.com/product/cc2564)
[<img src="https://buildbot.bluekitchen-gmbh.com/btstack/badges/port-msp432p401lp-cc256x-master.svg">](https://buildbot.bluekitchen-gmbh.com/btstack/#/builders/port-msp432p401lp-cc256x-master)     | [msp432p401lp-cc256x](https://github.com/bluekitchen/btstack/tree/develop/port/msp432p401lp-cc256x) | [TI MSP432P401R LaunchPad](https://www.ti.com/tool/MSP-EXP432P401R) with [CC2564C Dual-mode Bluetooth® Controller Evaluation Module](https://store.ti.com/CC256XCQFN-EM-CC2564C-Dual-Mode-Bluetooth-Controller-Evaluation-Module-P51277.aspx) and [EM Adapter BoosterPack](https://www.ti.com/tool/boost-ccemadapter) with additional 32768Hz quartz oscillator
No build server | [renesas-tb-s1ja-cc256x](https://github.com/bluekitchen/btstack/tree/develop/port/renesas-tb-s1ja-cc256x) | [TB-S1JA Target Board Kit](https://www.renesas.com/eu/en/products/synergy/hardware/kits/tb-s1ja.html) with  with [Dual-mode Bluetooth® CC2564 evaluation board](https://www.ti.com/tool/CC256XQFNEM) and [EM Adapter BoosterPack](https://www.ti.com/tool/boost-ccemadapter) with additional 32768Hz quartz oscillator
[<img src="https://buildbot.bluekitchen-gmbh.com/btstack/badges/port-samv71-xplained-atwilc3000-master.svg">](https://buildbot.bluekitchen-gmbh.com/btstack/#/builders/port-samv71-xplained-atwilc3000-master) | [samv71-xplained-atwilc3000](https://github.com/bluekitchen/btstack/tree/develop/port/samv71-xplained-atwilc3000) | [SAMV71 Ultra Xplained Ultra](https://www.microchip.com/DevelopmentTools/ProductDetails/PartNO/ATSAMV71-XULT) evaluation kit with [ATWILC3000 SHIELD](https://www.microchip.com/DevelopmentTools/ProductDetails/ATWILC3000-SHLD)
[<img src="https://buildbot.bluekitchen-gmbh.com/btstack/badges/port-stm32-f4discovery-cc256x-master.svg">](https://buildbot.bluekitchen-gmbh.com/btstack/#/builders/port-stm32-f4discovery-cc256x-master) | [stm32-f4discovery-cc2564b](https://github.com/bluekitchen/btstack/tree/develop/port/stm32-f4discovery-cc256x) | [STM32 F4 Discovery Board](https://www.st.com/en/evaluation-tools/stm32f4discovery.html) with [CC256xEM Bluetooth Adapter Kit for ST](https://store.ti.com/CC256XEM-STADAPT-CC256xEM-Bluetooth-Adapter-Kit-P45158.aspx) and [CC2564B Dual-mode Bluetooth® Controller Evaluation Module](https://store.ti.com/cc2564modnem.aspx)
[<img src="https://buildbot.bluekitchen-gmbh.com/btstack/badges/port-stm32-l073rz-nucleo-em9304-master.svg">](https://buildbot.bluekitchen-gmbh.com/btstack/#/builders/port-stm32-l073rz-nucleo-em9304)  | [stm32-l073rz-nucleo-em9304](https://github.com/bluekitchen/btstack/tree/develop/port/stm32-l073rz-nucleo-em9304) | EM9304 DVK: [STM32 Nucleo development board NUCELO-L73RZ](https://www.st.com/en/evaluation-tools/nucleo-l073rz.html) with [EM9304 Bluetooth Controller](https://www.emmicroelectronic.com/product/standard-protocols/em9304)
No build server | [stm32-wb55xx-nucleo-freertos](https://github.com/bluekitchen/btstack/tree/develop/port/stm32-wb55xx-nucleo-freertos) | [P-NUCLEO-WB55 kit](https://www.st.com/en/evaluation-tools/p-nucleo-wb55.html)
[<img src="https://buildbot.bluekitchen-gmbh.com/btstack/badges/port-wiced-h4-master.svg">](https://buildbot.bluekitchen-gmbh.com/btstack/#/builders/port-wiced-h4-master)                           | [wiced-h4](https://github.com/bluekitchen/btstack/tree/develop/port/wiced-h4) | Broadcom/Cypress platforms that support the WICED SDK via H4 UART, e.g. [RedBear Duo](https://www.seeedstudio.com/RedBear-DUO-Wi-Fi-BLE-IoT-Board-p-2635.html) (BCM43438 A1), [Inventek Systems ISM4334x](https://www.inventeksys.com/wifi/wifi-modules/ism4343-wmb-l151/) (BCM43438 A1), [Inventek Systems ISM4343](https://www.inventeksys.com/products-page/wifi-modules/serial-wifi/ism43341-m4g-l44-cu-embedded-serial-to-wifi-ble-nfc-module/) (BCM43340)


#### Other Platforms:     
Status             | Port  | Platform
-------------------| ------|---------
[<img src="https://buildbot.bluekitchen-gmbh.com/btstack/badges/port-libusb-master.svg">](https://buildbot.bluekitchen-gmbh.com/btstack/#/builders/port-libusb-master) | [libusb](https://github.com/bluekitchen/btstack/tree/master/port/libusb) | Unix-based system with dedicated USB Bluetooth dongle
No build server | [libusb-intel](https://github.com/bluekitchen/btstack/tree/master/port/libusb-intel) | Unix-based system with Intel Wireless 8260/8265 Controller
[<img src="https://buildbot.bluekitchen-gmbh.com/btstack/badges/port-posix-h4-master.svg">](https://buildbot.bluekitchen-gmbh.com/btstack/#/builders/port-posix-h4-master) | [posix-h4](https://github.com/bluekitchen/btstack/tree/master/port/posix-h4) | Unix-based system connected to Bluetooth module via H4 over serial port   
No build server | [posix-h4-da14581](https://github.com/bluekitchen/btstack/tree/master/port/posix-h4-da14581) | Unix-based system connected to Dialog Semiconductor DA14581 via H4 over serial port
No build server | [posix-h4-da14585](https://github.com/bluekitchen/btstack/tree/master/port/posix-h4-da14585) | Unix-based system connected to Dialog Semiconductor DA14585 via H4 over serial port   
No build server | [posix-h5](https://github.com/bluekitchen/btstack/tree/master/port/posix-h5) | Unix-based system connected to Bluetooth module via H5 over serial port   
No build server | [qt-h4](https://github.com/bluekitchen/btstack/tree/master/port/qt-h4) | Unix- or Win32-based [Qt application](https://qt.io) connected to Bluetooth module via H4 over serial port 
No build server | [qt-usb](https://github.com/bluekitchen/btstack/tree/master/port/qt-usb) | Unix- or Win32-based [Qt application](https://qt.io) with dedicated USB Bluetooth dongle
No build server | [windows-h4](https://github.com/bluekitchen/btstack/tree/master/port/windows-h4) | Win32-based system connected to Bluetooth module via serial port   
No build server | [windows-h4-da14585](https://github.com/bluekitchen/btstack/tree/master/port/windows-h4-da14585) | Win32-based system connected to Dialog Semiconductor DA14585 via H4 over serial port   
No build server | [windows-winusb](https://github.com/bluekitchen/btstack/tree/master/port/windows-winusb) | Win32-based system with dedicated USB Bluetooth dongle
No build server | [windows-winusb-intel](https://github.com/bluekitchen/btstack/tree/master/port/windows-winusb-intel) | Win32-based system with Intel Wireless 8260/8265 Controller
No build server | [raspi](https://github.com/bluekitchen/btstack/tree/master/port/raspi) | Raspberry Pi 3 or Raspberry Pi Zero W with built-in BCM4343 Bluetooth/Wifi Controller
[<img src="https://buildbot.bluekitchen-gmbh.com/btstack/badges/port-daemon-master.svg">](https://buildbot.bluekitchen-gmbh.com/btstack/#/builders/port-daemon-master)     | [daemon](https://github.com/bluekitchen/btstack/tree/master/port/daemon) | TCP and Unix domain named socket client-server architecture supporting multiple clients
[<img src="https://buildbot.bluekitchen-gmbh.com/btstack/badges/java-master.svg">](https://buildbot.bluekitchen-gmbh.com/btstack/#/builders/java-master)                   | [java](https://github.com/bluekitchen/btstack/tree/master/platform/daemon/binding/java) | Java wrapper for daemon
[<img src="https://buildbot.bluekitchen-gmbh.com/btstack/badges/port-mtk-master.svg">](https://buildbot.bluekitchen-gmbh.com/btstack/#/builders/port-mtk-master)           | [mtk](https://github.com/bluekitchen/btstack/tree/master/port/mtk) | daemon for rooted Android devices, based on Mediatek MT65xx processor, Java and C client-server API
No build server | [freertos](https://github.com/bluekitchen/btstack/tree/master/platform/freertos) | [FreeRTOS](https://www.freertos.org): Run BTstack on a dedicated thread, not thread-safe.

## Supported Chipsets

Chipset                      | Type      | HCI Transport   | SCO over HCI     | BTstack folder | Comment
---------------------------- |-----------| ----------------|------------------|----------------|---------
Atmel ATWILC3000             | LE        | H4              | n.a.             | atwilc3000     | Firmware size: 60 kB
Broadcom UART                | Dual mode | H4, H5          | Probably         | bcm            | Max UART baudrate 2 mbps
Broadcom USB Dongles         | Dual mode | USB             | Yes              | bcm            |
CSR UART                     | Dual mode | H4, H5, BCSP    | No (didn't work) | csr            |
CSR USB Dongles              | Dual mode | USB             | Yes              | csr            |
Cypress CYW20704             | Dual mode | H4, H5, USB     | Probably         | bcm            |
Cypress CYW20819             | Dual mode | H4, H5, USB     | Probably         | bcm            | Keep CTS high during power cycle
Cypress CYW43xxx             | Dual mode + Wifi | H4, H5   | Don't know       | bcm            | Bluetooth + Wifi Combo Controller
Cypress PSoC 4               | LE        | H4              | n.a.             |                | HCI Firmware part of PSoC Creator kit examples
Dialog Semiconductor DA14581, DA14585 | LE      | H4, SPI  | n.a.             | da14581        | Official HCI firmware used
Dialog Semiconductor DA1469x | LE        | H4, SPI         | n.a              |                | HCI Firmware part of DA1469x SDK
Espressif ESP32              | Dual mode | VHCI            | Not yet          |                | SoC with Bluetooth and Wifi
EM 9301, 9304                | LE        | SPI             | n.a.             | em9301         | Custom HCI SPI implementation
Intel Dual Wireless 3165, 8260, 8265 | Dual mode | USB           | Probably         | intel          | Firmware size: 400 kB 
Nordic nRF                   | LE        | H4              | n.a.             |                | Requires custom HCI firmware
Renesas RX23W                | LE        | H4              | n.a.             |                | HCI Firmware part of BTTS
STM STLC2500D                | Classic   | H4              | No (didn't try)  | stlc2500d      | Custom deep sleep management not supported
STM32-WB5x                   | LE        | VHCI            | n.a.             |                | SoC with multi-protocol Radio co-processor
Toshiba TC35661              | Dual mode | H4              | No               | tc3566         |
TI CC256x, WL183x            | Dual mode | H4, H5, eHCILL  | Yes              | cc256x         | Also WL185x, WL187x, and WL189x

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
