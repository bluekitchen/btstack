#include <stdint.h>
#include <unistd.h>

#include "btstack_defines.h"
#include "btstack_debug.h"
#include "bluetooth.h"

extern "C" int l2cap_can_send_packet_now(uint16_t cid){
    return 1;
}
extern "C" uint8_t l2cap_create_channel(btstack_packet_handler_t packet_handler, bd_addr_t address, uint16_t psm, uint16_t mtu, uint16_t * out_local_cid){
    return 0x41;
}
extern "C" void l2cap_disconnect(uint16_t local_cid, uint8_t reason){
}
extern "C" uint8_t *l2cap_get_outgoing_buffer(void){
    return NULL;
}
extern "C" uint16_t l2cap_max_mtu(void){
    return 0;
}
extern "C" int l2cap_reserve_packet_buffer(void){
    return 0;
}
extern "C" int l2cap_send_prepared(uint16_t local_cid, uint16_t len){
    return 0;
}