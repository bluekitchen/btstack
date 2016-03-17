@@
typedef uint8_t, uint16_t, uint32_t;
@@
- dummy_rule_to_add_typedefs( ... );

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

// HCI Packet Handler

@@
identifier packet_handler;
@@
- hci_register_packet_handler(packet_handler);
+ hci_register_packet_handler(&packet_handler);

@hci_register_packet_handler@
identifier packet_handler;
@@
- hci_register_packet_handler(&packet_handler);
+ static btstack_packet_callback_registration_t callback_registration;
+ callback_registration.callback = &packet_handler;
+ hci_add_event_handler(&callback_registration);

@@
identifier hci_register_packet_handler.packet_handler, packet_type, packet, size;
@@
packet_handler(uint8_t packet_type, 
+ uint16_t channel,
uint8_t * packet, uint16_t size)
{ ... }

// L2CAP Packet Handler

@@
identifier packet_handler;
@@
- l2cap_register_packet_handler(packet_handler);
+ l2cap_register_packet_handler(&packet_handler);

@l2cap_register_packet_handler@
identifier packet_handler;
@@
- l2cap_register_packet_handler(&packet_handler);
+ static btstack_packet_callback_registration_t callback_registration;
+ callback_registration.callback = &packet_handler;
+ hci_add_event_handler(&callback_registration);

@@
identifier l2cap_register_packet_handler.packet_handler, connection, packet_type, channel, packet, size;
@@
- void packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size)
+ void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size)
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

// SDP Client

// propagate sdp client callback into individual sdp client calls
// add warning that type doesn't match

// remove sdp_parser_init()
@@
@@
- sdp_parser_init();

// track calls to sdp_parser_register_callback

@@
identifier sdp_client_callback;
@@
- sdp_parser_register_callback(sdp_client_callback);
+ sdp_parser_register_callback(&sdp_client_callback);

@sdp_parser_register_callback@
identifier sdp_client_callback;
@@
- sdp_parser_register_callback(&sdp_client_callback);

// fix calls for sdp_query_util.h 
@@
identifier sdp_parser_register_callback.sdp_client_callback;
expression E1, E2;
@@
- sdp_general_query_for_uuid(E1, E2)
+ sdp_client_query_uuid16(sdp_client_callback, E1, E2)

@@
identifier sdp_parser_register_callback.sdp_client_callback;
expression E1, E2;
@@
- sdp_general_query_for_uuid128(E1, E2)
+ sdp_client_query_uuid128(sdp_client_callback, E1, E2)

// fix calls for sdp_client.h
@@
identifier sdp_parser_register_callback.sdp_client_callback;
expression E1, E2, E3;
@@
+ sdp_client_query(E1, E2, E3)
- sdp_client_query(sdp_client_callback, E1, E2, E3)

// track calls to sdp_client_query_rfcomm_register_callback
@@
identifier sdp_client_callback;
expression E1;
@@
- sdp_client_query_rfcomm_register_callback(sdp_client_callback, E1);
+ sdp_client_query_rfcomm_register_callback(&sdp_client_callback, E1);

@sdp_client_query_rfcomm_register_callback @
identifier sdp_client_callback;
expression E1;
@@
- sdp_client_query_rfcomm_register_callback(&sdp_client_callback, E1);

@@
typedef sdp_query_event_t;
identifier fn, event;
type T;
@@
- T fn(sdp_query_event_t * event)
+
+ // MIGRATION: SDP Client callback changed to btstack_packet_handler_t
+ // Please  use sdp_client_X functions from btstack_event.h to access event fields
+
+ T fn(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size)
{ ... }

// fix calls to sdp_query_rfcomm 
@@
identifier sdp_client_query_rfcomm_register_callback.sdp_client_callback;
expression E1, E2;
@@
- sdp_client_query_rfcomm_channel_and_name_for_uuid(E1, E2)
+ sdp_client_query_rfcomm_channel_and_name_for_uuid(sdp_client_callback, E1, E2)

@@
identifier sdp_client_query_rfcomm_register_callback.sdp_client_callback;
expression E1, E2;
@@
- sdp_client_query_rfcomm_channel_and_name_for_search_pattern(E1, E2)
+ sdp_client_query_rfcomm_channel_and_name_for_search_pattern(sdp_client_callback, E1, E2)

@@
identifier fn, event, context;
type T;
@@
- T fn(sdp_query_event_t * event, void * context)
+
+ // MIGRATION: SDP Client callback changed to btstack_packet_handler_t
+ // Please  use sdp_client_X functions from btstack_event.h to access event fields
+
+ T fn(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size)
{ ... }

// SDP Util

@@
expression E1, E2, E3;
@@
- spp_create_sdp_record(E1, E2, E3)
+ // MIGRATION: using 0x10001 as Service Record Handle. Please fix if using multiple services
+ spp_create_sdp_record(E1, 0x10001, E2, E3)

// SDP Server
@@
expression E1, E2;
@@
- sdp_register_service(E1, E2)
+ sdp_register_service(E2)

// L2CAP - just drop connection param
@@
expression E1, E2, E3, E4, E5;
@@
- l2cap_create_channel(E1, E2, E3, E4, E5)
+ l2cap_create_channel(E2, E3, E4, E5)

