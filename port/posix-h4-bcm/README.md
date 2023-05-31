# BTstack Port for POSIX Systems with modern Infineon (CYW) H4 Bluetooth Controller

## Configuration
Newer Infineon Airoc (tm) Controllers like the CYW5557x series accept PatchRAM upload only in a so-called
'auto-baud mode' which is entered by asserting CTS (low) and starting/resetting the controller via BT_REG_EN.
This port currently only supports the CYW5557x Controllers.

## Compilation

BTstack's posix-h4-bcm port does not have additional dependencies. You can directly run cmake and then your default
build system. E.g. with Ninja:

	mkdir build
    cd build
    cmake -G Ninja ..
    ninja
 
## Running the examples

On start, BTstack opens the serial port, which asserts CTS, and requests you to reset the Controller with a countdown.

	$ ./gatt_counter
    Packet Log: /tmp/hci_dump.pklg
    Phase 1: Download firmware
    Please reset Bluetooth Controller, e.g. via RESET button. Firmware download starts in:
    3
    2
    1
    Firmware download started
    Local version information:
    - HCI Version    0x000a
    - HCI Revision   0x0b73
    - LMP Version    0x000a
    - LMP Subversion 0x2257
    - Manufacturer 0x0009
    Phase 2: Main app
    BTstack up and running at 55:56:0A:0A:76:93
    ...

## ToDo
- select PatchRAM file based on HCI Read Local Version Information
