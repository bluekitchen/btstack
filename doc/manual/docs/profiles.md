

In the following, we explain how the various Bluetooth profiles are used
in BTstack.

## GAP - Generic Access Profile: Classic


The GAP profile defines how devices find each other and establish a
secure connection for other profiles. As mentioned before, the GAP
functionality is split between and . Please check both.

### Become discoverable

A remote unconnected Bluetooth device must be set as “discoverable” in
order to be seen by a device performing the inquiry scan. To become
discoverable, an application can call *gap_discoverable_control* with
input parameter 1. If you want to provide a helpful name for your
device, the application can set its local name by calling
*gap_set_local_name*. To save energy, you may set the device as
undiscoverable again, once a connection is established. See Listing
[below](#lst:Discoverable) for an example.

~~~~ {#lst:Discoverable .c caption="{Setting discoverable mode.}"}
    int main(void){
        ...
        // make discoverable
        gap_discoverable_control(1);
        btstack_run_loop_execute();
        return 0;
    }
    void packet_handler (uint8_t packet_type, uint8_t *packet, uint16_t size){
         ...
         switch(state){
              case W4_CHANNEL_COMPLETE:
                  // if connection is successful, make device undiscoverable
                  gap_discoverable_control(0);
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
        int numResponses = hci_event_inquiry_result_get_num_responses(packet);
        uint16_t classOfDevice, clockOffset;
        uint8_t  rssi, pageScanRepetitionMode;
        for (i=0; i<numResponses; i++){
            bt_flip_addr(addr, &packet[3+i*6]);
            pageScanRepetitionMode = packet [3 + numResponses*6 + i];
            if (event == HCI_EVENT_INQUIRY_RESULT){
                classOfDevice = little_endian_read_24(packet, 3 + numResponses*(6+1+1+1) + i*3);
                clockOffset =   little_endian_read_16(packet, 3 + numResponses*(6+1+1+1+3) + i*2) & 0x7fff;
                rssi  = 0;
            } else {
                classOfDevice = little_endian_read_24(packet, 3 + numResponses*(6+1+1)     + i*3);
                clockOffset =   little_endian_read_16(packet, 3 + numResponses*(6+1+1+3)   + i*2) & 0x7fff;
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

A complete GAP inquiry example is provided [here](examples/examples/#sec:gapinquiryExample).

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
                hci_event_pin_code_request_get_bd_addr(packet, bd_addr);

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
two Bluetooth enabled devices. Please keep in mind that a serial port does not 
preserve packet boundaries if you try to send data as packets and read about
[RFCOMM packet boundaries]({protocols/#sec:noRfcommPacketBoundaries}).

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
*sdp_register_service*. BTstack provides the
*spp_create_sdp_record* function in that requires an empty buffer of
approximately 200 bytes, the service channel number, and a service name.
Have a look at the [SPP Counter example](examples/examples/#sec:sppcounterExample).


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
*bnep_connect* function. The Section on [PANU Demo example](examples/examples/#sec:panudemoExample)
shows how this is accomplished.

### Providing a PANU service

To provide a PANU service, you need to provide a BNEP service with the
service UUID, e.g. the PANU UUID, and a maximal ethernet frame size,
as explained in Section [on BNEP service](protocols/#sec:bnepServiceProtocols). Then, you need to
create an SDP record for it and publish it with the SDP server by
calling *sdp_register_service*. BTstack provides the
*pan_create_panu_sdp_record* function in *src/pan.c* that requires an
empty buffer of approximately 200 bytes, a description, and a security
description.

## HSP - Headset Profile

The HSP profile defines how a Bluetooth-enabled headset should communicate
with another Bluetooth enabled device. It relies on SCO for audio encoded
in 64 kbit/s CVSD and a subset of AT commands from GSM 07.07 for
minimal controls including the ability to ring, answer a call, hang up and adjust the volume.

The HSP defines two roles:
 - Audio Gateway (AG) - a device that acts as the gateway of the audio, typically a mobile phone or PC.
 - Headset (HS) - a device that acts as the AG's remote audio input and output control.

There are following restrictions:
- The CVSD is used for audio transmission.
- Between headset and audio gateway, only one audio connection at a time is supported.
- The profile offers only basic interoperability – for example, handling of multiple calls at the audio gateway is not included.
- The only assumption on the headset’s user interface is the possibility to detect a user initiated action (e.g. pressing a button).

%TODO: audio paths


## HFP - Hands-Free Profile

The HFP profile defines how a Bluetooth-enabled device, e.g. a car kit or a headset, can be used to place and receive calls via a audio gateway device, typically a mobile phone.
It relies on SCO for audio encoded in 64 kbit/s CVSD and a bigger subset of AT commands from GSM 07.07 then HSP for
controls including the ability to ring, to place and receive calls, join a conference call, to answer, hold or reject a call, and adjust the volume.

The HFP defines two roles:
- Audio Gateway (AG) – a device that acts as the gateway of the audio,, typically a mobile phone.
- Hands-Free Unit (HF) – a device that acts as the AG's remote audio input and output control.

%TODO: audio paths


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

In addition to the Advertisement data, a device in the peripheral role
can also provide Scan Response data, which has to be explicitly queried
by the central device. It can be set with *gap_scan_response_set_data*.

Please have a look at the [SPP and LE
Counter example](examples/examples/#sec:sppandlecounterExample).

The scan parameters can be set with
*gap_set_scan_parameters*. The scan can be started/stopped
with *gap_start_scan*/*gap_stop_scan*.

Finally, if a suitable device is found, a connection can be initiated by
calling *gap_connect*. In contrast to Bluetooth classic, there
is no timeout for an LE connection establishment. To cancel such an
attempt, *gap_connect_cancel* has be be called.

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

The GATT Client is used to discover services, characteristics
and their descriptors on a peer device. It allows to subscribe for
notifications or indications that the characteristic on the GATT server
has changed its value.

To perform GATT queries, it provides a rich interface. Before calling
queries, the GATT client must be initialized with *gatt_client_init*
once.

To allow for modular profile implementations, GATT client can be used
independently by multiple entities.

After an LE connection was created using the GAP LE API, you can query
for the connection MTU with *gatt_client_get_mtu*.

Multiple GATT queries to the same GATT Server cannot be interleaved.
Therefore, you can either use a state machine or similar to perform the 
queries in sequence, or you can check if you can perform a GATT query
on a particular connection right now using
*gatt_client_is_ready*, and retry later if it is not ready.
As a result to a GATT query, zero to many
*le_event*s are returned before a *GATT_EVENT_QUERY_COMPLETE* event
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
    // import service_name
    #import <service_name.gatt>

    PRIMARY_SERVICE, {SERVICE_UUID}
    CHARACTERISTIC, {ATTRIBUTE_TYPE_UUID}, {PROPERTIES}, {VALUE}
    CHARACTERISTIC, {ATTRIBUTE_TYPE_UUID}, {PROPERTIES}, {VALUE}
    ...
    PRIMARY_SERVICE, {SERVICE_UUID}
    CHARACTERISTIC, {ATTRIBUTE_TYPE_UUID}, {PROPERTIES}, {VALUE}
    ...
~~~~

UUIDs are either 16 bit (1800) or 128 bit
(00001234-0000-1000-8000-00805F9B34FB).

Value can either be a string (“this is a string”), or, a sequence of hex
bytes (e.g. 01 02 03).

Properties can be a list of properties combined using '|'

Reads/writes to a Characteristic that is defined with the DYNAMIC flag,
are forwarded to the application via callback. Otherwise, the
Characteristics cannot be written and it will return the specified
constant value.

Adding NOTIFY and/or INDICATE automatically creates an addition Client
Configuration Characteristic.

Property                | Meaning
------------------------|-----------------------------------------------
READ                    | Characteristic can be read
WRITE                   | Characteristic can be written using Write Request
WRITE_WITHOUT_RESPONSE  | Characteristic can be written using Write Command
NOTIFY                  | Characteristic allows notifications by server
INDICATE                | Characteristic allows indication by server
DYNAMIC                 | Read or writes to Characteristic are handled by application

To require encryption or authentication before a Characteristic can be
accessed, you can add one or more of the following properties:

Property                | Meaning
------------------------|-----------------------------------------------
AUTHENTICATION_REQUIRED | Read and Write operatsions require Authentication
READ_ENCRYPTED          | Read operations require Encryption
READ_AUTHENTICATED      | Read operations require Authentication
WRITE_ENCRYPTED         | Write operations require Encryption
WRITE_AUTHENTICATED     | Write operations require Authentication
ENCRYPTION_KEY_SIZE_X   | Require encryption size >= X, with W in [7..16]

To use already implemented GATT Services, you can import it
using the *#import <service_name.gatt>* command. See [list of provided services](gatt_services.md).

BTstack only provides an ATT Server, while the GATT Server logic is
mainly provided by the GATT compiler. While GATT identifies
Characteristics by UUIDs, ATT uses Handles (16 bit values). To allow to
identify a Characteristic without hard-coding the attribute ID, the GATT
compiler creates a list of defines in the generated \*.h file.

Similar to other protocols, it might be not possible to send any time.
To send a Notification, you can call *att_server_request_can_send_now*
to receive a ATT_EVENT_CAN_SEND_NOW event.

If your application cannot handle an ATT Read Request in the *att_read_callback*
in some situations, you can enable support for this by adding ENABLE_ATT_DELAYED_RESPONSE
to *btstack_config.h*. Now, you can store the requested attribute handle and return
*ATT_READ_RESPONSE_PENDING* instead of the length of the provided data when you don't have the data ready. 
For ATT operations that read more than one attribute, your *att_read_callback*
might get called multiple times as well. To let you know that all necessary
attribute handles have been 'requested' by the *att_server*, you'll get a final
*att_read_callback* with the attribute handle of *ATT_READ_RESPONSE_PENDING*.
When you've got the data for all requested attributes ready, you can call
*att_server_response_ready*, which will trigger processing of the current request.
Please keep in mind that there is only one active ATT operation and that it has a 30 second
timeout after which the ATT server is considered defunct by the GATT Client.

### Implementing Standard GATT Services {#sec:GATTStandardServices}

Implementation of a standard GATT Service consists of the following 4 steps:

  1. Identify full Service Name
  2. Use Service Name to fetch XML definition at Bluetooth SIG site and convert into generic .gatt file
  3. Edit .gatt file to set constant values and exclude unwanted Characteristics
  4. Implement Service server, e.g., battery_service_server.c

Step 1:

To facilitate the creation of .gatt files for standard profiles defined by the Bluetooth SIG,
the *tool/convert_gatt_service.py* script can be used. When run without a parameter, it queries the
Bluetooth SIG website and lists the available Services by their Specification Name, e.g.,
*org.bluetooth.service.battery_service*.

    $ tool/convert_gatt_service.py
    Fetching list of services from https://www.bluetooth.com/specifications/gatt/services

    Specification Type                                     | Specification Name            | UUID
    -------------------------------------------------------+-------------------------------+-----------
    org.bluetooth.service.alert_notification               | Alert Notification Service    | 0x1811
    org.bluetooth.service.automation_io                    | Automation IO                 | 0x1815
    org.bluetooth.service.battery_service                  | Battery Service               | 0x180F
    ...
    org.bluetooth.service.weight_scale                     | Weight Scale                  | 0x181D

    To convert a service into a .gatt file template, please call the script again with the requested Specification Type and the output file name
    Usage: tool/convert_gatt_service.py SPECIFICATION_TYPE [service_name.gatt]

Step 2:

To convert service into .gatt file, call *tool/convert_gatt_service.py with the requested Specification Type and the output file name.

    $ tool/convert_gatt_service.py org.bluetooth.service.battery_service battery_service.gatt
    Fetching org.bluetooth.service.battery_service from
    https://www.bluetooth.com/api/gatt/xmlfile?xmlFileName=org.bluetooth.service.battery_service.xml

    Service Battery Service
    - Characteristic Battery Level - properties ['Read', 'Notify']
    -- Descriptor Characteristic Presentation Format       - TODO: Please set values
    -- Descriptor Client Characteristic Configuration

    Service successfully converted into battery_service.gatt
    Please check for TODOs in the .gatt file


Step 3:

In most cases, you will need to customize the .gatt file. Please pay attention to the tool output and have a look
at the generated .gatt file.

E.g. in the generated .gatt file for the Battery Service

    // Specification Type org.bluetooth.service.battery_service
    // https://www.bluetooth.com/api/gatt/xmlfile?xmlFileName=org.bluetooth.service.battery_service.xml

    // Battery Service 180F
    PRIMARY_SERVICE, ORG_BLUETOOTH_SERVICE_BATTERY_SERVICE
    CHARACTERISTIC, ORG_BLUETOOTH_CHARACTERISTIC_BATTERY_LEVEL, DYNAMIC | READ | NOTIFY,
    // TODO: Characteristic Presentation Format: please set values
    #TODO CHARACTERISTIC_FORMAT, READ, _format_, _exponent_, _unit_, _name_space_, _description_
    CLIENT_CHARACTERISTIC_CONFIGURATION, READ | WRITE,

you could delete the line regarding the CHARACTERISTIC_FORMAT, since it's not required if there is a single instance of the service.
Please compare the .gatt file against the [Adopted Specifications](https://www.bluetooth.com/specifications/adopted-specifications).

Step 4:

As described [above](#sec:GATTServerProfiles) all read/write requests are handled by the application.
To implement the new services as a reusable module, it's neccessary to get access to all read/write requests related to this service.

For this, the ATT DB allows to register read/write callbacks for a specific handle range with *att_server_register_can_send_now_callback()*.

Since the handle range depends on the application's .gatt file, the handle range for Primary and Secondary Services can be queried with *gatt_server_get_get_handle_range_for_service_with_uuid16*.

Similarly, you will need to know the attribute handle for particular Characteristics to handle Characteristic read/writes requests. You can get the attribute value handle for a Characteristics *gatt_server_get_value_handle_for_characteristic_with_uuid16()*.

In addition to the attribute value handle, the handle for the Client Characteristic Configuration is needed to support Indications/Notifications. You can get this attribute handle with *gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16()*

Finally, in order to send Notifications and Indications independently from the main application, *att_server_register_can_send_now_callback* can be used to request a callback when it's possible to send a Notification or Indication.

To see how this works together, please check out the Battery Service Server in *src/ble/battery_service_server.c*.
