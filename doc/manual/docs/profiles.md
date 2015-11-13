

In the following, we explain how the various Bluetooth profiles are used
in BTstack.

## GAP - Generic Access Profile: Classic


The GAP profile defines how devices find each other and establish a
secure connection for other profiles. As mentioned before, the GAP
functionality is split between and . Please check both.

### Become discoverable

A remote unconnected Bluetooth device must be set as “discoverable” in
order to be seen by a device performing the inquiry scan. To become
discoverable, an application can call *hci_discoverable_control* with
input parameter 1. If you want to provide a helpful name for your
device, the application can set its local name by calling
$gap_set_local_name$. To save energy, you may set the device as
undiscoverable again, once a connection is established. See Listing
[below](#lst:Discoverable) for an example.

~~~~ {#lst:Discoverable .c caption="{Setting discoverable mode.}"}

    int main(void){
        ... 
        // make discoverable
        hci_discoverable_control(1);
        run_loop_execute(); 
        return 0;
    }
    void packet_handler (uint8_t packet_type, uint8_t *packet, uint16_t size){
         ...
         switch(state){
              case W4_CHANNEL_COMPLETE:
                  // if connection is successful, make device undiscoverable
                  hci_discoverable_control(0);
              ...
         }
     }
~~~~ 

### Discover remote devices {#sec:GAPdiscoverRemoteDevices}

To scan for remote devices, the *hci_inquiry* command is used. Found
remote devices are reported as a part of:

- HCI_EVENT_INQUIRY_RESULT,

- HCI_EVENT-_INQUIRY_RESULT_WITH_RSSI, or

- HCI_EVENT_EXTENDED_INQUIRY_RESPONSE events. 

Each response contains at least the Bluetooth address, the class of device, the page scan
repetition mode, and the clock offset of found device. The latter events
add information about the received signal strength or provide the
Extended Inquiry Result (EIR). A code snippet is shown in Listing
[below](#lst:DiscoverDevices).

~~~~ {#lst:DiscoverDevices .c caption="{Discover remote devices.}"}

    void print_inquiry_results(uint8_t *packet){
        int event = packet[0];
        int numResponses = packet[2];
        uint16_t classOfDevice, clockOffset;
        uint8_t  rssi, pageScanRepetitionMode;
        for (i=0; i<numResponses; i++){
            bt_flip_addr(addr, &packet[3+i*6]);
            pageScanRepetitionMode = packet [3 + numResponses*6 + i];
            if (event == HCI_EVENT_INQUIRY_RESULT){
                classOfDevice = READ_BT_24(packet, 3 + numResponses*(6+1+1+1) + i*3);
                clockOffset =   READ_BT_16(packet, 3 + numResponses*(6+1+1+1+3) + i*2) & 0x7fff;
                rssi  = 0;
            } else {
                classOfDevice = READ_BT_24(packet, 3 + numResponses*(6+1+1)     + i*3);
                clockOffset =   READ_BT_16(packet, 3 + numResponses*(6+1+1+3)   + i*2) & 0x7fff;
                rssi  = packet [3 + numResponses*(6+1+1+3+2) + i*1];
            }
            printf("Device found: %s with COD: 0x%06x, pageScan %u, clock offset 0x%04x, rssi 0x%02x\n", bd_addr_to_str(addr), classOfDevice, pageScanRepetitionMode, clockOffset, rssi);
        }
    }

    void packet_handler (uint8_t packet_type, uint8_t *packet, uint16_t size){
        ...
        switch (event) {
             case HCI_STATE_WORKING:
                hci_send_cmd(&hci_write_inquiry_mode, 0x01); // with RSSI
                break;
            case HCI_EVENT_COMMAND_COMPLETE:
                if (COMMAND_COMPLETE_EVENT(packet, hci_write_inquiry_mode) ) {
                    start_scan();
                }
            case HCI_EVENT_COMMAND_STATUS:
                if (COMMAND_STATUS_EVENT(packet, hci_write_inquiry_mode) ) {
                    printf("Ignoring error (0x%x) from hci_write_inquiry_mode.\n", packet[2]);
                    hci_send_cmd(&hci_inquiry, HCI_INQUIRY_LAP, INQUIRY_INTERVAL, 0);
                }
                break;
            case HCI_EVENT_INQUIRY_RESULT:
            case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:
                print_inquiry_results(packet);
                break;
           ...
        }
    }
~~~~ 

By default, neither RSSI values nor EIR are reported. If the Bluetooth
device implements Bluetooth Specification 2.1 or higher, the
*hci_write_inquiry_mode* command enables reporting of this advanced
features (0 for standard results, 1 for RSSI, 2 for RSSI and EIR).

A complete GAP inquiry example is provided [here](examples/generated/#sec:gapinquiryExample).

### Pairing of Devices

By default, Bluetooth communication is not authenticated, and any device
can talk to any other device. A Bluetooth device (for example, cellular
phone) may choose to require authentication to provide a particular
service (for example, a Dial-Up service). The process of establishing
authentication is called pairing. Bluetooth provides two mechanism for
this.

On Bluetooth devices that conform to the Bluetooth v2.0 or older
specification, a PIN code (up to 16 bytes ASCII) has to be entered on
both sides. This isn’t optimal for embedded systems that do not have
full I/O capabilities. To support pairing with older devices using a
PIN, see Listing [below](#lst:PinCodeRequest).

~~~~ {#lst:PinCodeRequest .c caption="{PIN code request.}"}

    void packet_handler (uint8_t packet_type, uint8_t *packet, uint16_t size){
        ...
        switch (event) {
            case HCI_EVENT_PIN_CODE_REQUEST:
                // inform about pin code request
                printf("Pin code request - using '0000'\n\r");
                bt_flip_addr(bd_addr, &packet[2]);
                
                // baseband address, pin length, PIN: c-string
                hci_send_cmd(&hci_pin_code_request_reply, &bd_addr, 4, "0000");
                break;
           ...
        }
    }
~~~~ 

The Bluetooth v2.1 specification introduces Secure Simple Pairing (SSP),
which is a better approach as it both improves security and is better
adapted to embedded systems. With SSP, the devices first exchange their
IO Capabilities and then settle on one of several ways to verify that
the pairing is legitimate. If the Bluetooth device supports SSP, BTstack
enables it by default and even automatically accepts SSP pairing
requests. Depending on the product in which BTstack is used, this may
not be desired and should be replaced with code to interact with the
user.

Regardless of the authentication mechanism (PIN/SSP), on success, both
devices will generate a link key. The link key can be stored either in
the Bluetooth module itself or in a persistent storage, see 
[here](porting/#sec:persistentStoragePorting). The next time the device connects and
requests an authenticated connection, both devices can use the
previously generated link key. Please note that the pairing must be
repeated if the link key is lost by one device.

### Dedicated Bonding

Aside from the regular bonding, Bluetooth also provides the concept of
“dedicated bonding”, where a connection is established for the sole
purpose of bonding the device. After the bonding process is over, the
connection will be automatically terminated. BTstack supports dedicated
bonding via the *gap_dedicated_bonding* function.

## SPP - Serial Port Profile


The SPP profile defines how to set up virtual serial ports and connect
two Bluetooth enabled devices.

### Accessing an SPP Server on a remote device

To access a remote SPP server, you first need to query the remote device
for its SPP services. Section [on querying remote SDP service](#sec:querySDPProtocols) 
shows how to query for all RFCOMM channels. For SPP, you can do the same 
but use the SPP UUID 0x1101 for the query. After you have identified the 
correct RFCOMM channel, you can create an RFCOMM connection as shown 
[here](protocols/#sec:rfcommClientProtocols).

### Providing an SPP Server

To provide an SPP Server, you need to provide an RFCOMM service with a
specific RFCOMM channel number as explained in section on 
[RFCOMM service](protocols/#sec:rfcommServiceProtocols). Then, you need to create 
an SDP record for it and publish it with the SDP server by calling
*sdp_register_service_internal*. BTstack provides the
*sdp_create_spp_service* function in that requires an empty buffer of
approximately 200 bytes, the service channel number, and a service name.
Have a look at the [SPP Counter example](examples/generated/#sec:sppcounterExample].


## PAN - Personal Area Networking Profile {#sec:panProfiles}


The PAN profile uses BNEP to provide on-demand networking capabilities
between Bluetooth devices. The PAN profile defines the following roles:

-   PAN User (PANU)

-   Network Access Point (NAP)

-   Group Ad-hoc Network (GN)

PANU is a Bluetooth device that communicates as a client with GN, or
NAP, or with another PANU Bluetooth device, through a point-to-point
connection. Either the PANU or the other Bluetooth device may terminate
the connection at anytime.

NAP is a Bluetooth device that provides the service of routing network
packets between PANU by using BNEP and the IP routing mechanism. A NAP
can also act as a bridge between Bluetooth networks and other network
technologies by using the Ethernet packets.

The GN role enables two or more PANUs to interact with each other
through a wireless network without using additional networking hardware.
The devices are connected in a piconet where the GN acts as a master and
communicates either point-to-point or a point-to-multipoint with a
maximum of seven PANU slaves by using BNEP.

Currently, BTstack supports only PANU.

### Accessing a remote PANU service

To access a remote PANU service, you first need perform an SDP query to
get the L2CAP PSM for the requested PANU UUID. With these two pieces of
information, you can connect BNEP to the remote PANU service with the
*bnep_connect* function. The Section on [PANU Demo example](#exaples/#sec:panudemoExample)
shows how this is accomplished.

### Providing a PANU service

To provide a PANU service, you need to provide a BNEP service with the
service UUID, e.g. the PANU UUID, and a a maximal ethernet frame size,
as explained in Section [on BNEP service](protocols/#sec:bnepServiceProtocols). Then, you need to
create an SDP record for it and publish it with the SDP server by
calling *sdp_register_service_internal*. BTstack provides the
*pan_create_panu_service* function in *src/pan.c* that requires an
empty buffer of approximately 200 bytes, a description, and a security
description.

## GAP LE - Generic Access Profile for Low Energy


As with GAP for Classic, the GAP LE profile defines how to discover and
how to connect to a Bluetooth Low Energy device. There are several GAP
roles that a Bluetooth device can take, but the most important ones are
the Central and the Peripheral role. Peripheral devices are those that
provide information or can be controlled. Central devices are those that
consume information or control the peripherals. Before the connection
can be established, devices are first going through an advertising
process.

### Private addresses.

To better protect privacy, an LE device can choose to use a private i.e.
random Bluetooth address. This address changes at a user-specified rate.
To allow for later reconnection, the central and peripheral devices will
exchange their Identity Resolving Keys (IRKs) during bonding. The IRK is
used to verify if a new address belongs to a previously bonded device.

To toggle privacy mode using private addresses, call the
*gap_random_address_set_mode* function. The update period can be set
with *gap_random_address_set_update_period*.

After a connection is established, the Security Manager will try to
resolve the peer Bluetooth address as explained in Section on 
[SMP](protocols/#sec:smpProtocols).

### Advertising and Discovery

An LE device is discoverable and connectable, only if it periodically
sends out Advertisements. An advertisement contains up to 31 bytes of
data. To configure and enable advertisement broadcast, the following GAP
functions can be used:

-   *gap_advertisements_set_data*

-   *gap_advertisements_set_params*

-   *gap_advertisements_enable*

Please have a look at the [SPP and LE
Counter example](examples/generated/#sec:sppandlecounterExample).

In addition to the Advertisement data, a device in the peripheral role
can also provide Scan Response data, which has to be explicitly queried
by the central device. It can be provided with the
*hci_le_set_scan_response_data*.

The scan parameters can be set with
*le_central_set_scan_parameters*. The scan can be started/stopped
with *le_central_start_scan*/*le_central_stop_scan*.

Finally, if a suitable device is found, a connection can be initiated by
calling *le_central_connect*. In contrast to Bluetooth classic, there
is no timeout for an LE connection establishment. To cancel such an
attempt, *le_central_connect_cancel* has be be called.

By default, a Bluetooth device stops sending Advertisements when it gets
into the Connected state. However, it does not start broadcasting
advertisements on disconnect again. To re-enable it, please send the
*hci_le_set_advertise_enable* again .

## GATT - Generic Attribute Profile


The GATT profile uses ATT Attributes to represent a hierarchical
structure of GATT Services and GATT Characteristics. Each Service has
one or more Characteristics. Each Characteristic has meta data attached
like its type or its properties. This hierarchy of Characteristics and
Services are queried and modified via ATT operations.

GATT defines both a server and a client role. A device can implement one
or both GATT roles.


### GATT Client {#sec:GATTClientProfiles}

The GATT Client is used to discover services, and their characteristics
and descriptors on a peer device. It can also subscribe for
notifications or indications that the characteristic on the GATT server
has changed its value.

To perform GATT queries, provides a rich interface. Before calling
queries, the GATT client must be initialized with *gatt_client_init*
once.

To allow for modular profile implementations, GATT client can be used
independently by multiple entities.

To use it by a GATT client, you register a packet handler with
*gatt_client_register_packet_ handler*. The return value of that is
a GATT client ID which has to be provided in all queries.

After an LE connection was created using the GAP LE API, you can query
for the connection MTU with *gatt_client_get_mtu*.

GATT queries cannot be interleaved. Therefore, you can check if you can
perform a GATT query on a particular connection using
*gatt_client_is_ready*. As a result to a GATT query, zero to many
*le_event*s are returned before a *GATT_QUERY_COMPLETE* event
completes the query.

For more details on the available GATT queries, please consult 
[GATT Client API](#sec:gattClientAPIAppendix).


### GATT Server {#sec:GATTServerProfiles}

The GATT server stores data and accepts GATT client requests, commands
and confirmations. The GATT server sends responses to requests and when
configured, sends indication and notifications asynchronously to the
GATT client.

To save on both code space and memory, BTstack does not provide a GATT
Server implementation. Instead, a textual description of the GATT
profile is directly converted into a compact internal ATT Attribute
database by a GATT profile compiler. The ATT protocol server -
implemented by and - answers incoming ATT requests based on information
provided in the compiled database and provides read- and write-callbacks
for dynamic attributes.

GATT profiles are defined by a simple textual comma separated value
(.csv) representation. While the description is easy to read and edit,
it is compact and can be placed in ROM.

The current format is shown in Listing [below](#lst:GATTServerProfile).

~~~~ {#lst:GATTServerProfile .c caption="{GATT profile.}"}

    PRIMARY_SERVICE, {SERVICE_UUID}
    CHARACTERISTIC, {ATTRIBUTE_TYPE_UUID}, {PROPERTIES}, {VALUE}
    CHARACTERISTIC, {ATTRIBUTE_TYPE_UUID}, {PROPERTIES}, {VALUE}
    ...
    PRIMARY_SERVICE, {SERVICE_UUID}
    CHARACTERISTIC, {ATTRIBUTE_TYPE_UUID}, {PROPERTIES}, {VALUE}
    ...
~~~~ 

Properties can be a list of READ $|$ WRITE $|$ WRITE_WITHOUT_RESPONSE
$|$ NOTIFY $|$ INDICATE $|$ DYNAMIC.

Value can either be a string (“this is a string”), or, a sequence of hex
bytes (e.g. 01 02 03).

UUIDs are either 16 bit (1800) or 128 bit
(00001234-0000-1000-8000-00805F9B34FB).

Reads/writes to a Characteristic that is defined with the DYNAMIC flag,
are forwarded to the application via callback. Otherwise, the
Characteristics cannot be written and it will return the specified
constant value.

Adding NOTIFY and/or INDICATE automatically creates an addition Client
Configuration Characteristic.

To require encryption or authentication before a Characteristic can be
accessed, you can add ENCRYPTION_KEY_SIZE_X - with $X \in [7..16]$ -
or AUTHENTICATION_REQUIRED.

BTstack only provides an ATT Server, while the GATT Server logic is
mainly provided by the GATT compiler. While GATT identifies
Characteristics by UUIDs, ATT uses Handles (16 bit values). To allow to
identify a Characteristic without hard-coding the attribute ID, the GATT
compiler creates a list of defines in the generated \*.h file.