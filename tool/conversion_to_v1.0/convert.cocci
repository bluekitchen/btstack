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
