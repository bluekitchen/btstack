# BTstack Port for POSIX Systems with Dialog Semiconductor DA14531 Controller

This port allows to use the DA14531 connected via UART with BTstack running on a POSIX host system.

Instead of storing the HCI firmware in the OTP, it first downloads the hci_531_active_uart_460800.hex firmware 
from the 6.0.16.1144 SDK, before BTstack starts up.

After Power Cycle, please start one of the test applications and press the Reset button to trigger firmware download.

Please note that it does not detect if the firmware has already been downloaded, so you need to Power Cycle
the DA14531 before starting an example again. 

Alternatively, after running one of the examples once to upload the firmware, you can use the regular 'posix-h4' port
and change the initial UART baud rate to 460800 as long as you don't power cycle the dev kit.

For production use, the DA14531 could be power cycled from the main CPU during startup, e.g. after the call
to btstack_chipset_da145xx_download_firmware_with_uart, or, the HCI firmware could be burned into the OTP.

# Software Setup / Firmware

On the [DA14531 USB Development Kit](https://www.dialog-semiconductor.com/products/bluetooth-low-energy/da14531-development-kit-usb),
the UART is configured via DIP switched. By this, the mapping to the DA14531 GPIOs is fixed. In SDK 6.0.6.1144, the
GPIO mapping of RTS and CTS is flipped. In order to be able to us the same HCI firmware on both dev kits, we've 
used the following configuration in `user_perip_setup.h`

```C
    #define UART1_TX_PORT           GPIO_PORT_0
    #define UART1_TX_PIN            GPIO_PIN_0

    #define UART1_RX_PORT           GPIO_PORT_0
    #define UART1_RX_PIN            GPIO_PIN_1

    #define UART1_RTSN_PORT         GPIO_PORT_0
    #define UART1_RTSN_PIN          GPIO_PIN_4

    #define UART1_CTSN_PORT         GPIO_PORT_0
    #define UART1_CTSN_PIN          GPIO_PIN_3
```

We also increased the UART baudrate to 460800

```C
#define UART1_BAUDRATE              UART_BAUDRATE_460800
```

We also disabled the SLEEP mode in user_config.h:

```C
static const sleep_state_t app_default_sleep_mode = ARCH_SLEEP_OFF;
```

After compilation with Keil uVision 5, the generated .hex file is copied into btstack/chipset/da145xx as
`hci_531_active_uart_460800.hex`, and then
`convert_hex_files" is used to convert it into a C data array.


# Hardware Setup - Dev Kit Pro

To use the [DA14531 Dev Kit Pro](https://www.dialog-semiconductor.com/products/bluetooth-low-energy/da14530-and-da14531-development-kit-pro)
with BTstack, please make the following modifications:
- Follow Chapter 4.1 and Figure 4 in the [DA14531 Development Kit Pro Hardware User Manual
  UM-B-114](https://www.dialog-semiconductor.com/sites/default/files/2021-06/UM-B-114_DA14531_Devkit_Pro_Hardware_User%20manual_1v5.pdf)
  and set SW1 of the 14531 daughter board into position "BUCK" position marked with an "H" on the left side.
- configure the dev kit for Full UART (4-wire) Configuration by adding jumper wires between J1 and J2

# Hardware Setup - Dev Kit USB

To use the [Dev Kit USB](https://www.dialog-semiconductor.com/products/bluetooth-low-energy/da14531-development-kit-usb#tab-field_tab_content_overview)
with BTstack, please make the following modifications:
- Follow Chapter 5.6 in the [DA14531 USB Development Kit Hardware UM-B-125](https://www.dialog-semiconductor.com/sites/default/files/um-b-125_da14531_usb_development_kit_hw_manual_1v1.pdf)
  and set the DIP switches as described.
 
 # Example Run

```
$ ./gatt_counter
Packet Log: /tmp/hci_dump.pklg
Phase 1: Download firmware
Phase 2: Main app
BTstack counter 0001
BTstack up and running on 80:EA:CA:70:00:08.
```
