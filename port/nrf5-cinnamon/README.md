# BTstack Port with Cinnamon for Nordic nRF5 Series 

*Cinnamon* is BlueKitchen's minimal, yet robust Controller/Link Layer implementation for use with BTstack.

In contrast to common Link Layer implementations, our focus is on a robust and compact implementation for production use,
where code size matters (e.g. current code size about 8 kB).

## Status
The current implementation supports a single Peripheral role, or, passive scanning in Observer role. In the Peripheral role,
channel map updates, as well as connection param updates are supported.

Support for LE Central Role as well as Encryption is planned but not supported yet. 

## Requirements
- arm-none-eabi toolchain
- Nordic's nRF5-SDK

## Supported Hardware
All nNRF5x SOCs. Built files are provided for PCA10040 (52832 DK), but others can be supported with minimal changes.

## Use
- Provide path to nRF5-SDK either in `NRF5_SDK_ROOT` environment variable or directly in `pca10040/armgcc/Makefile`.
- run make
- All supported examples are built in the `build` folder.
- You can use Segger's OZONE with the provided `EXAMPLE.jdebug` project file to flash and run the examples.
