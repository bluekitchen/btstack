#

In the following, we explain how the various Bluetooth profiles are used
in BTstack.

## A2DP - Advanced Audio Distribution

The A2DP profile defines how to stream audio over a Bluetooth connection from one device, such as a mobile phone, to another device such as a headset.  A device that acts as source of audio stream implements the A2DP Source role. Similarly, a device that receives an audio stream implements the A2DP Sink role. As such, the A2DP service allows uni-directional transfer of an audio stream, from single channel mono, up to two channel stereo. Our implementation includes mandatory support for the low-complexity SBC codec. Signaling for optional codes (FDK AAC, LDAC, APTX) is supported as well, by you need to provide your own codec library.


## AVRCP - Audio/Video Remote Control Profile

The AVRCP profile defines how audio playback on a remote device (e.g. a music app on a smartphone) can be controlled as well as how to state changes such as volume, information on currently played media, battery, etc. can be received from a remote device (e.g. a speaker). Usually, each device implements two roles: 
- The Controller role allows to query information on currently played media, such are title, artist and album, as well as to control the playback, i.e. to play, stop, repeat, etc. 
- The Target role responds to commands, e.g. playback control, and queries, e.g. playback status, media info, from the Controller currently played media.


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

A complete GAP inquiry example is provided [here](../examples/examples/#sec:gapinquiryExample).

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
[here](../porting/#sec:persistentStoragePorting). The next time the device connects and
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
[RFCOMM packet boundaries](../protocols/#sec:noRfcommPacketBoundaries).

### Accessing an SPP Server on a remote device

To access a remote SPP server, you first need to query the remote device
for its SPP services. Section [on querying remote SDP service](#sec:querySDPProtocols)
shows how to query for all RFCOMM channels. For SPP, you can do the same
but use the SPP UUID 0x1101 for the query. After you have identified the
correct RFCOMM channel, you can create an RFCOMM connection as shown
[here](../protocols/#sec:rfcommClientProtocols).

### Providing an SPP Server

To provide an SPP Server, you need to provide an RFCOMM service with a
specific RFCOMM channel number as explained in section on
[RFCOMM service](../protocols/#sec:rfcommServiceProtocols). Then, you need to create
an SDP record for it and publish it with the SDP server by calling
*sdp_register_service*. BTstack provides the
*spp_create_sdp_record* function in that requires an empty buffer of
approximately 200 bytes, the service channel number, and a service name.
Have a look at the [SPP Counter example](../examples/examples/#sec:sppcounterExample).


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
*bnep_connect* function. The Section on [PANU Demo example](../examples/examples/#sec:panudemoExample)
shows how this is accomplished.

### Providing a PANU service

To provide a PANU service, you need to provide a BNEP service with the
service UUID, e.g. the PANU UUID, and a maximal ethernet frame size,
as explained in Section [on BNEP service](../protocols/#sec:bnepServiceProtocols). Then, you need to
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


## HFP - Hands-Free Profile  {#sec:hfp}

The HFP profile defines how a Bluetooth-enabled device, e.g. a car kit or a headset, can be used to place and receive calls via a audio gateway device, typically a mobile phone.
It relies on SCO for audio encoded in 64 kbit/s CVSD and a bigger subset of AT commands from GSM 07.07 then HSP for
controls including the ability to ring, to place and receive calls, join a conference call, to answer, hold or reject a call, and adjust the volume.

The HFP defines two roles:

- Audio Gateway (AG) – a device that acts as the gateway of the audio,, typically a mobile phone.

- Hands-Free Unit (HF) – a device that acts as the AG's remote audio input and output control.

### Supported Features {#sec:hfpSupportedFeatures}

The supported features define the HFP capabilities of the device. The enumeration unfortunately differs between HF and AG sides.

The AG supported features are set by combining the flags that start with HFP_AGSF_xx and calling hfp_ag_init_supported_features, followed by creating SDP record for the service using the same feature set.

Similarly, the HF supported features are a combination of HFP_HFSF_xx flags and are configured by calling hfp_hf_init_supported_features, as well as creating an SDP record.

| Define for AG Supported Feature         |  Description  |
| --------------------------------------- |  ----------------------------  |
| HFP_AGSF_THREE_WAY_CALLING              |  Three-way calling             |
| HFP_AGSF_EC_NR_FUNCTION                 |  Echo Canceling and/or Noise Reduction function |
| HFP_AGSF_VOICE_RECOGNITION_FUNCTION     |  Voice recognition function |
| HFP_AGSF_IN_BAND_RING_TONE              |  In-band ring tone capability |
| HFP_AGSF_ATTACH_A_NUMBER_TO_A_VOICE_TAG |  Attach a number to a voice tag |
| HFP_AGSF_ABILITY_TO_REJECT_A_CALL       |  Ability to reject a call |
| HFP_AGSF_ENHANCED_CALL_STATUS           |  Enhanced call status |
| HFP_AGSF_ENHANCED_CALL_CONTROL          |  Enhanced call control |
| HFP_AGSF_EXTENDED_ERROR_RESULT_CODES    |  Extended Error Result Codes |
| HFP_AGSF_CODEC_NEGOTIATION              |  Codec negotiation |
| HFP_AGSF_HF_INDICATORS                  |  HF Indicators |
| HFP_AGSF_ESCO_S4                        |  eSCO S4 (and T2) Settings Supported |
| HFP_AGSF_ENHANCED_VOICE_RECOGNITION_STATUS |  Enhanced voice recognition status |
| HFP_AGSF_VOICE_RECOGNITION_TEXT         |  Voice recognition text |


| Define for HF Supported Feature         |  Description  |
| --------------------------------------- |  ----------------------------  |
| HFP_HFSF_THREE_WAY_CALLING              |  Three-way calling  |
| HFP_HFSF_EC_NR_FUNCTION                 |  Echo Canceling and/or Noise Reduction function |
| HFP_HFSF_CLI_PRESENTATION_CAPABILITY    |  CLI presentation capability |
| HFP_HFSF_VOICE_RECOGNITION_FUNCTION     |  Voice recognition function |
| HFP_HFSF_REMOTE_VOLUME_CONTROL          |  Remote volume control |
| HFP_HFSF_ATTACH_A_NUMBER_TO_A_VOICE_TAG |  Attach a number to a voice tag |
| HFP_HFSF_ENHANCED_CALL_STATUS           |  Enhanced call status |
| HFP_HFSF_ENHANCED_CALL_CONTROL          |  Enhanced call control |
| HFP_HFSF_CODEC_NEGOTIATION              |  Codec negotiation |
| HFP_HFSF_HF_INDICATORS                  |  HF Indicators |
| HFP_HFSF_ESCO_S4                        |  eSCO S4 (and T2) Settings Supported |
| HFP_HFSF_ENHANCED_VOICE_RECOGNITION_STATUS |  Enhanced voice recognition status |
| HFP_HFSF_VOICE_RECOGNITION_TEXT         |  Voice recognition text |


### Audio Voice Recognition Activation {#sec:hfpAVRActivation}

Audio voice recognition (AVR) requires that HF and AG have the following  features enabled: 

- HF: HFP_HFSF_VOICE_RECOGNITION_FUNCTION and

- AG: HFP_AGSF_VOICE_RECOGNITION_FUNCTION. 
 
It can be activated or deactivated on both sides by calling:

    // AG
    uint8_t hfp_ag_activate_voice_recognition(hci_con_handle_t acl_handle);
    uint8_t hfp_ag_deactivate_voice_recognition(hci_con_handle_t acl_handle);

    // HF
    uint8_t hfp_hf_activate_voice_recognition(hci_con_handle_t acl_handle);
    uint8_t hfp_hf_deactivate_voice_recognition(hci_con_handle_t acl_handle);

On activation change, the HFP_SUBEVENT_VOICE_RECOGNITION_(DE)ACTIVATED event will be emitted with status field set to ERROR_CODE_SUCCESS on success. 

Voice recognition will stay active until either the deactivation command is called, or until the current Service Level Connection between the AG and the HF is dropped for any reason.

| Use cases                                              |  Expected behavior  |
|--------------------------------------------------------|---------------------|
| No previous audio connection, AVR activated then deactivated | Audio connection will be opened by AG upon AVR activation, and upon AVR deactivation closed|
| AVR activated and deactivated during existing audio connection | Audio remains active upon AVR deactivation |
| Call to close audio connection during active AVR session       | The audio connection shut down will be refused |
| AVR activated, but audio connection failed to be established   | AVR will stay activated |

Beyond the audio routing and voice recognition activation capabilities, the rest of the voice recognition functionality is implementation dependent - the stack only provides the signaling for this.

### Enhanced Audio Voice Recognition {#sec:hfpeAVRActivation}

Similarly to AVR, Enhanced Audio voice recognition (eAVR) requires that HF and AG have the following features enabled: 

- HF: HFP_HFSF_ENHANCED_VOICE_RECOGNITION_STATUS and

- AG: HFP_AGSF_ENHANCED_VOICE_RECOGNITION_STATUS.

In addition, to allow textual representation of audio that is parsed by eAVR (note that parsing is not part of Bluetooth specification), both devices must enable:

- HF: HFP_HFSF_VOICE_RECOGNITION_TEXT and

- AG: HFP_AGSF_VOICE_RECOGNITION_TEXT. 

eAVR implements the same use cases as AVR (see previous section) and it can be activated or deactivated using the same API as for AVR, see above.

When eAVR and audio channel are established there are several additional commands that can be sent:

| HFP Role | eVRA API | Description |
-----------|----------|-------------|
|HF | hfp_hf_enhanced_voice_recognition_report_ready_for_audio| Ready to accept audio input. |
|AG | hfp_ag_enhanced_voice_recognition_report_ready_for_audio | Voice recognition engine is ready to accept audio input. |
|AG | hfp_ag_enhanced_voice_recognition_report_sending_audio | The voice recognition engine will play a sound, e.g. starting sound. |
|AG | hfp_ag_enhanced_voice_recognition_report_processing_input | Voice recognition engine is processing the audio input. |
|AG | hfp_ag_enhanced_voice_recognition_send_message | Report textual representation from the voice recognition engine. |


## HID - Human-Interface Device Profile

The HID profile allows an HID Host to connect to one or more HID Devices and communicate with them. 
Examples of Bluetooth HID devices are keyboards, mice, joysticks, gamepads, remote controls, and also voltmeters and temperature sensors.
Typical HID hosts would be a personal computer, tablets, gaming console, industrial machine, or data-recording device. 

Please refer to:

- [HID Host API](../appendix/apis/#sec:hidHostAPIAppendix) and [hid_host_demo](../examples/examples/#sec:hidhostdemoExample) for the HID Host role

- [HID Device API](../appendix/apis/#sec:hidDeviceAPIAppendix), [hid_keyboard_demo](../examples/examples/#sec:hidkeyboarddemoExample) and [hid_mouse_demo](../examples/examples/#sec:hidmousedemoExample)  for the HID Device role.


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
[SMP](../protocols/#sec:smpProtocols).

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
Counter example](../examples/examples/#sec:sppandlecounterExample).

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

## GATT Client {#sec:GATTClientProfiles}

The GATT profile uses ATT Attributes to represent a hierarchical
structure of GATT Services and GATT Characteristics. Each Service has
one or more Characteristics. Each Characteristic has meta data attached
like its type or its properties. This hierarchy of Characteristics and
Services are queried and modified via ATT operations.

GATT defines both a server and a client role. A device can implement one
or both GATT roles.

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
*GATT_EVENT_X*s are returned before a *GATT_EVENT_QUERY_COMPLETE* event
completes the query.

For more details on the available GATT queries, please consult
[GATT Client API](#sec:gattClientAPIAppendix).

### Authentication

By default, the GATT Server is responsible for security and the GATT Client does not enforce any kind of authentication.
If the GATT Client accesses Characteristic that require encrytion or authentication, the remote GATT Server will return an error,
which is returned in the *att status* of the *GATT_EVENT_QUERY_COMPLETE*.

You can define *ENABLE_GATT_CLIENT_PAIRING* to instruct the GATT Client to trigger pairing in this case and to repeat the request.

This model allows for an attacker to spoof another device, but don't require authentication for the Characteristics.
As a first improvement, you can define *ENABLE_LE_PROACTIVE_AUTHENTICATION* in *btstack_config.h*. When defined, the GATT Client will
request the Security Manager to re-encrypt the connection if there is stored bonding information available.
If this fails, the  *GATT_EVENT_QUERY_COMPLETE* will return with the att status *ATT_ERROR_BONDING_INFORMATION_MISSING*.

With *ENABLE_LE_PROACTIVE_AUTHENTICATION* defined and in Central role, you need to delete the local bonding information if the remote
lost its bonding information, e.g. because of a device reset. See *example/sm_pairing_central.c*.

Even with the Proactive Authentication, your device may still connect to an attacker that provides the same advertising data as 
your actual device. If the device that you want to connect requires pairing, you can instruct the GATT Client to automatically
request an encrypted connection before sending any GATT Client request by calling *gatt_client_set_required_security_level()*.
If the device provides sufficient IO capabilities, a MITM attack can then be prevented. We call this 'Mandatory Authentication'.

The following diagrams provide a detailed overview about the GATT Client security mechanisms in different configurations:

-  [Reactive Authentication as Central](picts/gatt_client_security_reactive_authentication_central.svg)
-  [Reactive Authentication as Peripheral](picts/gatt_client_security_reactive_authentication_peripheral.svg)
-  [Proactive Authentication as Central](picts/gatt_client_security_proactive_authentication_central.svg)
-  [Proactive Authentication as Peripheral](picts/gatt_client_security_proactive_authentication_peripheral.svg)
-  [Mandatory Authentication as Central](picts/gatt_client_security_mandatory_authentication_central.svg)
-  [Mandatory Authentication as Peripheral](picts/gatt_client_security_mandatory_authentication_peripheral.svg)

## GATT Server {#sec:GATTServerProfiles}

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

Adding NOTIFY and/or INDICATE automatically creates an additional Client
Configuration Characteristic.

Property                | Description
------------------------|-----------------------------------------------
READ                    | Characteristic can be read
WRITE                   | Characteristic can be written using Write Request
WRITE_WITHOUT_RESPONSE  | Characteristic can be written using Write Command
NOTIFY                  | Characteristic allows notifications by server
INDICATE                | Characteristic allows indication by server
DYNAMIC                 | Read or writes to Characteristic are handled by application

To require encryption or authentication before a Characteristic can be
accessed, you can add one or more of the following properties:

Property                | Description
------------------------|-----------------------------------------------
AUTHENTICATION_REQUIRED | Read and Write operations require Authentication
READ_ENCRYPTED          | Read operations require Encryption
READ_AUTHENTICATED      | Read operations require Authentication
WRITE_ENCRYPTED         | Write operations require Encryption
WRITE_AUTHENTICATED     | Write operations require Authentication
ENCRYPTION_KEY_SIZE_X   | Require encryption size >= X, with W in [7..16]

For example, Volume State Characteristic (Voice Control Service) requires:
- Mandatory Properties: Read, Notify
- Security Permissions: Encryption Required

In addition, its read is handled by application. We can model this Characteristic as follows:

~~~~
    CHARACTERISTIC, ORG_BLUETOOTH_CHARACTERISTIC_VOLUME_STATE, DYNAMIC | READ | NOTIFY | ENCRYPTION_KEY_SIZE_16
~~~~

To use already implemented GATT Services, you can import it
using the *#import <service_name.gatt>* command. See [list of provided services](gatt_services.md).

BTstack only provides an ATT Server, while the GATT Server logic is
mainly provided by the GATT compiler. While GATT identifies
Characteristics by UUIDs, ATT uses Handles (16 bit values). To allow to
identify a Characteristic without hard-coding the attribute ID, the GATT
compiler creates a list of defines in the generated \*.h file.

Similar to other protocols, it might be not possible to send any time.
To send a Notification, you can call *att_server_request_to_send_notification*
to request a callback, when yuo can send the Notification.

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
To implement the new services as a reusable module, it's necessary to get access to all read/write requests related to this service.

For this, the ATT DB allows to register read/write callbacks for a specific handle range with *att_server_register_service_handler()*.

Since the handle range depends on the application's .gatt file, the handle range for Primary and Secondary Services can be queried with *gatt_server_get_get_handle_range_for_service_with_uuid16*.

Similarly, you will need to know the attribute handle for particular Characteristics to handle Characteristic read/writes requests. You can get the attribute value handle for a Characteristics *gatt_server_get_value_handle_for_characteristic_with_uuid16()*.

In addition to the attribute value handle, the handle for the Client Characteristic Configuration is needed to support Indications/Notifications. You can get this attribute handle with *gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16()*

Finally, in order to send Notifications and Indications independently from the main application, *att_server_register_can_send_now_callback* can be used to request a callback when it's possible to send a Notification or Indication.

To see how this works together, please check out the Battery Service Server in *src/ble/battery_service_server.c*.

### GATT Database Hash

When a GATT Client connects to a GATT Server, it cannot know if the GATT Database has changed 
and has to discover the provided GATT Services and Characteristics after each connect.
 
To speed this up, the Bluetooth
specification defines a GATT Service Changed Characteristic, with the idea that a GATT Server would notify
a bonded GATT Client if its database changed. However, this is quite fragile and it is not clear how it can be implemented
in a robust way.

The Bluetooth Core Spec 5.1 introduced the GATT Database Hash Characteristic, which allows for a simple 
robust mechanism to cache a remote GATT Database. The GATT Database Hash is a 16-byte value that is calculated
over the list of Services and Characteristics. If there is any change to the database, the hash will change as well.

To support this on the GATT Server, you only need to add a GATT Service with the GATT Database Characteristic to your .gatt file.
The hash value is then calculated by the GATT compiler. 


    PRIMARY_SERVICE, GATT_SERVICE
    CHARACTERISTIC, GATT_DATABASE_HASH, READ,

Note: make sure to install the PyCryptodome python package as the hash is calculated using AES-CMAC,
e.g. with:

    pip install pycryptodomex
