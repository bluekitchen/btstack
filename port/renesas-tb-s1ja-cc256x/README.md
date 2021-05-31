# BTstack Port for Renesas Target Board TB-S1JA with CC256x

This port uses the Renesas TB-S1JA with TI's CC256XEM ST Adapter Kit that allows to plug in a CC256xB or CC256xC Bluetooth module.
Renesas e2 Studio (Eclise-based) was used with the SSP HAL and without an RTOS.
For easy debugging, Ozone project files are generated as well.

## Hardware

Renesas Target Board TB-S1JA:
- [TB-S1JA Target Board Kit](https://www.renesas.com/eu/en/products/synergy/hardware/kits/tb-s1ja.html)

- CC2564B Bluetooth Controller:
  1. The best option is to get it as a BoostPack 
     - Info: BOOST-CC2564MODA: http://www.ti.com/tool/BOOST-CC2564MODA
  2. Alternatively, get the evaluation module together with the EM Wireless Booster pack and a 32.768 kHz oscillator
     - EM Wireless Booster Pack:
       - [Info](http://www.ti.com/tool/BOOST-CCEMADAPTER)
       - [User Guide](http://www.ti.com/lit/pdf/swru338)
    - CC256x Bluetooth module:
       - [CC2564B Dual-mode Bluetooth® Controller Evaluation Module](https://store.ti.com/cc2564modnem.aspx)
       - [CC2564C Dual-mode Bluetooth® Controller Evaluation Module](https://store.ti.com/CC256XCQFN-EM-CC2564C-Dual-Mode-Bluetooth-Controller-Evaluation-Module-P51277.aspx)
       - The module with the older CC2564B is around USD 20, while the one with the new CC2564C costs around USD 60

The projects are configured for the CC2564C. When using the CC2564B, *bluetooth_init_cc2564B_1.8_BT_Spec_4.1.c* should be used as cc256x_init_script. You can update this in the create_examples.py script.

Connct the Target Board to the TI Boosterpack, see Booster Pack Pinout: http://www.ti.com/ww/en/launchpad/dl/boosterpack-pinout-v2.pdf


J2 PIN | S1JA PORT | S1JA Signal | Boosterpack
-------|-----------|-------------|------------
2      | P301      | RXD0        | 3  (LP1)
4      | P302      | TXD0        | 4  (LP1)
6      | P304      | CTS0        | 36 (LP2)
8      | P303      | RTS0        | 37 (LP2)
10     | VCC       | VCC         |  1 (LP1)
12     | VSS       | GND         | 20 (LP2)
14     | P112      | nShutdown   | 19 (LP1)

## Software

Generate example projects

    $ python create_examples.py

This will generate an e2 Studio project for each example. 

### Excluded Examples

The a2dp examples (a2dp_source_demo and a2dp_sink_demo) were disabled as the C open-source SBC codec
with compile option -O2 wasn't fast enough to provide real-time encoding/decoding. 


## Build, Flash And Run The Examples in e2 Studio

Open the e2 Studio project and press the 'Debug' button. Debug output is only available via SEGGER RTT. You can run SEGGER's JLinkRTTViewer or use Ozone as described below.


## Run Example Project using Ozone

After compiling the project with e2 Studio, the genereated .elf file can be used with Ozone (also e.g. on macOS). 
In Ozone, the debug output is readily available in the terminal. A .jdebug file is provided in the project folder.


## Debug output

All debug output is send via SEGGER RTT.

In src/btstack_config.h resp. in example/btstack_config.h of the generated projects, additional debug information can be enabled by uncommenting ENABLE_LOG_INFO.

Also, the full packet log can be enabled in src/hal_entry.c by uncommenting the hci_dump_init(...) call.
The console output can then be converted into .pklg files by running tool/create_packet_log.py. The .pklg file can be 
analyzed with the macOS X PacketLogger or WireShark.

## GATT Database
In BTstack, the GATT Database is defined via the .gatt file in the example folder. The create_examples.py script converts the .gatt files into a corresponding .h for the project. After updating a .gatt file, the .h can be updated manually by running the provided update_gatt_db.sh or update_gatt_db.bat scripts. 

Note: In theory, this can be integrated into the e2 Studio/Eclipse project.


## Notes
- HCI UART is set to 2 mbps. Using 3 or 4 mbps causes hang during startup


## Nice to have
- Allow compilation using Makefile/CMake without the e2 Studio, e.g. on the Mac.
