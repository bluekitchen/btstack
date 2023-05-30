#

In this chapter, we first explain how Bluetooth chipsets are connected physically and then provide information about popular Bluetooth chipset and their use with BTstack.

## HCI Interface

The communication between a Host (a computer or an MCU) and a Host Controller (the actual Bluetooth chipset) follows the Host Controller Interface (HCI), see {@fig:HostChipsetConnection}. HCI defines how commands, events, asynchronous and synchronous data packets are exchanged. Asynchronous packets (ACL) are used for data transfer, while synchronous packets (SCO) are used for Voice with the Headset and the Hands-Free Profiles.

![Host Controller to Host connection](../doc/manual/docs-template/picts/host_chipset_connection.png){#fig:HostChipsetConnection}

### HCI H2
On desktop-class computers incl. laptops, USB is mainly used as HCI transport layer. For USB Bluetooth chipsets, there is little variation: most USB dongles on the market currently contain a Broadcom BCM20702 or a CSR 851x chipset. It is also called H2.

On embedded systems, UART connections are used instead, although USB could be used as well.

For UART connections, different transport layer variants exist.

### HCI H4
The most common one is the official "UART Transport", also called H4. It requires hardware flow control via the CTS/RTS lines and assumes no errors on the UART lines.

### HCI H5
The "Three-Wire UART Transport", also called H5, makes use of the SLIP protocol to transmit a packet and can deal with packet loss and bit-errors by retransmission. While it is possible to use H5 really with "three wires" without hardware handshake, we recommend to use a full UART with hardware handshake. If your design lacks the hardware handshake, H5 is your only option.

### BCSP
The predecessor of H5. The main difference to H5 is that Even Parity is used for BCSP. To use BCSP with BTstack, you use the H5 transport and can call *hci_transport_h5_enable_bcsp_mode*

### eHCILL
Finally, Texas Instruments extended H4 to create the "eHCILL transport" layer that allows both sides to enter sleep mode without loosing synchronisation. While it is easier to implement than H5, it it is only supported by TI chipsets and cannot handle packet loss or bit-errors.

### H4 over SPI
Chipsets from Dialog Semiconductor and EM Marin allow to send H4 formatted HCI packets via SPI. SPI has the benefit of a simpler implementation for both Host Controller and Host as it does not require an exact clock. The SPI Master, here the Host, provides the SPI Clock and the SPI Slave (Host Controller) only has to read and update it's data lines when the clock line changes. The EM9304 supports an SPI clock of up to 8 Mhz. However, an additional protocol is needed to let the Host know when the Host Controller has HCI packet for it. Often, an additional GPIO is used to signal this.

### HCI Shortcomings

Unfortunately, the HCI standard misses a few relevant details:

  * For UART based connections, the initial baud rate isn't defined but most Bluetooth chipsets use 115200 baud. For better throughput, a higher baud rate is necessary, but there's no standard HCI command to change it. Instead, each vendor had to come up with their own set of vendor-specific commands. Sometimes, additional steps, e.g. doing a warm reset, are necessary to activate the baud rate change as well.

  * Some Bluetooth chipsets don't have a unique MAC address. On start, the MAC address needs to be set, but there's no standard HCI command to set it.

  * SCO data for Voice can either be transmitted via the HCI interface or via an explicit PCM/I2S interface on the chipset. Most chipsets default to the PCM/I2S interface. To use it via USB or for Wide-Band Speech in the Hands-Free Profile, the data needs to be delivered to the host MCU. Newer Bluetooth standards define a HCI command to configure the SCO routing, but it is not implemented in the chipsets we've tested so far. Instead, this is configured in a vendor-specific way as well.

  * In addition, most vendors allow to patch or configure their chipsets at run time by sending custom commands to the chipset. Obviously, this is also vendor dependent.

## Documentation and Support
The level of developer documentation and support varies widely between the various Bluetooth chipset providers.

From our experience, only Texas Instruments and EM Microelectronics provide all relevant information directly on their website. 
Nordic Semiconductor does not officially have Bluetooth chipsets with HCI interface, but their documentation on the nRF5 series is complete and very informative.
TI and Nordic also provide excellent support via their respective web forum.

Infineon acquired Cypress Semiconductor Corporation in 2020, which acquired the Bluetooth + Wifi division of Broadcom in 2016 provides
support via their Community Forum. In addition, firmware updates (PatchRAM files) for Bluetooth + Wifi controllers 
are available via [Murata's Cypress GitHub](https://github.com/murata-wireless/cyw-bt-patch).

CSR, which has been acquired by Qualcomm, provides all relevant information on their Support website after signing an NDA.


## Chipset Overview

| Chipset                              | Type             | HCI Transport  | BD_ADDR (1)  | SCO over HCI (2) | LE DLE     | Multiple LE Roles (3) | Classic SC (4)    | LE Addr Resolution | BTstack folder | Comment                                          |
|--------------------------------------|------------------|----------------|--------------|------------------|------------|-----------------------|-------------------|--------------------|----------------|--------------------------------------------------|
| Atmel ATWILC3000                     | LE               | H4             | Yes          | n.a              | No         | No                    | n.a.              | Don't know         | atwilc3000     | BLE Firmware size: 60 kB                         |
| Broadcom UART                        | Dual mode        | H4, H5         | Rarely       | Partially (2)    | No         | Maybe (3)             | 43438: Yes        |                    | bcm            | Max UART baudrate 2 mbps                         |
| Broadcom USB Dongles                 | Dual mode        | USB            | Yes          | Yes              | No         | No                    | BCM20702: No      |                    | bcm            |                                                  |
| CSR UART                             | Dual mode        | H4, H5, BCSP   | Rarely       | Partially (2)    | No         | No                    | CSR8811:  No      |                    | csr            |                                                  |
| CSR USB Dongles                      | Dual mode        | USB            | Mostly       | Yes              | No         | No                    | CSR8510:  No      |                    | csr            |                                                  |
| Infineon CYW207xx                    | Dual mode        | H4, H5, USB    | Don't know   | Partially (2)    | Yes        | Yes                   | Yes               | Yes                | bcm            |                                                  |
| Infineon CYW208xx                    | Dual mode        | H4, H5, USB    | Don't know   | Partially (2)    | Yes        | Yes                   | Yes               | Don't know         | bcm            | Keep CTS high during power cycle                 |
| Infineon CYW43xxx                    | Dual mode + Wifi | H4, H5         | Don't know   | Partially (2)    | Don't know | On newer versions     | On wewer versions | On newer versions  | bcm            | Bluetooth + Wifi Combo Controller                |
| Infineon CYW5557x                    | Dual mode + Wifi | H4, H5         | No           | Yes              | Yes        | Yes                   | Yes               | Yes                | bcm            | Bautobaud-mode needed, see posix-h4-bcm          |
| Cypress PSoC 4                       | LE               | H4             | Don't know   | n.a.             | Yes        | Don't know            | n.a.              | Don't know         |                | HCI Firmware part of PSoC Creator kits examples  |
| Dialog DA14531                       | LE               | H4             | No           | n.a.             | Yes        | Yes                   | n.a.              | Don't know         | da145xx        | Official HCI firmware included in BTstack        |
| Dialog DA14581                       | LE               | H4, SPI        | No           | n.a.             | No         | No                    | n.a.              | Don't know         | da145xx        | Official HCI firmware included in BTstack        |
| Dialog DA14585                       | LE               | H4, SPI        | No           | n.a.             | Yes        | Yes                   | n.a.              | Yes                | da145xx        | Official HCI firmware included in BTstack        |
| Dialog DA1469x                       | LE               | H4, SPI        | No           | n.a.             | Yes        | Yes                   | n.a.              | Yes                | da145xx        | HCI Firmware part of DA1469x SDK                 |
| Espressif ESP32                      | Dual mode + Wifi | VHCI, H4       | Yes          | Yes              | Yes        | Yes                   | Yes               | Don't know         |                | SoC with Bluetooth and Wifi                      |
| Espressif ESP32-S3,C3                | LE + Wifi        | VHCI, H4       | Yes          | No               | Yes        | Yes                   | Yes               | Yes                |                | SoC with Bluetooth and Wifi                      |
| EM 9301                              | LE               | SPI, H4        | No           | n.a.             | No         | No                    | n.a.              | Don't know         | em9301         | Custom HCI SPI implementation                    |
| EM 9304                              | LE               | SPI, H4        | Yes          | n.a.             | Yes        | Yes                   | n.a.              | Don't know         | em9301         | Custom HCI SPI implementation                    |
| EM 9305                              | LE               | SPI, H4        | Yes          | n.a.             | Yes        | Yes                   | n.a.              | Yes                | em9301         | Custom HCI SPI implementation                    |
| Intel Dual Wireless 3165, 8260, 8265 | Dual mode        | USB            | Yes          | Probably         | Don't know | Don't know            | Don't know        | Don't know         | intel          | Firmware size: 400 kB                            |
| Nordic nRF                           | LE               | H4             | Fixed Random | n.a.             | Yes        | Yes                   | n.a.              | Yes                |                | Requires HCI firmware                            |
| NXP 88W8997                          | Dual mode        | H4             | Yes          | Partially(2)     | Yes        | Yes                   | No                | Yes                | nxp            | Requires initial firmware                        |
| NXP IW416                            | Dual mode        | H4             | Yes          | No               | Yes        | Yes                   | No                | Yes                | nxp            | Requires initial firmware                        |
| NXP IW61x                            | Dual mode        | H4             | Yes          | Partially(2)     | Yes        | Yes                   | No                | Yes                | nxp            | Requires initial firmware                        |
| STM STLC2500D                        | Classic          | H4             | No           | Don't know       | n.a        | n.a.                  | No                | n.a.               | stlc2500d      | Custom deep sleep management not supported       |
| Renesas RX23W                        | LE               | H4             | No           | n.a.             | Yes        | Yes                   | n.a .             | Don't know         |                | HCI Firmware part of BTTS                        |
| Realtek RTL8822CS                    | Dual mode + Wifi | H5             | Yes          | Yes              | Don't know | Don't know            | Don't know        | Don't know         |                | Requires initial firmware + config               |
| Realtek USB Dongles                  | Dual mode + Wifi | USB            | Yes          | Yes              | Don't know | Don't know            | Don't know        | Don't know         | realtek        | Requires initial firmware + config               |
| Toshiba TC35661                      | Dual mode        | H4             | No           | No               | No         | No                    | No                | No                 | tc3566         | Only -007/009 models provide full HCI. See below |
| TI CC256x, WL183x                    | Dual mode        | H4, H5, eHCILL | Yes          | Yes              | No         | Yes for CC256XC       | No                | No                 | cc256x         | Also WL185x, WL187x, and WL189x                  |

**Notes**:

  1. BD_ADDR: Indicates if Bluetooth chipset comes with its own valid MAC Address. Better Broadcom and CSR dongles usually come with a MAC address from the dongle manufacturer, but cheaper ones might come with identical addresses.
  2. SCO over HCI: All Bluetooth Classic chipsets support SCO over HCI in general. BTstack can receive SCO packets without problems. However, only TI CC256x has support for using SCO buffers in the Controller and a useful flow control. On CSR/Broadcom/Cypress Controllers, BTstack cannot queue multiple SCO packets in the Controller. Instead, the SCO packet must be sent periodically at the right time - without a clear indication about when this time is. The current implementation observes the timestamps of the received SCO packets to schedule sending packets. With full control over the system and no other Bluetooth data, this can be flawless, but it's rather fragile in general. For these, it's necessary to use the I2S/PCM interface for stable operation.
  , for those that are marked with No, we either didn't try or didn't found enough information to configure it correctly.
  3. Multiple LE Roles: Apple uses Broadcom Bluetooth+Wifi in their iOS devices and newer iOS versions support multiple concurrent LE roles,
  so at least some Broadcom models support multiple concurrent LE roles.

## Atmel/Microchip

The ATILC3000 Bluetooth/Wifi combo controller has been used with Linux on embedded devices by Atmel/Microchip. Drivers and documentation are available from a [GitHub repository](https://github.com/atwilc3000). The ATWILC3000 has a basic HCI implementation stored in ROM and requires a firmware image to be uploaded before it can be used. The BLE Controller is qualified as [QDID 99659](https://www.bluetooth.org/tpg/QLI_viewQDL.cfm?qid=36402). Please note: the BLE firmware is around 60 kB. It might need a separate Wifi firmware as well.

**BD Addr** can be set with vendor-specific command although all chipsets have an official address stored. The BD_ADDR lookup results in "Newport Media Inc." which was [acquired by Atmel](http://www.atmel.com/about/news/release.aspx?reference=tcm:26-62532) in 2014.

**Baud rate** can be set with a custom command.

**BTstack integration**: *btstack_chipset_atwilc3000.c* contains the code to download the Bluetooth firmware image into the RAM of the ATWILC3000. After that, it can be normally used by BTstack.

## Broadcom/Cypress/Infineon Semiconductor

Before the Broadcom Wifi+Bluetooth division was taken over by Cypress Semiconductor, it was not possible to buy Broadcom chipset in low quantities. Nevertheless, module manufacturers like Ampak created modules that contained Broadcom BCM chipsets (Bluetooth as well as Bluetooth+Wifi combos) that might already have been pre-tested for FCC and similar certifications.

A popular example is the Ampak AP6212A module that contains an BCM 43438A1 and is used on the Raspberry Pi 3, the RedBear Duo, and the RedBear IoT pHAT for older Raspberry Pi models.

The CYW20704 A2 controller supports both DLE as well as multiple LE roles and is available e.g. from [LairdTech](https://www.lairdtech.com/bt850-bt851-and-bt860-series-modules-adapter-dvks-0002) as UART module (BT860), USB module (BT850), and USB dongle.

Interestingly, the CYW20704 exhibits the same UART flow control bug as the CC2564. You can add ENABLE_CYPRESS_BAUDRATE_CHANGE_FLOWCONTROL_BUG_WORKAROUND to activate a workaround and/or read the bug & workardound description in the TI section below.

The best source for documentation on vendor specific commands so far has been the source code for blueZ and the Bluedroid Bluetooth stack from Android, but with the takeover by Cypress, documentation is directly available.

Broadcom USB dongles do not require special configuration, however SCO data is not routed over USB by default.

The PSoC 4 SoCs can be programmed with the "BLE DTM" HCI Firmware from the PSoC Creator Kit Examples. The UART baudrate is set to 115200. For higher baud rates, the clocks probably need to be configured differently, as the IDE gives a warning about this.

The CYW20819 can be used as a SoC with Cypress' Bluetooth stack. To use it as a regular Bluetooth Controller over HCI H4, CTS must be asserted during Power-Up / Reset. 

The CYW43xxx series contains a Wifi and Bluetooth Controller. The Bluetooth Controller can be used independent from the Wifi part.

Newer Controller likes the CYW5557x series requires to enter a so-called autobaud mode by asserting CTS (low) during reset/power-up. In this mode, only a subset of 
HCI commands are available. Please see posix-h4-bcm port to get started.

**Init scripts**: For UART connected chipsets, an init script has to be uploaded after power on. For Bluetooth chipsets that are used in Broadcom Wifi+Bluetooth combos, this file often can be found as a binary file in Linux distributions with the ending *'.hcd'* or as part of the WICED SDK as C source file that contains the init script as a data array for use without a file system.

To find the correct file, Broadcom chipsets return their model number when asked for their local name.

BTstack supports uploading of the init script in two variants: using .hcd files looked up by name in the posix-h4 port and by linking against the init script in the WICED port. While the init script is processed, the chipsets RTS line goes high, but only 2 ms after the command complete event for the last command from the init script was sent. BTstack waits for 10 ms after receiving the command complete event for the last command to avoid sending before RTS goes high and the command fails.

**BD Addr** can be set with a custom command. A fixed address is provided on some modules, e.g. the AP6212A, but not on others.

**SCO data** can be configured with a custom command found in the bluez sources. It works with USB chipsets. The chipsets don't implement the SCO Flow Control that is used by BTstack for UART connected devices. A forum suggests to send SCO packets as fast as they are received since both directions have the same constant speed.

**Baud rate** can be set with custom command. The baud rate resets during the warm start after uploading the init script. So, the overall scheme is this: start at default baud rate, get local version info, send custom Broadcom baud rate change command, wait for response, set local UART to high baud rate, and then send init script. After sending the last command from the init script, reset the local UART. Finally, send custom baud rate change command, wait for response, and set local UART to high baud rate.

**BTstack integration**: The common code for all Broadcom chipsets is provided by *btstack_chipset_bcm.c*. During the setup, *btstack_chipset_bcm_instance* function is used to get a *btstack_chipset_t* instance and passed to *hci_init* function.

SCO Data can be routed over HCI for both USB dongles and UART connections, however BTstack only can send audio correctly over UART with newer Controllers that support SCO Flow Control. 
HSP and HFP Narrow Band Speech is supported via I2C/PCM pins. Newer Controllers provide an mSBC codec that allows to use HSP/HFP incl. WBS over PCM/I2S with ENABLE_BCM_PCM_WBS.


## CSR / Qualcomm Incorporated

CSR plc has been acquired by Qualcomm Incorporated in August 2015.

Similar to Broadcom, the best source for documentation is the source code for blueZ. 

CSR USB dongles do not require special configuration and SCO data is routed over USB by default.

CSR chipset do not require an actual init script in general, but they allow to configure the chipset via so-called PSKEYs. After setting one or more PSKEYs, a warm reset activates the new setting.

**BD Addr** can be set via PSKEY. A fixed address can be provided if the chipset has some kind of persistent memory to store it. Most USB Bluetooth dongles have a fixed BD ADDR.

**SCO data** can be configured via a set of PSKEYs. We haven't been able to route SCO data over HCI for UART connections yet.

**Baud rate** can be set as part of the initial configuration and gets actived by the warm reset.

**BTstack integration**: The common code for all Broadcom chipsets is provided by *btstack_chipset_csr.c*. During the setup, *btstack_chipset_csr_instance* function is used to get a *btstack_chipset_t* instance and passed to *hci_init* function. The baud rate is set during the general configuration.

SCO Data is routed over HCI for USB dongles, but not for UART connections. HSP and HFP Narrow Band Speech is supported via I2C/PCM pins.


## Dialog Semiconductor / Renesas

Daialo Semiconductor has been aquired by Renesas in February 2021.

They offers the DA145xx and DA1469xx series of LE-only SoCs that can be programmed with an HCI firmware. 
The HCI firmware can be uploaded on boot into SRAM or stored in the OTP (One-time programmable) memory, or in an external SPI.

The 581 does not implement the Data Length Extension or supports multiple concurrent roles, while later versions support it.

The mechanism to boot a firmware via UART/SPI has mostly stayed the same, while the set of supported interfaces and baudrates have slightly changed.

The DA1469x uses an external flash. The DA 1469x SDK contains a HCI firmware that can be compiled and downloaded into flash using the SmartSnippets Studio.

Unexpected issues:
- DA14585 cannot scan for other devices if advertising is enabled alhtough it supports multiple Peripheral/Central roles (last check: 6.0.14.1114)
 
**BD Addr** fixed to 80:EA:CA:00:00:01. No command in HCI firmware to set it differently. Random addresses could be used instead.

**Baud rate**: The baud rate is fixed at 115200 with the provided firmware. A higher baud rate could be achieved by re-compiling the HCI firmware.

**BTstack integration**: *btstack_chipset_da145xx.c* contains the code to download the provided HCI firmware into the SRAM of the DA145xx. After that, it can be used as any other HCI chipset. No special support needed for DA1469x after compiling and flashing the HCI firmware.


## Espressif ESP32

The ESP32 is a SoC with a built-in Dual mode Bluetooth and Wifi radio. The HCI Controller is implemented in software and accessed via a so called Virtual HCI (VHCI) interface.
It supports both LE Data Length Extensions (DLE) as well as multiple LE roles. Since ESP-IDF v4.3, SCO-over-HCI is usable for HSP/HFP.

The newer ESP32-S3 and ESP32-C3 SoCs have a newer LE Controller that also supports 2M-PHY, but does support Classic (BR/EDR) anymore.

ALl can either be used as an SoC with the application running on the ESP32 itself or can be configured as a regular Bluetooth HCI Controller.
BTstack can work either on the SoC itself or on another MCU with the ESP32 connected via 4-wire UART.

See Espressif's [ESP-Hosted firmware](https://github.com/espressif/esp-hosted) for use as Bluetooth/Wifi Ccontroller.


## EM Microelectronic Marin

For a long time, the EM9301 has been the only Bluetooth Single-Mode LE chipset with an HCI interface. The EM9301 can be connected via SPI or UART. 
The UART interface does not support hardware flow control and is not recommended for use with BTstack. The SPI mode uses a proprietary but documented extension to implement flow control and signal if the EM9301 has data to send.

In December 2016, EM released the new EM9304 that also features an HCI mode and adds support for optional Bluetooth 4.2. features. 
It supports the Data Length Extension and up to 8 LE roles. The EM9304 is a larger MCU that allows to run custom code on it. 
For this, an advanced mechanism to upload configuration and firmware to RAM or into an One-Time-Programmable area of 128 kB is supported. 
It supports a superset of the vendor specific commands of the EM9301.

EM9304 is used by the 'stm32-l073rz-em9304' port in BTstack. The port.c file also contains an IRQ+DMA-driven implementation of the SPI H4 protocol specified in the [datasheet](http://www.emmicroelectronic.com/sites/default/files/public/products/datasheets/9304-ds_0.pdf).

**BD Addr** must be set during startup for EM9301 since it does not have a stored fix address. The EM9304 comes with an valid address stored in OTP.

**SCO data** is not supported since it is LE only.

**Baud rate** can be set for UART mode. For SPI, the master controls the speed via the SPI Clock line. With 3.3V, 16 Mhz is supported.

**Init scripts** are not required although it is possible to upload small firmware patches to RAM or the OTP memory (EM9304 only).

**BTstack integration**: The common code for the EM9304 is provided by *btstack_chipset_em9301.c*. During the setup, *btstack_chipset_em9301_instance* function is used to get a *btstack_chipset_t* instance and passed to *hci_init* function. It enables to set the BD Addr during start.


## Intel Dual Wireless 8260, 8265

Wifi/Bluetooth combo cards mainly used in mobile computers. The Bluetooth part requires the upload of a firmware file and a configuration file. SCO, DLE, Multiple roles not tested.


## Nordic nRF5 series

The Single-Mode LE chipsets from the Nordic nRF5 series chipsets usually do not have an HCI interface. Instead, they provide an LE Bluetooth Stack as a binary library, the so-called *SoftDevices*. Developer can write their Bluetooth application on top of this library. Since the chipset can be programmed, it can also be loaded with a firmware that provides a regular HCI H4 interface for a Host.

An interesting feature of the nRF5 chipsets is that they can support multiple LE roles at the same time, e.g. being Central in one connection and a Peripheral in another connection. Also, the nRF52 SoftDevice implementation supports the Bluetooth 4.2 Data Length Extension.

Both nRF5 series, the nRF51 and the nRF52, can be used with an HCI firmware. The nRF51 does not support encrypted connections at the moment (November 18th, 2016) although this might become supported as well.

**BD ADDR** is not set automatically. However, during production, a 64-bit random number is stored in the each chip. Nordic uses this random number as a random static address in their SoftDevice implementation.

**SCO data** is not supported since it is LE only.

**Baud rate** is fixed to 115200 by the patch although the firmware could be extended to support a baud rate change.

**Init script** is not required.

**BTstack integration**: Support for a nRF5 chipset with the Zephyr Controller is provided by *btstack_chipset_zephyr.c*. It queries the static random address during init.

To use these chipsets with BTstack, you need to install an arm-none-eabi gcc toolchain and the nRF5x Command Line Tools incl. the J-Link drivers, checkout the Zephyr project, apply a minimal patch to help with using a random static address, and flash it onto the chipset:

  * Install [J-Link Software and documentation pack](https://www.segger.com/jlink-software.html).
  * Get nrfjprog as part of the [nRFx-Command-Line-Tools](http://www.nordicsemi.com/eng/Products/Bluetooth-low-energy/nRF52-DK). Click on Downloads tab on the top and look for your OS.
  * [Checkout Zephyr and install toolchain](https://www.zephyrproject.org/doc/getting_started/getting_started.html). We recommend using the [arm-non-eabi gcc binaries](https://launchpad.net/gcc-arm-embedded) instead of compiling it yourself. At least on OS X, this failed for us.

  * In *samples/bluetooth/hci_uart* compile the firmware for nRF52 Dev Kit

<!-- -->

      $ make BOARD=nrf52_pca10040

   * Upload the firmware

      $ ./flash_nrf52_pca10040.sh

   * For the nRF51 Dev Kit, use `make BOARD=nrf51_pca10028` and `./flash_nrf51_10028.sh` with the nRF51 kit.
   * The nRF5 dev kit acts as an LE HCI Controller with H4 interface.


## NXP Semiconductors
NXP Semiconductors acquired the Bluetooth + Wifi division of Marvel in 2019 and continues their products with new names.
As the Controllers contain no Bluetooth firmware, the firmware needs to be uploaded on start.
BTstack supports firmware upload for older Controllers with bootloader version v1, like the NXP 88W8997.

**BD ADDR** is stored in Controller.

**SCO data** is routed over HCI by default but does not support flow control.

**Baud rate** is currently kept at 115200

**Init script** is required.

**BTstack integration**: firmware update required and implemented by *btstack_chipset_nxp.c*  See port/posix-h4-nxp for details on how to use it.


## Realtek

Realtek provides Dual-Mode Bluetooth Controllers with USB and UART (H4/H5) interfaces as well as combined Bluetooth/WiFi Controllers, which are also available as M.2 modules for laptops.
They commonly require to download a patch and a configuration file. Patch and configuration file can be found as part of their Linux drivers.

**BD ADDR** is stored in Controller.

**SCO data** can either be routed over HCI with working flow control or over I2S/PCM. The 8822CS supports mSBC codec internally.

**Baud rate** is set by the config file.

**Init script** is required.

**BTstack integration**: H4/H5 Controller require firmware upload. 'rtk_attach' can be used for this. For USB Controllers, 
*btstack_chipset_realtek.c* implements the patch and config upload mechanism. See port/libusb for details on how to use it.


## Renesas Electronics

Please see Dialog Semiconducator for DA14xxx Bluetooth SoCs above.

Renesas currently has 3 LE-only SoCs: the older 16-bit RL78 and the newer RX23W and the RA4W1. 
For the newer SoCs, Renesas provides a pre-compiled HCI firmware as well as an HCI project for their e2 Studio IDE.
The HCI firmware needs to be programmed into the SoC Flash.

Both newer SoC provide the newer Bluetooth 5.0 features like DLE, 2M-PHY, Long Range, and Multiple Roles.

To install the HCI Firmware on the [Target Board for RX23W](https://www.renesas.com/us/en/products/software-tools/boards-and-kits/eval-kits/rx-family-target-board.html):
	
  * Download [Bluetooth Test Tool Suite](https://www.renesas.com/eu/en/software/D6004348.html)
  * Install [Renesas Flash Programmer](https://www.renesas.com/tw/en/products/software-tools/tools/programmer/renesas-flash-programmer-programming-gui.html)
  * Follow instruction in [Target Board for RX23W Quick Start Guide](https://www.renesas.com/us/en/doc/products/mpumcu/apn/rx/013/r20qs0014ej0100-23wtbqsg.pdf) to flash `rx23w_uart_hci_sci8_br2000k_v1.00.mot`

**BD Addr** fixed to 74:90:50:FF:FF:FF. A Windows tool in the BTTS suite allows to set a public Bluetooth Address.

**Baud rate**: The baud rate is fixed at 115200 resp. 2000000 with the provided firmware images. With 2 mbps, there's no need to update the baudrate at run-time.

**BTstack integration**: No special support needed.


## STMicroelectronics

STMicroelectronics has several different Bluetooth series.

### STLC2500D

It offers the Bluetooth V2.1 + EDR chipset STLC2500D that supports SPI and UART H4 connection.

**BD Addr** can be set with custom command although all chipsets have an official address stored.

**SCO data** might work. We didn't try.

**Baud rate** can be set with custom command. The baud rate change of the chipset happens within 0.5 seconds. At least on BTstack, knowning exactly when the command was fully sent over the UART is non-trivial, so BTstack switches to the new baud rate after 100 ms to expect the command response on the new speed.

**Init scripts** are not required although it is possible to upload firmware patches.

**BTstack integration**: Support for the STLC2500C is provided by *btstack_chipset_stlc.c*. During the setup, *btstack_chipset_stlc2500d_instance* function is used to get a *btstack_chipset_t* instance and passed to *hci_init* function. It enables higher UART baud rate and to set the BD Addr during startup.


### BlueNRG

The BlueNRG series is an LE-only SoC which can be used with an HCI Firmware over a custom SPI interface. 

### STM32-WB5x

The new STM32-WB5x series microcontroller is an SoC with a multi-protocol 2.4 Ghz radio co-processor. It provides a virtual HCI interface.


## Texas Instruments CC256x series

The Texas Instruments CC256x series is currently in its fourth iteration and provides a Classic-only (CC2560), a Dual-mode (CC2564), and a Classic + ANT (CC2567) model. A variant of the Dual-mode chipset is also integrated into TI's WiLink 8 Wifi+Bluetooth combo modules of the WL183x, WL185x, WL187x, and WL189x series. Some of the latter support ANT as well.

The CC256x chipset is connected via an UART connection and supports the H4, H5 (since third iteration), and eHCILL.

The latest generation `CC256xC` chipsets support multiple LE roles in parallel.

TI provides an alternative firmware that integrates an SBC Codec in the Bluetooth Controller itself for Assisted A2DP (A3DP) and Assisted HFP (Wide-band speech support). While this can save computation and code size on the main host, it cannot be used together with BLE, making it useless in most projects.

The different CC256x chipset can be identified by the LMP Subversion returned by the *hci_read_local_version_information* command. TI also uses a numeric way (AKA) to identify their chipsets. The table shows the LMP Subversion and AKA number for the CC256x and the WL18xx series.

Chipset | LMP Subversion |  AKA
--------|----------------|-------
CC2560  |         0x191f | 6.2.31
CC2560A, CC2564, CC2567 | 0x1B0F | 6.6.15
CC256xB |         0x1B90 | 6.7.16
CC256xC |         0x9a1a | 6.12.26
WL18xx  |         0xac20 | 11.8.32

**SCO data:** Routing of SCO data can be configured with the `HCI_VS_Write_SCO_Configuration` command.

**Baud rate** can be set with `HCI_VS_Update_UART_HCI_Baudrate`. The chipset confirms the change with a command complete event after which the local UART is set to the new speed. Oddly enough, the CC256x chipsets ignore the incoming CTS line during this particular command complete response. 

If you've implemented the hal_uart_dma.h without an additional ring buffer (as recommended!) and you have a bit of delay, e.g. because of thread switching on a RTOS, this could cause a UART overrun.
If this happens, BTstack provides a workaround in the HCI H4 transport implementation by adding `ENABLE_CC256X_BAUDRATE_CHANGE_FLOWCONTROL_BUG_WORKAROUND`. 
If this is enabled, the H4 transport layer will resort to "deep packet inspection" to first check if its a TI controller and then wait for the HCI_VS_Update_UART_HCI_Baudrate. 
When detected, it will tweak the next UART read to expect the HCI Command Complete event.

**BD Addr** can be set with `HCI_VS_Write_BD_Addr` although all chipsets have an official address stored.

**Init Scripts.** In order to use the CC256x chipset an initialization script must be obtained and converted into a C file for use with BTstack. For newer revisions, TI provides a main.bts and a ble_add_on.bts that need to be combined.

The Makefile at *chipset/cc256x/Makefile.inc* is able to automatically download and convert the requested file. It does this by:

-   Downloading one or more `BTS files` for your chipset.
-   Running the Python script:

<!-- -->

    ./convert_bts_init_scripts.py main.bts [ble_add_on.bts] output_file.c

**BTstack integration**: 
- The common code for all CC256x chipsets is provided by *btstack_chipset_cc256x.c*. During the setup, *btstack_chipset_cc256x_instance* function is used to get a *btstack_chipset_t* instance and passed to *hci_init* function. *btstack_chipset_cc256x_lmp_subversion* provides the LMP Subversion for the selected init script.
- SCO Data is be routed over HCI with `ENABLE_SCO_OVER_HCI` or to PCM/I2S with `ENABLE_SCO_OVER_PCM`. Wide-band speech is supported in both cases. For SCO-over-HCI, BTstack implements the mSBC Codec. For SCO-over-I2S, Assisted HFP can be used.
- Assisted HFP: BTstack provides support for Assisted HFP mode if enabled with `ENABLE_CC256X_ASSISTED_HFP` and SCO is routed over PCM/I2S with `ENABLE_SCO_OVER_PCM`. During startup, the PCM/I2S is configured and the HFP implementation will enable/disable the mSBC Codec for Wide-band-speech when needed.

Known issues:
- The CC2564C v1.5 may loose the connection in Peripheral role with a Slave Latency > 0 when the Central updates Connection Parameters. See [https://github.com/bluekitchen/btstack/issues/429](Issue #429)


## Toshiba

The Toshiba TC35661 Dual-Mode chipset is available in three variants: standalone incl. binary Bluetooth stack, as a module with embedded stack or with a regular HCI interface. The HCI variant has the model number TC35661–007 resp TC35561-009 for the newer silicon.

We first tried their USB Evaluation Stick that contains an USB-to-UART adapter and the PAN1026 module that contains the TC35661 -501. While it does support the HCI interface and Bluetooth Classic operations worked as expected, LE HCI Commands are not supported. With the -007 and the -009 models, everything works as expected.

**SCO data** does not seem to be supported.

**Baud rate** can be set with custom command.

**BD Addr ** must be set with custom command. It does not have a stored valid public BD Addr.

**Init Script** is not required. A patch file might be uploaded.

**BTstack integration**: Support for the TC35661 series is provided by *btstack_chipset_tc3566x.c*. During the setup, *btstack_chipset_tc3566x_instance* function is used to get a *btstack_chipset_t* instance and passed to *hci_init* function. It enables higher UART baud rate and sets the BD Addr during startup.
