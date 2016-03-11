@@
expression dest, src;
@@
- bt_flip_addr(dest,src)
+ reverse_bd_addr(src, dest)

@@
expression handle;
@@
- hci_remote_eSCO_supported(handle)
+ hci_remote_esco_supported(handle)

@@
expression packet_handler;
@@
- hci_register_packet_handler(packet_handler);
+ static btstack_packet_callback_registration_t callback_registration;
+ callback_registration.callback = packet_handler;
+ hci_add_event_handler(&callback_registration);

@@
typedef uint8_t, uint16_t;
identifier fn, packet_type, packet, size;
@@
- void fn(uint8_t packet_type, uint8_t * packet, uint16_t size)
+ void fn(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size)
{ ... }

@@
// typedef uint8_t, uint16_t;
identifier fn, connection, packet_type, channel, packet, size;
@@
- void fn(void * connection, uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size)
+ void fn(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size)
{ ... }

@@
expression handle;
@@
- hci_can_send_sco_packet_now(handle)
+ hci_can_send_sco_packet_now()  

@@
expression uuid;
@@
- printUUID128(uuid)
+ printf("%s", uuid128_to_str(uuid))

@@
expression addr;
@@
- print_bd_addr(addr)
+ printf("%s", bd_addr_to_str(addr))

@@
expression str, addr;
typedef bd_addr_t;
@@
- sscan_bd_addr((uint8_t*)str, addr)
+ sscanf_bd_addr(str, addr)

@@
typedef timer;
typedef btstack_timer_source_t;
identifier fn, ts;
@@
- fn(struct timer * ts)
+ fn(btstack_timer_source_t * ts)
{ ... }

// propagate sdp client callback into individual sdp client calls
// add warning that type doesn't match

@@
@@
- sdp_parser_init();

@sdp_parser_register_callback @
identifier sdp_client_callback;
@@
- sdp_parser_register_callback(sdp_client_callback);

@@
identifier sdp_parser_register_callback.sdp_client_callback;
expression E1, E2;
@@
- sdp_general_query_for_uuid(E1, E2)
+ sdp_client_query_uuid16(sdp_client_callback, E1, E2)

@@
identifier fn, event;
identifier ve;
identifier ce;
typedef sdp_query_event_t;
typedef sdp_query_attribute_value_event_t;
typedef sdp_query_complete_event_t;
type T;
@@
- T fn(sdp_query_event_t * event)
+
+ // MIGRATION: SDP Client callback changed to btstack_packet_handler_t
+ // Please  use sdp_client_X functions to access event fields
+ T fn(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size)
{ ... }




