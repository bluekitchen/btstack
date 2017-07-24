**_Note: Major API Changes. For older projects, you may use the [v0.9 branch](https://github.com/bluekitchen/btstack/tree/v0.9).
Please see [Migration notes](https://github.com/bluekitchen/btstack/blob/master/doc/manual/docs/appendix/migration.md)_**

# Welcome to BTstack

BTstack is [BlueKitchen's](http://bluekitchen-gmbh.com) implementation of the official Bluetooth stack.
It is well suited for small, resource-constraint devices
such as 8 or 16 bit embedded systems as it is highly configurable and comes with an ultra small memory footprint.
A minimal configuration for an SPP server on a MSP430 can run in 32 kB FLASH and only 4 kB of RAM.

Targeting a variety of platforms is as simple as providing the necessary UART, CPU, and CLOCK implementations. BTstack is currently capable of connecting to Bluetooth-modules via: (H2) HCI USB, (H4) HCI UART + TI's eHCILL, and (H5) HCI Three-Wire UART.

On smaller embedded systems, a minimal run loop implementation allows to use BTstack without a Real Time OS (RTOS).
If a RTOS is already provided, BTstack can be integrated and run as a single thread.

On larger systems, BTstack provides a daemon that connects to a Bluetooth module.
Multiple applications can communicate with this daemon over different inter-process communication methods.

BTstack supports the Central and the Peripheral Role of Bluetooth 4.2 Low Energy specification incl. LE Secure Connections, LE Data Channels, and LE Data Length Extension. It can be configured to run as either single-mode stack or a dual-mode stack.

BTstack is free for non-commercial use. However, for commercial use, <a href="mailto:contact@bluekitchen-gmbh.com">tell us</a> a bit about your project to get a quote.

**Documentation:** [HTML](http://bluekitchen-gmbh.com/btstack/), [PDF](http://bluekitchen-gmbh.com/btstack_master.pdf)

**Discussion and Community Support:** [BTstack Google Group](http://groups.google.com/group/btstack-dev)

### Supported Protocols and Profiles

**Protocols:** L2CAP, RFCOMM, SDP, BNEP, ATT, SM (incl. LE Secure Connections).

**Profiles** GAP, IOP, HFP, HSP, SPP, PAN, GATT.

**Coming next** A2DP, AVRCP, HID, HOGP, BLE Mesh, and more.

It has been qualified with the the Bluetooth SIG for GAP, IOP, HFP, HSP, SPP, PAN profiles and
GATT, SM of the Bluetooth 4.2 LE Central and Peripheral roles (QD ID 25340). For information on MFi/iAP2 support, please <a href="mailto:contact@bluekitchen-gmbh.com">contact us</a>.


## Evaluation Platforms

Build Status               | Port | Platform
---------------------| -----| ------
No build server | [esp32](https://github.com/bluekitchen/btstack/tree/master/port/esp32) | [Espressif ESP32](http://www.espressif.com/products/hardware/esp32/overview) 2.4 GHz Wi-Fi and Bluetooth Dual-Mode combo chip using [FreeRTOS](http://www.freertos.org)
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-ez430-rf2560-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-ez430-rf2560-master) | [ez430-rf2560](https://github.com/bluekitchen/btstack/tree/master/port/ez430-rf2560) | [EZ430-RF256x Bluetooth Evaluation Tool for MSP430](http://www.ti.com/tool/ez430-rf256x)
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-msp-exp430f5438-cc2564b-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-msp-exp430f5438-cc2564b-master) | [msp-exp430f5438-cc2564b](https://github.com/bluekitchen/btstack/tree/master/port/msp-exp430f5438-cc2564b) |[MSP430F5438 Experimenter Board for MSP430](http://www.ti.com/tool/msp-exp430f5438) with [Bluetooth CC2564 Module Evaluation Board](http://www.ti.com/tool/cc2564modnem)
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-msp430f5229lp-cc2564b-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-msp430f5229lp-cc2564b-master)     | [msp430f5529lp-cc2564b](https://github.com/bluekitchen/btstack/tree/master/port/msp430f5229lp-cc2564b) | [MSP-EXP430F5529LP LaunchPad](http://www.ti.com/ww/en/launchpad/launchpads-msp430-msp-exp430f5529lp.html#tabs) with [Bluetooth CC2564 Module Evaluation Board](http://www.ti.com/tool/cc2564modnem) and [EM Adapter BoosterPack](http://www.ti.com/tool/boost-ccemadapter) with additional 32768Hz quartz oscillator
No build server | [max32630-fthr](https://github.com/bluekitchen/btstack/tree/master/port/max32630-fthr) | [MAX32630FTHR ARM Cortex M4F Board](https://www.maximintegrated.com/en/products/digital/microcontrollers/MAX32630FTHR.html) with onboard [Panasonic PAN1326 module](https://na.industrial.panasonic.com/products/wireless-connectivity/bluetooth/multi-mode/series/pan13261316-series/CS467) containing  [TI CC2564B Bluetooth controller](http://www.ti.com/product/cc2564)
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-stm32-f103rb-nucleo-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-stm32-f103rb-nucleo-master)       | [stm32-f103rb-nucleo](https://github.com/bluekitchen/btstack/tree/master/port/stm32-f103rb-nucleo) | [STM32 Nucleo development board NUCLEO-F103RB](http://www.st.com/web/catalog/tools/FM116/SC959/SS1532/LN1847/PF259875) with [Bluetooth CC2564 Module Evaluation Board](http://www.ti.com/tool/cc2564modnem) and [EM Adapter BoosterPack](http://www.ti.com/tool/boost-ccemadapter) with additional 32768Hz quartz oscillator
No build Server | [stm32-f4discovery-cc2564b](https://github.com/bluekitchen/btstack/tree/master/port/stm32-f4discovery-cc256x) | [STM32 F4 Discovery Board](http://www.st.com/en/evaluation-tools/stm32f4discovery.html) with [CC256xEM Bluetooth Adapter Kit for ST](https://store.ti.com/CC256XEM-STADAPT-CC256xEM-Bluetooth-Adapter-Kit-P45158.aspx) and [CC2564B Dual-mode Bluetooth® Controller Evaluation Module](https://store.ti.com/cc2564modnem.aspx)
No build Server | [stm32-l053rb-em9304](https://github.com/bluekitchen/btstack/tree/master/port/stm32-l053rb-em9304) | EM9304 DVK: [STM32 Nucleo development board NUCELO-L053R](http://www.st.com/en/evaluation-tools/nucleo-l053r8.html) with [EM9304  Bluetooth Controller](http://www.emmicroelectronic.com/products/wireless-rf/standard-protocols/em9304)
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-pic32-harmony-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-pic32-harmony-master)                |  [pic32-harmony](https://github.com/bluekitchen/btstack/tree/master/port/pic32-harmony)  | [Microchip's PIC32 Bluetooth Audio Development Kit](http://www.microchip.com/Developmenttools/ProductDetails.aspx?PartNO=DV320032)
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-wiced-h4-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-wiced-h4-master)                           | [wiced-h4](https://github.com/bluekitchen/btstack/tree/master/port/wiced-h4) | Broadcom platforms that support the WICED SDK via H4 UART, e.g. [RedBear Duo](https://github.com/redbear/WICED-SDK) with Broadcom BCM43438 A1
No build server | [wiced-h5](https://github.com/bluekitchen/btstack/tree/master/port/wiced-h5) | Broadcom platforms that support the WICED SDK via H5 UART

#### Other Platforms:     
Status             | Port  | Platform
-------------------| ------|---------
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-posix-h4-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-posix-h4-master) | [posix-h4](https://github.com/bluekitchen/btstack/tree/master/port/posix-h4) | Unix-based system connected to Bluetooth module via H4 over serial port   
No build server | [posix-h4-da14581](https://github.com/bluekitchen/btstack/tree/master/port/posix-h4-da14581) | Unix-based system connected to Dialog Semiconductor DA14581 via H4 over serial port   
No build server | [posix-h5](https://github.com/bluekitchen/btstack/tree/master/port/posix-h5) | Unix-based system connected to Bluetooth module via H5 over serial port   
No build server | [posix-h5-bcm](https://github.com/bluekitchen/btstack/tree/master/port/posix-h5) | Unix-based system connected to Broadcom/Cypress Bluetooth module via H5 over serial port   
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-libusb-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-libusb-master) | [libusb](https://github.com/bluekitchen/btstack/tree/master/port/libusb) | Unix-based system with dedicated USB Bluetooth dongle
No build server | [windows-h4](https://github.com/bluekitchen/btstack/tree/master/port/windows-h4) | Win32-based system connected to Bluetooth module via serial port   
No build server | [windows-winusb](https://github.com/bluekitchen/btstack/tree/master/port/windows-winusb) | Win32-based system with dedicated USB Bluetooth dongle
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-daemon-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-daemon-master)     | [daemon](https://github.com/bluekitchen/btstack/tree/master/port/daemon) | TCP and Unix domain named socket client-server architecture supporting multiple clients
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=java-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/java-master)                   | [java](https://github.com/bluekitchen/btstack/tree/master/platform/daemon/binding/java) | Java wrapper for daemon
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-mtk-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-mtk-master)           | [mtk](https://github.com/bluekitchen/btstack/tree/master/port/mtk) | daemon for rooted Android devices, based on Mediatek MT65xx processor, Java and C client-server API
[<img src="http://buildbot.bluekitchen-gmbh.com/btstack/badge.png?builder=port-ios-master">](https://buildbot.bluekitchen-gmbh.com/btstack/builders/port-ios-master)           | [ios](https://github.com/bluekitchen/btstack/tree/master/port/ios) | daemon for iOS jailbreak devices, C client-server API
No build server | [freertos](https://github.com/bluekitchen/btstack/tree/master/platform/freertos) | [FreeRTOS](http://www.freertos.org): Run BTstack on a dedicated thread, not thread-safe.

## Supported Chipsets

Chipset                      | Type      | HCI Transport   | SCO over HCI (2) | BTstack folder | Comment
---------------------------- |-----------| ----------------|------------------|----------------|---------
Broadcom UART                | Dual mode | H4, H5          | Probably         | bcm            | Max UART baudrate 2 mbps
Broadcom USB Dongles         | Dual mode | USB             | Yes              | bcm            |
CSR UART                     | Dual mode | H4, H5, BCSP    | No (didn't work) | csr            |
CSR USB Dongles              | Dual mode | USB             | Yes              | csr            |
Dialog Semiconductor DA14581 | LE        | H4, SPI         | n.a.             | da14581        | Official HCI firmware used
EM 9301, 9304                | LE        | SPI             | n.a.             | em9301         | Custom HCI SPI implementation
Nordic nRF                   | LE        | H4              | n.a.             |                | Requires custom HCI firmware
STM STLC2500D                | Classic   | H4              | No (didn't try)  | stlc2500d      | Custom deep sleep management not supported
Toshiba TC35661              | Dual mode | H4              | No (didn't try)  | tc3566         | HCI version not tested.
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
