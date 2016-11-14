In this chapter, we first explain how Bluetooth chipsets are connected physically and 
then provide information about popular Bluetooth chipset and their use with BTstack.

The communication between a Host (a computer or an MCU) and a Host Controller (the actual Bluetoot chipset) follows the Host Controller Interface (HCI), see {@fig:HostChipsetConnection}. HCI defines how commands, events, asynchronous and synchronous data packets are exchanged, see [here]). Asynchronous packets (ACL) are used for data transfer, while synchronous packets (SCO) are used for Voice with the Headset and the Hands-free Profiles.

![Host Controller to Host connection](picts/host_chipset_connection.png){#fig:HostChipsetConnection}

On desktop-class computers incl. laptops, USB is mainly used as HCI transport layer. On embedded systems, UART connections are used instead, although USB could be used as well.

For UART connections, different transport layer variants exist. The most common one is the official "UART Transport", also called H4. It requires hardware flow control via the CTS/RTS lines and assumes no errors on the UART lines. The "Three-Wire UART Transport", also called H5, makes use of the SLIP protocol to transmit a packet and can deal with packet loss and bit-errors by retranssion. Finally, Texas Instruments created the "eHCILL transport" layer based on H4 that allows both sides to enter sleep mode without loosing synchronisation.

Unfortunately, the HCI standard misses a few relevant details:

  * For UART based connections, the initial baud rate isn't defined but most Bluetooth chipsets use 115200 baud. For better throughput, a higher baud rate is necessary, but there's no standard HCI command to change it. Instead, each vendor had to come up with their own set of vendor-specific commands. Sometimes, additional steps, e.g. doing a warm reset, are neceesary to activate the baud rate change as well.

  * Some Bluetooth chipsets don't have a unique MAC address. On start, the MAC address needs to be set, but there's no standard HCI command to set it.
 
  * SCO data for Voice can either be transmitted via the HCI interface or via an explicit PCM/I2S interface on the chipset. Most chipsets default to the PCM/I2S interface. To use it via USB or for Wide-Band Speech in the Hands-Free Profile, the data needs to be delivered to the host MCU. Newer Bluetooth standards define a HCI command to configure the routing, but it is not implemented in the chipsets we've tested so far. Instead, this is configured in a vendor-specific way as well. 

  * In addition, most vendors allow to patch their chipsets at run time by sending patch comands to the chipset. Obviously, this is also vendor dependent. 


The level of developer documentation and support varies widely between the various Bluetooth chipset providers.

From our experience, only Texas Instruments and EM Microelectronics provide all relevant information directly on their website. Nordic Semiconductor does not officially have Bluetooth chipsets with HCI interface, but their the documentation on their nRF5 series is complete and very informative. TI and Nordic also provide excellent support via their web forum.

Broadcom, whose Bluetooth + Wifi division has been acquired by the Cypress Semiconductor Corporation, provides developer documentation only to large customers as far as we know. It's possible to join their Community forum and download the WICED SDK. The WICED SDK is targeted at Wifi + Bluetooth Combo chipsets and contains the necessary chipset patch files.

CSR, which has been acquired by Qualcomm, provides all relevant information on their Support website after signing an NDA.

Chipset              | HCI Transport  | SCO over HCI     | BTstack support   | Comment 
-------------------- | ---------------|------------------|-------------------|--------
Broadcom USB Dongles | USB            | No (didn't work) | chipset/bcm       | 
Broadcom BCM43438    | H4, H5         | No (didn't work) | chipset/bcm       | max UART baudrate 3 mbps
CSR 8x10, 8x11       | H4, H5         | No (didn't work) | chipset/csr       | 
CSR USB Dongles      | USB            | Yes              | chipset/csr       |
EM 9301              | SPI            | n.a.             | chipset/em9301    | LE only, custom HCI SPI implementation
Nordic nRF           | H4             | n.a.             | n.a.              | LE only, requires HCI firmware
STM STLC2500D        | H4             | No (didn't try)  | chipset/stlc2500d | custom deep sleep management not supported
TC35661              | H4             | No (didn't try)  | chipset/tc3566    | BLE patches missing
TI CC256x, WL183x    | H4, H5, eHCILL | Yes              | chipset/cc256x    | also WL185x, WL187x, and WL189x

**Note** All Bluetooth Classic chipsets support SCO over HCI, for those that are marked with No, we either didn't try or didn't found enough information to configure it correctly.

## Broadcom

## CSR

## EM Microelectronic Marin

## Nordic nRF5 series

## ST Micro

## Texas Instruments CC256x series

The Texas Instruments CC256x series is currently in its third iteration and provides a Classic-only (CC2560), a Dual-mode (CC2564), and a Classic + ANT model (CC2567). A variant of the Dual-mode chipset is also integrated into TI's WiLink 8 Wifi+Bluetooth combo modules of the WL183x, WL185x, WL187x, and WL189x series. 

The CC256x chipset is connected via an UART connection and supports the H4, H5 (since third iteration), and eHCILL.

**SCO data** is routed to the I2S/PCM interface but can be configured with the [HCI_VS_Write_SCO_Configuration](http://processors.wiki.ti.com/index.php/CC256x_VS_HCI_Commands#HCI_VS_Write_SCO_Configuration_.280xFE10.29) command.

**Baud rate** can be set with [HCI_VS_Update_UART_HCI_Baudrate](http://processors.wiki.ti.com/index.php/CC256x_VS_HCI_Commands#HCI_VS_Update_UART_HCI_Baudrate_.280xFF36.29)

**BD Addr** can be set with [HCI_VS_Write_BD_Addr](2.2.1 HCI_VS_Write_BD_Addr (0xFC06)) although all chipsets have an official address stored.

**CC256x Init Scripts.** In order to use the CC256x chipset an initialization script must be obtained and converted into a C file for use with BTstack. 

The Makefile at *chipset/cc256x/Makefile* is able to automatically download and convert the requested file. It does this by:

-   Downloading one or more [BTS files](http://processors.wiki.ti.com/index.php/CC256x_Downloads) for your chipset.
-   Running runnig the Python script: 

<!-- -->

    ./convert_bts_init_scripts.py

**Update:** For the latest revision of the CC256x chipsets, the CC2560B
and CC2564B, TI decided to split the init script into a main part and
the BLE part. The conversion script has been updated to detect
*bluetooth_init_cc256x_1.2.bts* and adds *BLE_init_cc256x_1.2.bts*
if present and merges them into a single .c file.

**Update 2:** In May 2015, TI renamed the init scripts to match
the naming scheme previously used on Linux systems. The conversion
script has been updated to also detect *initscripts_TIInit_6.7.16_bt_spec_4.1.bts*
and integrates *initscripts_TIInit_6.7.16_ble_add-on.bts* if present.

**BTstack integration**: The common code for all CC256x chipsets is provided by *btstack_chipset_cc256x.c*. During the setup, *btstack_chipset_cc256x_instance* function is used to get a *btstack_control_t* instance and passed to *hci_init* function.

SCO Data can be route over HCI, so HFP Wide-Band Speech is supported.

## Toshiba


