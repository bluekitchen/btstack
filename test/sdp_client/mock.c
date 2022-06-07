#include <stdint.h>
#include <unistd.h>

#include "btstack_defines.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "bluetooth.h"

static btstack_packet_handler_t packet_handler;

int l2cap_can_send_packet_now(uint16_t cid){
    return 1;
}
uint8_t l2cap_request_can_send_now_event(uint16_t cid){
    uint8_t event[] = { L2CAP_EVENT_CAN_SEND_NOW, 2, 0, 0};
    little_endian_store_16(event, 2, cid);
    packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
    return ERROR_CODE_SUCCESS;
}

uint8_t l2cap_create_channel(btstack_packet_handler_t handler, bd_addr_t address, uint16_t psm, uint16_t mtu, uint16_t * out_local_cid){
	packet_handler = handler;
    return 0x41;
}
uint8_t l2cap_disconnect(uint16_t local_cid){
    return ERROR_CODE_SUCCESS;
}
uint8_t *l2cap_get_outgoing_buffer(void){
    return NULL;
}
uint16_t l2cap_max_mtu(void){
    return 0;
}
int l2cap_reserve_packet_buffer(void){
    return 0;
}
int l2cap_send_prepared(uint16_t local_cid, uint16_t len){
    return 0;
}
