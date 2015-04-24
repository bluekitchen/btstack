# STM32 F103RB Nucleo with CC256x

BTstack port for STM32 F103RB Nucleo board and CC256x Bluetooth chipset
based on GNU Tools for ARM Embedded Processors and libopencm3

Requirements:
- GNU Tools for ARM Embedded Processors: https://launchpad.net/gcc-arm-embedded
- libopencm3 is automatically fetched and build from its git repository by make
- openocd 0.8.0 (or higher) is used to upload firmware

Components:
- STM32 F103RB Nucleo Board
  - User's Manual: http://www.st.com/web/en/resource/technical/document/user_manual/DM00105823.pdf
- EM Wireless Booster Pack 
  - Info: http://www.ti.com/tool/boost-ccemadapter
  - User Guide: http://www.ti.com/lit/ug/swru338a/swru338a.pdf
  - Booster Pack Pinout: http://www.ti.com/ww/en/launchpad/dl/boosterpack-pinout-v2.pdf
- CC256x Evaluation Module
  - Info: http://www.ti.com/tool/cc256xqfnem

Configuration:
- Sys tick 250 ms
- LED on PA5, on when MCU in Run mode and off while in Sleep mode
- Debug UART: USART2 - 9600/8/N/1, TX on PA2
- Bluetooth: USART3 with hardware flowcontrol RTS. IRQ on CTS Rising. TX PB10, RX PB11, CTS PB13 (in), RTS PB14 (out), N_SHUTDOWN PB15

Setup:
- Solder 32.768 kHz quarz oscillator to EM Adapter Booster Pack as explained in 4.7 of the EM Wireless Booster Pack User Guide. If you don't have an oscillator of that size, you might solder one upside done (turtle on back style) to the unused upper right pad and wire GCC, VCC, and clock with thin wires.
- Connect STM32 Nucleo Board to EM Wireless Board (see boosterpack pinout)
  - GND:        CN10-9  - 20 (LP2)
  - VCC:        CN7 -16 -  1 (LP1)
  - RX3:        CN10-18 -  3 (LP1)
  - TX3:        CN10-25 -  4 (LP1)
  - CTS3:       CN10-30 - 11 (LP2)
  - RTS3:       CN10-28 - 12 (LP2)
  - N_SHUTDOWN: CN10-26 - 10 (LP1)

TODO:
- figure out how to compile multiple examples with single Makefile/folder
