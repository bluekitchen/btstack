
BTstack has grown organically since its creation and mostly kept the API compatible.
After the second listing with the Bluetooth SIG, we decided that it's time for a major overhaul of the API - making easier for new users. This version will be released as v1.0.

In the following, we provide an overview of the changes and guidelines for updating an existing code base.

# Changes

## Repository structure

### *include/btstack* folder
The header files had been split between *src/* and *include/btstack*/. Now, the *include/* folder is gone and the headers are provided in *src/*, *src/classic/*, *src/ble/* or by the *platform/* folders. There's a new "btstack.h" header that includes all BTstack headers.

### Plural folder names
Folder with plural names have been renamed to the singular form, examples: *doc*, *chipset*, *platform*, *tool*.

### ble and src folders
The *ble* folder has been renamed into *src/ble*. Files only relevant for using BTstack in Classic mode, have been moved to *src/classic*. Files in *src* that are specific to individual platforms have been moved to *platform* or *port*.

### platform and port
The *platforms* folder has been cleaned up. Now, the *port* folder contains a complete BTstack port of for a specific setup consisting of an MCU and a Bluetooth chipset. Code that didn't belong to the BTstack core in *src* have been moved to *platform* or *port* as well, e.g. *hci_transport_h4_dma.c*.

### Common file names
Types with generic names like linked_list have been prefixed with btstack_ (e.g. btstack_linked_list.h) to avoid problems when integrating with other environments like vendor SDKs.

## Defines and event names
All defines that originate from the Bluetooth Specification are now in *src/bluetooth.h*. Addition defines by BTstack are collected in *src/btstack_defines.h*. All events names have the form MODULE_EVENT_NAME now.

## Function names
- The _internal suffix has been an artefact from the iOS port. All public functions with the _internal suffix have been stripped of this suffix.
- Types with generic names like linked_list have been prefixed with btstack_ (e.g. btstack_linked_list) to avoid problems when integrating with other environments like vendor SDKs.

## Packet Handlers
We streamlined the use of packet handlers and their types. Most callbacks are now of type *btstack_packet_handler_t* and receive a pointer to an HCI Event packet. Often a void * connection was the first argument - this has been removed.

To facilitate working with HCI Events and get rid of manually calculating offsets into packets, we're providing auto-generated getters for all fields of all elements in *src/hci_event.h*. All functions are defined as static inline, so they are not wasting any program memory if not used. If used, the memory footprint should be identical to accessing the field directly via offsets into the packet. Feel free to start using these getters as needed. 

## Event Forwarding
In the past, events have been forwarded up the stack from layer to layer, with the undesired effect that an app that used both *att_server* and *security_manager* would get each HCI event twice. To fix this and other potential issues, this has been cleaned up by providing a way to register multiple listeners for HCI events. If you need to receive HCI or GAP events, you can now directly register your callback with *hci_add_event_handler*.	

## util
Utils has been renamed into btstack_util to avoid compile issues with other frameworks.

- The functions to read/store values in little/bit endian have been renamed into bit/little_endian_read/write_16/24/32.
- The functions to reverse bytes swap16/24/32/48/64/128/X habe been renamed to reverse_16/24/32/48/64/128/X.

## btstack_config.h
- *btstack-config.h* is now *btstack_config.h*
- Defines have been sorted: HAVE_ specify features that are particular to your port. ENABLE_ features can then be added as needed.
- _NO_ has been replaced with _NR_ for the BTstack static memory allocation, e.g., *MAX_NO_HCI_CONNECTIONS8 -> *MAX_NR_HCI_CONNECTIONS*
- The #define EMBEDDED was dropped. The signature for sdp_register_service did differ with/without EMBEDDED being defined or not.

## Linked List
- The file has been renamed to btstack_linked_list.
- All functions are prefixed with btstack_.
- The user data field has been removed as well.

## Run Loop
- The files have been renamed to *btstack_run_loop_*
- To allow for simpler integration of custom run loop implementations, *run_loop_init(...)* is now called with a pointer to a *run_loop_t* function table instead of using an enum that needs to be defined inside the BTstack sources.
- Timers now have a new context field that can be used by the user.

## HCI Setup
In the past, hci_init(...) was called with an hci_transport_t, the transport configuration, remote_device_t and a bt_control_t objects. Besides cleaning up the types (see remote_device_db and bt_control below), hci_init only requires the hci_transport_t and it's configuration.

The used *btstack_chipset_t*, *bt_control_t*, or *link_key_db_t* can now be set with *hci_set_chipset*, *hci_set_control*, and *hci_set_link_key_db* respectively.

### remote_device_db
Has been split into *src/classic/btstack_link_key_db*, *platform/daemon/btstack_device_name_db*, and *platform/daemon/rfcomm_service_db*.

### bt_control
Has been split into *src/btstack_chipset.h* and *src/bstack_control.h*

## HCI / GAP
HCI functions that are commonly placed in GAP have been moved from *src/hci.h* to *src/gap.h*

## RFCOMM
In contrast to L2CAP, RFCOMM did not allow to register individual packet handlers for each service and outgoing connection. This has been fixed and the general *rfcomm_register_packet_handler(...)* has been removed.

## SPP Server
The function to create an SPP SDP record has been moved into *spp_server.h*

## SDP Client
- SDP Query structs have been replaced by HCI events. You can use btstack_event.h to access the fields.

## SDP Server
- Has been renamed to *src/sdp_server*.
- The API for adding a service record was different for 

## Security Manager
- In all Security Manager calls that refer to an active connection, pass in the current handle instead of addr + addr type.
- All Security Manager events are now regular HCI Events instead of sm_* structs
- Multiple packet handler can be registered with *sm_add_event_handler(...)* similar to HCI.

## GATT Client
- All GATT Client events are now regular HCI Events instead of gatt_* structs.
- The subclient_id has been replaced by a complete handler for GATT Client requests

## ANCS Client
Renamed to *src/ble/ancs_client*

## Flow control / DAEMON_EVENT_PACKET_SENT
When an application was allowed to send the next L2CAP or RFCOMM packet haven't been clear in the past and we suggested to check with l2cap_can_send_packet_now(..) or rfcomm_can_send_packet(..) whenever an HCI event was received. This has been cleaned up and streamlined as well in v1.0. 

Both L2CAP and RFCOMM now emit a L2CAP_EVENT_CAN_SEND_NOW or a RFCOMM_EVENT_CAN_SEND_NOW after the connection gets established. The app can now send a packet if it is ready to sent. If the app later wants to send, it has to call l2cap/rfcomm_can_send_packet_now(..). If the result is negative, BTstack will emit the L2CAP/RCOMM_EVENT_CAN_SEND_NOW as soon as sending becomes possible again. We hope that this design leads to simpler application code. 

## Daemon
- Not tested since API migration!
- Moved into *platform/daemon/*
- Header for clients: *platform/daemon/btstack_client.h*
- Java bindings are now at *platform/daemon/bindings*

# Migration to v1.0


To simplify the migration to the new v1.0 API, we've provided a tool in *tool/migration_to_v1.0* that fixes include path changes, handles all function renames and also updates the packet handlers to the btstack_packet_handler_t type. It also has a few rules that try to rename file names in Makefile as good as possible.

It's possible to migrate most of the provided embedded exmpamples to v1.0. The one change that it cannot handle is the switch from structs to HCI Events for the SDP, GATT, and ANCS clients. The migration tool updates the packet handler signature to btstack_packet_handler_t, but it does not convert the field accesses to sue the appropriate getters in *btstack_event.h*. This has to be done manually, but it is straight forward. E.g., to read the field *status* of the GATT_EVENT_QUERY_COMPLETE, you call call gatt_event_query_complete_get_status(packet).

## Requirements

- bash
- sed
- [Coccinelle](http://coccinelle.lip6.fr/). On Debian-based distributions, it's available via apt. On OS X, it can be installed via Homebrew.

## Usage
 
    tool/migration_to_v1.0/migration.sh PATH_TO_ORIGINAL_PROJECT PATH_TO_MIGRATED_PROJECT

The tool first creates a copy of the original project and then uses sed and coccinelle to update the source files.

## Web Service

BlueKitchen GmbH is providing a [web service](http://buildbot.bluekitchen-gmbh.com/migration) to help migrate your sources if you don't want or cannot install Coccinelle yourself.
