# Change Log
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- le_device_db: add secure_connection argument to le_device_db_encryption_set and le_device_db_encryption_get

### Fixed
- SM: Use provided authentication requirements in slave security request

### Added
- SM: Track if connection encryption is based on LE Secure Connection pairing
- ATT DB: Validate if connection encrypted is based on SC if requested 
- att_db_util: support ATT_SECURITY_AUTHENTICATED_SC permission flag
- GATT Compiler: support READ_AUTHENTICATED and WRITE_AUTHENTICATED permsission flags
- port/stm32-f4discovery-cc256x: add support for built-in MEMS microphone

## Changes February 2019

### Changed
- example/a2dp_sink_demo: use linear resampling to fix sample rate drift
- btstack_audio: split interface into sink and source

### Fixed
- Crypto: fix lockup when stack is shutdown while waiting for result of HCI Command, e.g. LE Read Local P256 Public Key
- SM: Avoid SM_EVENT_PAIRING_COMPLETE with ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION after successful pairing in responder role

### Added
- example/a2dp_sink_demo: add target role, support volume control on both devices
- example/audio_duplex: playback audio audio source on audio sink (test audio path)
- btstack_audio_embedded: implement audio source path

## Changes January 2019

### Changed
- L2CAP: provide channel mode (basic/ertm) and fcs option in L2CAP_EVENT_CHANNEL_OPENED 
- RFCOMM: support L2CAP ERTM. Callbacks passed to rfcomm_enable_l2cap_ertm() are used to manage ERTM buffers

### Added
- L2CAP: emit L2CAP_EVENT_ERTM_BUFFER_RELEASED if ERTM buffer not needed/used anymore
- L2CAP: add fcs_option to ERTM config l2cap_ertm_config_t
- HCI: validate advertisement data length field when generating GAP_EVENT_ADVERTISING_REPORT
- ad_parser: validate data element length fields in ad_iterator_has_more
- Raspberry Pi 3 A+/B+ port in port/raspi, starts without power cycle

### Fixed
- HCI: release outgoing buffer on disconnect if waiting to send another ACL fragment
- POSIX: use correct baudrate enums for baud rates higher than 921600 (Linux)
- Crypto: directly process queued crypto operation on HCI result

## Changes December 2018

### Added
- SM: generate and store ER / IR keys in TLV, unless manually set by application
- hci_dump: support PacketLogger or BlueZ format output via SEGGER RTT Channel 1 Up

### Fixed
- SM: fix internal buffer overrun during random address generation

## Changes November 2018

### Added
- GAP: gap_le_connection_interval provides connection interval for conn handle
- Nordic SPP Service Server: GATT service that emulates a serial port over BLE based on Nordic Semiconductor documentation.
- uBlox  SPP Service Server: GATT service that emulates a serial port over BLE based on uBlox documentation.
- SM: ENABLE_LE_CENTRAL_AUTO_ENCRYPTION triggers automatic encryption on connect to bonded devices
- SM: generate and store ER / IR keys in TLV, unless manually set by application

### Fixed
- SM: prevent random address updates if gap_random_address_set was used
- SM: fix internal buffer overrun that can cause storing of bonding information to fail
- SM: ignore Slave Security Request after sending own Pairing Request
- L2CAP: fix use after free on disconnect if ERTM is enabled
- HFP: Handle multiple commands/responses in single RFCOMM packet
- Memory Pools: clear all buffers before use

## Changes October 2018

### Added
- SDP Server: queue incoming connections when already connected instead of rejecting them
- GAP: Support enter/exit sniff mode via gap_sniff_mode_enter/exit. gap_set_default_link_policy_settings is needed to enable sniff mode in general.
- HIDS Device: GATT service that exposes HID reports intended for HID Devices, like keyboard and mouse.

### Fixed
- HCI: fix bug in gap_inquiry_stop that triggered additional GAP_EVENT_INQUIRY_COMPLETE instead of stopping the inquiry
- L2CAP: fix issue with outgoing connection before read remote supported complete when other channels exist
- L2CAP ERTM: allow SDU of szie MPS in first packet that contains L2CAP SDU Length
- L2CAP ERTM: fix memory corruption triggered if local_mtu > mps
- L2CAP ERTM: fix outgoing fragment management
- HFP: decline incoming RFCOMM connection after outgoing connection was started
- AVRCP: fix crash on disconnect of connection established by remote

## Changes September 2018

### Fixed
- HCI: Error creating outgoing connection (e.g. Connection Limit Exceeded) now handled
- L2CAP: Error creating outgoing connection (e.g. Connection Limit Exceeded) now handled
- L2CAP: Evaluate 'can send now' on HCI Disconnect as ACL buffers in Bluetooth Controller have been used for the closed connection are freed implicitly
- L2CAP: Check can send now before sending extended information requeste needed for ERTM mode
- L2CAP: Use valid signaling identifier for L2CAP Connection Parameter Update Request
- RFCOMM: Trigger l2cap request to send on rfcomm credits when client is waiting to sendtrigger l2cap request to send on rfcomm credits when client is waiting to send
- RFCOMM: Avoid use-after-free on channel finalize
- GATT Client: stop timer on disconnect - fixes use after free / crash

### Added
- A2DP Source: Support stream reconfiguration (a2dp_source_reconfigure_stream_sampling_frequency)
- 3rd-party: yxml is used for PBAP vCard list parsing
- cc256xC: new v1.1 init scripts

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
- Raspberry Pi 3 + Raspberry Pi Zero W port in port/raspi

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


