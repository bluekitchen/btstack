# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]


## Changes September 2018

### Fixed
- HCI/L2CAP: Error creating outgoing connection (e.g. Connection Limit Exceeded) now handled
- RFCOMM: Trigger l2cap request to send on rfcomm credits when client is waiting to sendtrigger l2cap request to send on rfcomm credits when client is waiting to send
- L2CAP: Try to emit 'can send now' on HCI Disconnect, if all ACL buffers in Bluetooth Controller have been used for the closed connection

## Changes August 2018

### Added
- PBAP: added pbap_get_phonebook_size() to get phonebook entry count

### Fixed
- GATT Server: Allow enable Notifications/Indication with Write Command. Fixes issue with some Android devices.
- SM: fix pairing for Secure Connections with Bonding if remote sends additional keys
- SM: drop LTK flag from Pairing Response for Secure Connections
- L2CAP: fix emitted L2CAP_EVENT_CONNECTION_PARAMETER_UPDATE_REQUEST

## Changes June 2018

### Added
- btstack_audio.h: application level API for audio playback and recording
- embedded/hal_audio.h: low-level API for audio playback and recording
- HID Device: hid_device_connect(..) function
- ESP32: implement hal_audio
- DA14585: support for Dialog Semiconductor DA14585 LE-only controller
- Rasperry Pi 3 + Raspberry Pi Zero W port in port/raspi

### Changed
- Errata 10734:
  - SM: Generate new EC Public Keypair after each pairing
  - SM: Abort failure with DHKEY_CHECK_FAILED if received public key is invalid (instead of unspecified error)
- btstack.h: only include classic headers if ENABLE_CLASSIC is defined
- windows: ignore virtual Bluetooth adapter provided by VMware
- Replaced HCI_PACKET_BUFFER_SIZE with HCI_INCOMING_PACKET_BUFFER_SIZE and HCI_OUTGOING_PACKET_BUFFER_SIZE

## Changes June 2018

### Fixed
- HFP: Fix Wide Band Speech bit in SDP record for both HF/AG. Missing bit prevents macOS from using mSBC
- ATT Server: send ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE with status ATT_HANDLE_VALUE_INDICATION_DISCONNECT on disconnect
- AVRCP Controller: fix parsing of now playing info

### Changed
- ATT Server: ATT_HANDLE_VALUE_INDICATION_DISCONNECT is delivered to service handler if registered

### Added
- ATT Server: packet handler field added to att_service_handler_t to receive: connect/disconnect, atu exchange, indication complete

## Changes May 2018

### Added
- GAP: re-encrypt outgoing connection if bonded
- ATT Server: wait until re-encryption is complete
- GATT Client: wait until re-encryption is complete
- ATT Server: added att_server_request_to_send_notification and att_server_request_to_send_indication
- GATT Client: if ENABLE_GATT_CLIENT_PAIRING, GATT Client starts pairing and retry operation on security error

### Changed
- ATT Server: att_server_register_can_send_now_callback is deprecated, use att_server_request_to_send_notification/indication instead

### Fixed
- SM: Fix LE Secure Connection pairing in Central role
- le_device_db_tlv: fix seq nr management
- SM: improve le_device_db lookup and storing of IRK
- GATT Server: fix lookup for Client Characteristic Configuration in services with multiple Characteristics
- RFCOMM: emit channel closed on HCI/L2CAP disconnect after sending DISC while expecting UA

## Changes April 2018

### Added
- Crypto: btstack_crypo.h provides cryptographic functions for random data generation, AES128, EEC, CBC-MAC (Mesh)
- SM: support pairing using Out-of-Band (OOB) data with LE Secure Connections
- Embedded: support btstack_stdin via SEGGER RTT

### Changed
- att_db_util: added security requirement arguments to characteristic creators
- SM: use btstack_crypto for cryptographpic functions
- GAP: security level for Classic protocols (asides SDP) raised to 2 (encryption)

### Fixed
- HFP: fix answer call command
- HCI: fix buffer overrun in gap_inquiry_explode
- SDP: free service record item on sdp_unregister_service

## Changes March 2018

### Added
- GAP: allow to limit number of connections in LE Peripheral role with gap_set_max_number_peripheral_connections
- ATT Server: support for delayed ATT read response, see example/att_delayed_read_response.c
- ATT Server: allow to specify security requirements seperately for read/writes. .h files need to be regenerated

### Fixed
- RFCOMM: fix infinite loop on L2CAP connection error (regression from 4c3eeed1)
- HSP HS: accept incomming SCO connection
- SM: fix iteration over LE Device DB entries for bonding and address resolving
- SM: store pairing information only if both devices have requested bonding

## Changes February 2018

### Added
- GATT Client: gatt_client_request_can_write_without_response_event() causes GATT_EVENT_CAN_WRITE_WITHOUT_RESPONSE
- SM: new event SM_EVENT_PAIRING_COMPLETE
- GAP: support iteration over stored Classic link keys: gap_link_key_iterator_init, gap_link_key_iterator_get_next, gap_link_key_iterator_done
- GAP: add gap_delete_all_link_keys

### Changed
- GATT Client: round robin for multiple connections
- ATT Dispatch: round robin for ATT Server & GATT Client
- L2CAP: round robin for all L2CAP channels (fixed and dynamic)
- btstack_link_key_db: addition functions for link key iteration
- GAP: LE scanning enabled not reset on HCI Reset -> can be enabled before HCI Power Up
- CSR: set all keys in psram instead of default

### Fixed
- tc3556x: fix startup after baud rate change


## Changes January 2018

### Added
- Port for Windows with Zephyr HCI Firmware connected via serial port
- em9304: upload patch containers during HCI bootup
- Makefile for STM32-F4Discovery port
- support for console out via SEGGER RTT
- LE Data Channels example in BTstack and [https://github.com/bluekitchen/CBL2CAPChannel-Demo](iOS example on GitHub)
- LE Streamer Client can send as fast as possbile as well
- L2CAP: option to limit ATT MTU via l2cap_set_max_le_mtu

### Changed
- HCI: allow to set hci_set_master_slave_policy (0: try to become master, 1: accept slave)
- GAP: gap_set_connection_parameters includes scan interval and window params
- GATT Client: GATT_EVENT_MTU indicates max MTU
- ATT DB Util: attribute handle is returned for new Services and Characteristics

### Fixed
- Windows: retry serial port operations if not all bytes have been read/written
- HFP: avoid buffer overflows setting up messages
- SBC Decoder: improved error handling for invalid SBC audio data
- GAP: fix Connection Parameter Response in Central role
- ATT DB Util: update pointer to database in case of realloc
- GATT Client: set UUID16 field if 16-bit UUID is stored as UUID128
- GAP: release HCI Connnection after gap_le_conne
- ATT: Exchanged MTU is propagate to ATT Server and GATT Client


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


