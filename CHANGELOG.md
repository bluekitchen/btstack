# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Port for Windows with Zephyr HCI Firmware connected via serial port  
- em9304: ability to upload patch containers during HCI bootup.

### Changed

### Fixed

## Changes December 2017

### Added
- Introduced btstack_network.h network interface abstraction
- btstack_network_posix implementation using Linux tun/tap interface
- WICED: support for btstack_stdin (HAVE_STDIN)
- GATT Server: Writes to GATT Client Characteristic Configuration are stored in TLV and restored on reconnect. The db.h file generated from db.gatt needs to be re-created.
- TLV: global TLV instance available via btstack_tlv_get_instance() (src/btstack_tlv.h)
- TLV: POSIX implementation that appends to a file on disk (platform/posix/btstack_tlv_posix.c
- LE Device DB TLV: overwrite oldest entry if no free entries available 
- SM: allow to set fixed passkey in display role using sm_use_fixed_passkey_in_display_role
- Example/hid_host_demo with HID parser and support for basic US keyboard layout
- EM9304: custom HCI Transport implementation for EM9304 on top of btstack_em9304_spi.h platform abstraction only requires hal_em9304_spi.h to be implemented for new ports
- Port for Apollo2 MCU with EM9304 (ports/apollo2-em9304)

### Changed
- panu_demo: uses btstack_network.h now
- WICED: configure printf to replace Linefeed with CRLF
- SBC: split btstack_sbc_bludroid.c into seperate encoder and decoder implementations

### Fixed
- RFCOMM: support connection requests during connection failure 
- L2CAP: support connection requests during connection failure 
- L2CAP: fix default remote MTU as 672 instead of 48 (Minimal MTU)
- HCI: avoid double free during halting
- SM: fixed reconnect using legacy pairing in slave role


