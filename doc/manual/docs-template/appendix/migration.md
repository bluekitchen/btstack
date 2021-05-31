We already had two listings with the Bluetooth SIG, but no official BTstack v1.0 release. After the second listing, we decided that it's time for a major overhaul of the API - making it easier for new users. 

In the following, we provide an overview of the changes and guidelines for updating an existing code base. At the end, we present a command line tool and as an alternative a Web service, that can simplify the migration to the new v1.0 API.

# Changes

## Repository structure

### *include/btstack* folder
The header files had been split between *src/* and *include/btstack*/. Now, the *include/* folder is gone and the headers are provided in *src/*, *src/classic/*, *src/ble/* or by the *platform/* folders. There's a new *src/btstack.h* header that includes all BTstack headers.

### Plural folder names
Folder with plural names have been renamed to the singular form, examples: *doc*, *chipset*, *platform*, *tool*.

### ble and src folders
The *ble* folder has been renamed into *src/ble*. Files that are relevant for using BTstack in Classic mode, have been moved to *src/classic*. Files in *src* that are specific to individual platforms have been moved to *platform* or *port*.

### platform and port folders
The *platforms* folder has been split into *platform* and "port" folders. The *port* folder contains a complete BTstack port of for a specific setup consisting of an MCU and a Bluetooth chipset. Code that didn't belong to the BTstack core in *src* have been moved to *platform* or *port* as well, e.g. *hci_transport_h4_dma.c*.

### Common file names
Types with generic names like *linked_list* have been prefixed with btstack_ (e.g. btstack_linked_list.h) to avoid problems when integrating with other environments like vendor SDKs.

## Defines and event names
All defines that originate from the Bluetooth Specification are now in *src/bluetooth.h*. Addition defines by BTstack are collected in *src/btstack_defines.h*. All events names have the form MODULE_EVENT_NAME now.

## Function names
- The _internal suffix has been an artifact from the iOS port. All public functions with the _internal suffix have been stripped of this suffix.
- Types with generic names like linked_list have been prefixed with btstack_ (e.g. btstack_linked_list) to avoid problems when integrating with other environments like vendor SDKs.

## Packet Handlers
We streamlined the use of packet handlers and their types. Most callbacks are now of type *btstack_packet_handler_t* and receive a pointer to an HCI Event packet. Often a void * connection was the first argument - this has been removed.

To facilitate working with HCI Events and get rid of manually calculating offsets into packets, we're providing auto-generated getters for all fields of all events in *src/hci_event.h*. All functions are defined as static inline, so they are not wasting any program memory if not used. If used, the memory footprint should be identical to accessing the field directly via offsets into the packet. Feel free to start using these getters as needed. 

## Event Forwarding
In the past, events have been forwarded up the stack from layer to layer, with the undesired effect that an app that used both *att_server* and *security_manager* would get each HCI event twice. To fix this and other potential issues, this has been cleaned up by providing a way to register multiple listeners for HCI events. If you need to receive HCI or GAP events, you can now directly register your callback with *hci_add_event_handler*.	

## util folder
The *utils* folder has been renamed into *btstack_util* to avoid compile issues with other frameworks.

- The functions to read/store values in little/bit endian have been renamed into big/little_endian_read/write_16/24/32.
- The functions to reverse bytes swap16/24/32/48/64/128/X habe been renamed to reverse_16/24/32/48/64/128/X.

## btstack_config.h
- *btstack-config.h* is now *btstack_config.h*
- Defines have been sorted: HAVE_ specify features that are particular to your port. ENABLE_ features can be added as needed.
- _NO_ has been replaced with _NR_ for the BTstack static memory allocation, e.g., *MAX_NO_HCI_CONNECTIONS8 -> MAX_NR_HCI_CONNECTIONS*
- The #define EMBEDDED is dropped, i.e. the distinction if the API is for embedded or not has been removed. 

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
- Has been renamed to *src/classic/sdp_server.h*.
- The distinction if the API is for embedded or not has been removed. 

## Security Manager
- In all Security Manager calls that refer to an active connection, pass in the current handle instead of addr + addr type.
- All Security Manager events are now regular HCI Events instead of sm_* structs
- Multiple packet handler can be registered with *sm_add_event_handler(...)* similar to HCI.

## GATT Client
- All GATT Client events are now regular HCI Events instead of gatt_* structs.
- The subclient_id has been replaced by a complete handler for GATT Client requests

## ANCS Client
Renamed to *src/ble/ancs_client*

## Flow control / DAEMON_EVENT_HCI_PACKET_SENT

In BTstack, you can only send a packet with most protocols (L2CAP, RFCOMM, ATT) if the outgoing buffer is free and also per-protocol constraints are satisfied, e.g., there are outgoing credits for RFCOMM.

Before v1.0, we suggested to check with *l2cap_can_send_packet_now(..)* or *rfcomm_can_send_packet(..)* whenever an HCI event was received. This has been cleaned up and streamlined in v1.0. 

Now, when there is a need to send a packet, you can call *rcomm_request_can_send_now(..)* / *l2cap_request_can_send_now(..)* to let BTstack know that you want to send a packet. As soon as it becomes possible to send a RFCOMM_EVENT_CAN_SEND_NOW/L2CAP_EVENT_CAN_SEND_NOW will be emitted and you can directly react on that and send a packet. After that, you can request another "can send event" if needed.


## Daemon
- Not tested since API migration!
- Moved into *platform/daemon/*
- Header for clients: *platform/daemon/btstack_client.h*
- Java bindings are now at *platform/daemon/bindings*

## Migration to v1.0 with a script

To simplify the migration to the new v1.0 API, we've provided a tool in *tool/migration_to_v1.0* that fixes include path changes, handles all function renames and also updates the packet handlers to the btstack_packet_handler_t type. It also has a few rules that try to rename file names in Makefile as good as possible.

It's possible to migrate most of the provided embedded examples to v1.0. The one change that it cannot handle is the switch from structs to HCI Events for the SDP, GATT, and ANCS clients. The migration tool updates the packet handler signature to btstack_packet_handler_t, but it does not convert the field accesses to sue the appropriate getters in *btstack_event.h*. This has to be done manually, but it is straight forward. E.g., to read the field *status* of the GATT_EVENT_QUERY_COMPLETE, you call call gatt_event_query_complete_get_status(packet).

### Requirements

- bash
- sed
- [Coccinelle](http://coccinelle.lip6.fr/). On Debian-based distributions, it's available via apt. On OS X, it can be installed via Homebrew.

### Usage
 
    tool/migration_to_v1.0/migration.sh PATH_TO_ORIGINAL_PROJECT PATH_TO_MIGRATED_PROJECT

The tool first creates a copy of the original project and then uses sed and coccinelle to update the source files.

## Migration to v1.0 with a Web Service

BlueKitchen GmbH is providing a [web service](http://buildbot.bluekitchen-gmbh.com/migration) to help migrate your sources if you don't want or cannot install Coccinelle yourself.
