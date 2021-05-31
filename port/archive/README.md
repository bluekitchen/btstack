# Archive of earlier ports

The ports in this folder are not recommended for new designs. This does not mean that the target hardware is not
suitable for new designs, just that the ports would need to reworked/refreshed.

List of ports with reason for move to this archive:

- MSP430 ports: The ports used the not-maintained version of the community MSP430 gcc port, which does not support more than 64 kB of FLASH RAM.
  A current port should use the official GCC version sponsored by TI. In addition, the MSP430 UART does not support hardware RTS/CTS.
  For a new port, our [ringbuffer approach](https://bluekitchen-gmbh.com/msp432p401r-cc2564c-port-uart-without-hardware-rtscts-flow-control/) should be used. Individual ports:
    - [EZ430-RF256x Bluetooth Evaluation Tool for MSP430](https://www.element14.com/community/docs/DOC-72027/l/ez430-rf256x-bluetooth-evaluation-too)
    - [MSP430F5438 Experimenter Board for MSP430](https://www.element14.com/community/docs/DOC-40373/l/msp430f5438-based-experimenter-board) with [Bluetooth CC2564 Module Evaluation Board](https://www.ti.com/tool/cc2564modnem)
    - [MSP-EXP430F5529LP LaunchPad](https://www.ti.com/ww/en/launchpad/launchpads-msp430-msp-exp430f5529lp.html#tabs) with [Bluetooth CC2564 Module Evaluation Board](https://www.ti.com/tool/cc2564modnem) and [EM Adapter BoosterPack](https://www.ti.com/tool/boost-ccemadapter) with additional 32768Hz quartz oscillator

- Ports for Broadcom/Cypress Controllers using HCI Transport H5: The PatchRAM cannot be uploaded via H5. This requires to upload the PatchRAM
  using H4 mode and then to start the stack using H5 transport. In addition, several bugs have been observed in H5 mode, e.g. LE Encrypt not wokring. Unless absolutely neccessary, it's better to use H4 mode.
    - Unix-based system connected to Broadcom/Cypress Bluetooth module via H5 over serial port
    - Broadcom platforms that support the WICED SDK via H5 UART, see wiced-h4

- Port for MicroChip PIC32 with Harmony Framework: the original port was for Harmony v1, while there's a Harmony V3 out since 2019:
    - [Microchip's PIC32 Bluetooth Audio Development Kit](https://www.microchip.com/Developmenttools/ProductDetails/DV320032)

- iOS port:
    - BTstack on iOS is not supported anymore.

