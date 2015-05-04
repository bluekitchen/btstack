
## L2CAP Events
<a name="appendix:events_and_errors"></a>

L2CAP events and data packets are delivered to the packet handler
specified by *l2cap_register_service* resp.
*l2cap_create_channel_internal*. Data packets have the
L2CAP_DATA_PACKET packet type. L2CAP provides the following events:

-   L2CAP_EVENT_CHANNEL_OPENED - sent if channel establishment is
    done. Status not equal zero indicates an error. Possible errors: out
    of memory; connection terminated by local host, when the connection
    to remote device fails.

-   L2CAP_EVENT_CHANNEL_CLOSED - emitted when channel is closed. No
    status information is provided.

-   L2CAP_EVENT_INCOMING_CONNECTION - received when the connection is
    requested by remote. Connection accept and decline are performed
    with *l2cap_accept_connection_internal* and
    *l2cap_decline_connecti-on_internal* respectively.

-   L2CAP_EVENT_CREDITS - emitted when there is a chance to send a new
    L2CAP packet. BTstack does not buffer packets. Instead, it requires
    the application to retry sending if BTstack cannot deliver a packet
    to the Bluetooth module. In this case, the l2cap_send_internal
    will return an error.

-   L2CAP_EVENT_SERVICE_REGISTERED - Status not equal zero indicates
    an error. Possible errors: service is already registered;
    MAX_NO_L2CAP_SERVICES (defined in config.h) already registered.



Event Code |Event / Event Parameters (size in bits) 
-----------|----------------------------------------
0x70 | L2CAP_EVENT_CHANNEL_OPENED<br/> *event(8), len(8), status(8), address(48), handle(16), psm(16), local_cid(16), remote_cid(16), local_mtu(16), remote_mtu(16)*  
0x71 | L2CAP_EVENT_CHANNEL_CLOSED<br/> *event (8), len(8), channel(16)* 
0x72 | L2CAP_EVENT_INCOMING_CONNECTION<br/> *event(8), len(8), address(48), handle(16), psm (16), local_cid(16), remote_cid (16)* 
0x74 | L2CAP_EVENT_CREDITS<br/> *event(8), len(8), local_cid(16), credits(8)*
0x75 | L2CAP_EVENT_SERVICE_REGISTERED</br> *event(8), len(8), status(8), psm(16)* 


## RFCOMM Events

All RFCOMM events and data packets are currently delivered to the packet
handler specified by *rfcomm_register_packet_handler*. Data packets
have the _DATA_PACKET packet type. Here is the list of events provided
by RFCOMM:

-   RFCOMM_EVENT_INCOMING_CONNECTION - received when the connection
    is requested by remote. Connection accept and decline are performed
    with *rfcomm_accept_connection_internal* and
    *rfcomm_decline_con-nection_internal* respectively.

-   RFCOMM_EVENT_CHANNEL_CLOSED - emitted when channel is closed. No
    status information is provided.

-   RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE - sent if channel
    establishment is done. Status not equal zero indicates an error.
    Possible errors: an L2CAP error, out of memory.

-   RFCOMM_EVENT_CREDITS - The application can resume sending when
    this even is received. See Section [section:flowcontrol] for more on
    RFCOMM credit-based flow-control.

-   RFCOMM_EVENT_SERVICE_REGISTERED - Status not equal zero indicates
    an error. Possible errors: service is already registered;
    MAX_NO_RFCOMM_SERVICES (defined in config.h) already registered.


Event Code |Event / Event Parameters (size in bits) 
-----------|----------------------------------------
0x80 | RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE<br/> *event(8), len(8), status(8), address(48), handle(16), server_channel(8), rfcomm_cid(16), max_frame_size(16)*
0x81 | RFCOMM_EVENT_CHANNEL_CLOSED<br/> *event(8), len(8), rfcomm_cid(16)*
0x82 | RFCOMM_EVENT_INCOMING_CONNECTION<br/> *event(8), len(8), address(48), channel (8), rfcomm_cid(16)*
0x84 | RFCOMM_EVENT_CREDITS<br/> *event(8), len(8), rfcomm_cid(16), credits(8)*
0x85 | RFCOMM_EVENT_SERVICE_REGISTERED<br/> *event(8), len(8), status(8), rfcomm server channel_id(8)*


## Errors

Error                                                                   |    Error Code
------------------------------------------------------------------------|-------------------
BTSTACK_MEMORY_ALLOC_FAILED | 0x56
BTSTACK_ACL_BUFFERS_FULL | 0x57
L2CAP_COMMAND_REJECT_REASON_COMMAND_NOT_UNDERSTOOD | 0x60
L2CAP_COMMAND_REJECT_REASON_SIGNALING_MTU_EXCEEDED | 0x61
L2CAP_COMMAND_REJECT_REASON_INVALID_CID_IN_REQUEST | 0x62
L2CAP_CONNECTION_RESPONSE_RESULT_SUCCESSFUL | 0x63
L2CAP_CONNECTION_RESPONSE_RESULT_PENDING | 0x64
L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_PSM | 0x65
L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_SECURITY | 0x66
L2CAP_CONNECTION_RESPONSE_RESULT_REFUSED_RESOURCES | 0x65
L2CAP_CONFIG_RESPONSE_RESULT_SUCCESSFUL | 0x66
L2CAP_CONFIG_RESPONSE_RESULT_UNACCEPTABLE_PARAMS | 0x67
L2CAP_CONFIG_RESPONSE_RESULT_REJECTED | 0x68
L2CAP_CONFIG_RESPONSE_RESULT_UNKNOWN_OPTIONS | 0x69
L2CAP_SERVICE_ALREADY_REGISTERED | 0x6a
RFCOMM_MULTIPLEXER_STOPPED | 0x70
RFCOMM_CHANNEL_ALREADY_REGISTERED | 0x71
RFCOMM_NO_OUTGOING_CREDITS | 0x72
SDP_HANDLE_ALREADY_REGISTERED | 0x80


