# BTstack Port for the Microchip PIC32 Harmony Platform

Status: All examples working, polling UART driver. Tested on Bluetooth Audio Development Kit only.

## Setup

- Place BTstack tree into harmony/framework folder. 
- Run port/pic32-harmony/create_examples.py to create examples in harmony/apps/btstack folder.

## Usage

The examples can be opened and compiled in Microchip MPLABX.

### Modifications to the GATT Database

After changing the GATT definition in $example.gatt, please run ./update_gatt_db.h to regenerate $example.h in the $example folder.