@@
expression E1, E2, E3, E4, E5;
@@
- l2cap_create_service(E1, E2, E3, E4, E5)
+ l2cap_create_service(E2, E3, E4, E5)

// RFCOMM

// track calls to rfcomm_register_packet_handler
@@
identifier rfcomm_callback;
@@
- rfcomm_register_packet_handler(rfcomm_callback);
+ rfcomm_register_packet_handler(&rfcomm_callback);

@rfcomm_register_packet_handler@
identifier rfcomm_callback;
@@
- rfcomm_register_packet_handler(&rfcomm_callback);

// fix calls to 
// rfcomm_register_service
@@
identifier rfcomm_register_packet_handler.rfcomm_callback;
expression E1, E2, E3;
@@
- rfcomm_register_service(E1, E2, E3)
+ rfcomm_register_service(rfcomm_callback, E2, E3)

// rfcomm_register_service_with_initial_credits,
@@
identifier rfcomm_register_packet_handler.rfcomm_callback;
expression E1, E2, E3, E4;
@@
- rfcomm_register_service_with_initial_credits(E1, E2, E3, E4)
+ rfcomm_register_service_with_initial_credits(rfcomm_callback, E2, E3, E4)

// rfcomm_create_channel
@@
identifier rfcomm_register_packet_handler.rfcomm_callback;
expression E1, E2, E3;
@@
- rfcomm_create_channel(E1, E2, E3)
+ rfcomm_create_channel(rfcomm_callback, E2, E3)

// rfcomm_create_channel_with_initial_credits
@@
identifier rfcomm_register_packet_handler.rfcomm_callback;
expression E1, E2, E3, E4;
@@
- rfcomm_create_channel_with_initial_creditis(E1, E2, E3, E4)
+ rfcomm_create_channel_with_initial_creditis(rfcomm_callback, E2, E3, E4)

// HSP
@@
expression E1, E2, E3, E4;
@@
- hsp_hs_create_sdp_record(E1, E2, E3, E4)
+ // MIGRATION: using 0x10002 as Service Record Handle. Please fix if using multiple services
+ hsp_hs_create_sdp_record(E1, 0x10002, E2, E3, E4)

@@
expression E1, E2, E3;
@@
- hsp_ag_create_sdp_record(E1, E2, E3)
+ // MIGRATION: using 0x10002 as Service Record Handle. Please fix if using multiple services
+ hsp_ag_create_sdp_record(E1, 0x10002, E2, E3)

// GATT Client

// track callback registration
@@
identifier gatt_callback;
@@
- gatt_client_register_packet_handler(gatt_callback);
+ gatt_client_register_packet_handler(&gatt_callback);

@gatt_client_register_packet_handler@
identifier gatt_callback;
identifier gc_id;
@@
- gc_id = gatt_client_register_packet_handler(&gatt_callback);

// update callback
@@
identifier gatt_client_register_packet_handler.gatt_callback;
identifier event;
typedef le_event_t;
@@
- gatt_callback(le_event_t * event)
+
+ // MIGRATION: GATT Client callback changed to btstack_packet_handler_t
+ // Please  use gatt_event_X functions from btstack_event.h to access event fields
+
+ gatt_callback(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size)
{ ... }

// update all calls
@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_discover_primary_services(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_discover_primary_services_by_uuid16(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_discover_primary_services_by_uuid128(
- gc_id,
+ gatt_callback,
... )

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_find_included_services_for_service(
- gc_id,
+ gatt_callback,
... )

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_discover_characteristics_for_service(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_discover_characteristics_for_handle_range_by_uuid16(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_discover_characteristics_for_handle_range_by_uuid128(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_discover_characteristics_for_service_by_uuid16 (
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_discover_characteristics_for_service_by_uuid128(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_discover_characteristic_descriptors(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_read_value_of_characteristic(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_read_value_of_characteristic_using_value_handle(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_read_value_of_characteristics_by_uuid16(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_read_value_of_characteristics_by_uuid128(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_read_long_value_of_characteristic(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_read_long_value_of_characteristic_using_value_handle(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_read_long_value_of_characteristic_using_value_handle_with_offset(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_read_multiple_characteristic_values(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_write_value_of_characteristic_without_response(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_signed_write_without_response(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_write_value_of_characteristic(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_write_long_value_of_characteristic(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_write_long_value_of_characteristic_with_offset(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_reliable_write_long_value_of_characteristic(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_read_characteristic_descriptor(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_read_characteristic_descriptor_using_descriptor_handle(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_read_long_characteristic_descriptor(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_read_long_characteristic_descriptor_using_descriptor_handle(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_read_long_characteristic_descriptor_using_descriptor_handle_with_offset(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_write_characteristic_descriptor(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_write_characteristic_descriptor_using_descriptor_handle(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_write_long_characteristic_descriptor(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_write_long_characteristic_descriptor_using_descriptor_handle(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_write_long_characteristic_descriptor_using_descriptor_handle_with_offset(
- gc_id,
+ gatt_callback,
... );

@@
expression gc_id;
identifier gatt_client_register_packet_handler.gatt_callback;
@@
gatt_client_write_client_characteristic_configuration(
- gc_id,
+ gatt_callback,
... );

