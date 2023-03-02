#

## L2CAP Events {#sec:eventsAndErrorsAppendix}

L2CAP events and data packets are delivered to the packet handler
specified by *l2cap_register_service* resp.
*l2cap_create_channel*. Data packets have the
L2CAP_DATA_PACKET packet type. L2CAP provides the following events:

-   L2CAP_EVENT_CHANNEL_OPENED - sent if channel establishment is
    done. Status not equal zero indicates an error. Possible errors: out
    of memory; connection terminated by local host, when the connection
    to remote device fails.

-   L2CAP_EVENT_CHANNEL_CLOSED - emitted when channel is closed. No
    status information is provided.

-   L2CAP_EVENT_INCOMING_CONNECTION - received when the connection is
    requested by remote. Connection accept and decline are performed
    with *l2cap_accept_connection* and *l2cap_decline_connecti-on* respectively.

-   L2CAP_EVENT_CAN_SEND_NOW - Indicates that an L2CAP data packet could
    be sent on the reported l2cap_cid. It is emitted after a call to
    *l2cap_request_can_send_now*. See [Sending L2CAP Data](../protocols/#sec:l2capSendProtocols)
    Please note that the guarantee that a packet can be sent is only valid when the event is received.
    After returning from the packet handler, BTstack might need to send itself.

Event      | Event Code
-----------|----------------------------------------
L2CAP_EVENT_CHANNEL_OPENED          | 0x70 
L2CAP_EVENT_CHANNEL_CLOSED          | 0x71 
L2CAP_EVENT_INCOMING_CONNECTION     | 0x72 
L2CAP_EVENT_CAN_SEND_NOW            | 0x78

Table: L2CAP Events. {#tbl:l2capEvents}

L2CAP event paramaters, with size in bits:

- L2CAP_EVENT_CHANNEL_OPENED: 
    - *event(8), len(8), status(8), address(48), handle(16), psm(16), local_cid(16), remote_cid(16), local_mtu(16), remote_mtu(16)*  
- L2CAP_EVENT_CHANNEL_CLOSED: 
    - *event (8), len(8), channel(16)* 
- L2CAP_EVENT_INCOMING_CONNECTION: 
    - *event(8), len(8), address(48), handle(16), psm (16), local_cid(16), remote_cid (16)* 
- L2CAP_EVENT_CAN_SEND_NOW:
    - *event(8), len(8), local_cid(16)

## RFCOMM Events

All RFCOMM events and data packets are currently delivered to the packet
handler specified by *rfcomm_register_packet_handler*. Data packets
have the _DATA_PACKET packet type. Here is the list of events provided
by RFCOMM:

-   RFCOMM_EVENT_INCOMING_CONNECTION - received when the connection
    is requested by remote. Connection accept and decline are performed
    with *rfcomm_accept_connection* and
    *rfcomm_decline_connection* respectively.

-   RFCOMM_EVENT_CHANNEL_CLOSED - emitted when channel is closed. No
    status information is provided.

-   RFCOMM_EVENT_CHANNEL_OPENED - sent if channel
    establishment is done. Status not equal zero indicates an error.
    Possible errors: an L2CAP error, out of memory.

-   RFCOMM_EVENT_CAN_SEND_NOW - Indicates that an RFCOMM data packet could
    be sent on the reported rfcomm_cid. It is emitted after a call to
    *rfcomm_request_can_send_now*. See [Sending RFCOMM Data](../protocols/#sec:rfcommSendProtocols)
    Please note that the guarantee that a packet can be sent is only valid when the event is received.
    After returning from the packet handler, BTstack might need to send itself.


Event      | Event Code
-----------|-----------------------------
RFCOMM_EVENT_CHANNEL_OPENED | 0x80 
RFCOMM_EVENT_CHANNEL_CLOSED        | 0x81 
RFCOMM_EVENT_INCOMING_CONNECTION   | 0x82 
RFCOMM_EVENT_CAN_SEND_NOW          | 0x89 

Table: RFCOMM Events. {#tbl:rfcommEvents}


RFCOMM event paramaters, with size in bits:

- RFCOMM_EVENT_CHANNEL_OPENED: 
    - *event(8), len(8), status(8), address(48), handle(16), server_channel(8), rfcomm_cid(16), max_frame_size(16)*
- RFCOMM_EVENT_CHANNEL_CLOSED: 
    - *event(8), len(8), rfcomm_cid(16)*
- RFCOMM_EVENT_INCOMING_CONNECTION: 
    - *event(8), len(8), address(48), channel (8), rfcomm_cid(16)*
- RFCOMM_EVENT_CAN_SEND_NOW:
    - *event(8), len(8), rfcomm_cid(16)

## Errors {#sec:errorsAppendix}


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
RFCOMM_NO_OUTGOING_CREDITS | 0x72
SDP_HANDLE_ALREADY_REGISTERED | 0x80

Table: Errors. {#tbl:errors}

