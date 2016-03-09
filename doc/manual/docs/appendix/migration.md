BTstack has grown organically since its creation and mostly kept the APIs compatible.
After the second listing with the Bluetooth SIG, we decided that it's time for a major overhaul of the API - making easier for new users. This version will be released as v1.0.

In the following, we provide an overview of the changes and guidelines for updating an existing code base.

# Changes

## Project structure

### include folder
The header files had been split between *src/* and *include/btstack*/. Now, the *include/* folder is gone and the headers are provided in *src/*, *src/classic/*, *src/ble/* or by the *platform/* folders. There's a new "btstack.h" header that includes all BTstack headers.

### Defines
All defines that originate from the Bluetooth Specification are now in *src/bluetooth.h*. BTstack defines are collected in *src/btstack_defines.h*

### Plural folder names
Folder with plural names have been renamed to singular, examples: *doc*, *chipset*, *platform*, *tool*.

### ble and src folders
The *ble* folder has been renamed into *src/ble*. Files only relevant for using BTstack in Classic mode, have been moved to *src/classic*. Files in *src* that are specific to individual platforms are moved to *platform* or *port*.

### platform and port
The *platforms* folder has been cleaned up. Now, the *port* folder contains a complete port of BTstack for a specific setup consisting of a MCU and a Bluetooth chipset. Code that didn't belong to the BTstack core in *src* have been moved to *platform* or *port*

## Function and file names
- The _internal suffix has been an artefact from the iOS port. All public functions with the _internal suffix have been stripped of this suffix.
- Types with generic names like linked_list have been prefixed with btstack_ (e.g. btstack_linked_list) to avoid problems when integrating with other environments like vendor SDKs.

## Event names
All events names have the form MODULE_EVENT_NAME now.

## Packet Handlers
We streamlined the use of packet handlers and their types. Most callbacks are now of type *btstack_packet_handler_t* and receive a pointer to an HCI Event packet. Often a void * connection was the first argument - this has been removed resp. pushed up into the BTstack Daemon.

To facilitate working with HCI Events and get rid of manually calculating offsets into packets, we're providing the auto-generated getters for all fields of all elements in *src/hci_event.h*. All functions are defined as static inline, so they are not wasting any program memory.

Feel free to start using these getters as needed. 

## Event forwarding
In the past, events have been forwarded up the stack with the effect that an app that used the *att_server* and the *security_manager* would get each HCI event twice. This has been cleaned up by providing a way to register multiple listeners for HCI events. 	

## remote_device_db.h
Has been split into *src/classic/btstack_link_key_db*, *platform/daemon/btstack_device_name_db*, and *platform/daemon/rfcomm_service_db*.

## bt_control.h
Has been split into *src/btstack_chipset.h* and *src/bstack_control.h*

## Security Manager
- In all Security Manager calls that refer to an active connection, pass in the current handle instead of addr + addr type.
- All Security Manager events are now regular HCI Events instead of sm_* structs
- Multiple packet handler can be registered with sm_add_event_handler()

## RFCOMM
- Packet handler per service and outgoing connection

## HCI / GAP
- In hci_init() only the HCI Transport and it's configuration have to be provided. A link key db can be set via hci_set_link_key_db. Similar, hci_set_chipset() and hci_set_control can be called if needed.
- HCI Event handlers can be registered with hci_add_event_handler
- HCI functions that are commonly placed in GAP have been moved from *src/hci.h* to *src/gap.h*

## btstack_util.h
- The functions to read/store values in little/bit endian have been renamed into bit/little_endian_read/write_16/24/32.
- swapX -> reverse_x

## Linked List
- Renamed to btstack_linked_list
- Removed user data field

## Timers
- Added context field

## Run Loop
- Run loop type selection via implementation run_loop_t struct (see run_loop_NAME_instance())

## GATT Client
- All GATT Client events are now regular HCI Events instead of gatt_* structs.
- The subclient_id has been replaced by a complete handler for GATT Client requests

## ANCS Client
Renamed to *src/ble/ancs_client*

## SDP Server
- Has been renamed to */src/sdp_server*.
- The API for adding a service record was different for 

## SDP Client
- SDP Query structs have been repalced by HCI events


## btstack_config.h
- *btstack-config.h* is now *btstack_config.h*
- Defines have been sorted. HAVE_ specify features that are particular to your port. ENABLE_ features can then be added as needed
- EMBEDDED dropped

## Flow control / DAEMON_EVENT_PACKET_SENT
- TODO -- don't hand out local credits

## Daemon
- Moved into *platform/daemon/*. Not tested yet.
- Header for clients: *platform/daemon/btstack_client.h*
- Java bindings are now at *platform/daemon/bindings*

## Unsorted



# Migration

