#include <stdint.h>
#include <unistd.h>

#include "btstack_defines.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "bluetooth.h"

static btstack_packet_handler_t packet_handler;
static uint8_t outgoing_buffer[1024];
static uint16_t disconnect_count;
static uint16_t send_prepared_count;

int l2cap_can_send_packet_now(uint16_t cid){
    UNUSED(cid);
    return 1;
}
uint8_t l2cap_request_can_send_now_event(uint16_t cid){
    uint8_t event[] = { L2CAP_EVENT_CAN_SEND_NOW, 2, 0, 0};
    little_endian_store_16(event, 2, cid);
    packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
    return ERROR_CODE_SUCCESS;
}

uint8_t l2cap_create_channel(btstack_packet_handler_t handler, bd_addr_t address, uint16_t psm, uint16_t mtu, uint16_t * out_local_cid){
    UNUSED(address);
    UNUSED(psm);
    UNUSED(mtu);
    UNUSED(out_local_cid);
	packet_handler = handler;
    return 0x41;
}
uint8_t l2cap_disconnect(uint16_t local_cid){
    UNUSED(local_cid);
    disconnect_count++;
    return ERROR_CODE_SUCCESS;
}
uint8_t *l2cap_get_outgoing_buffer(void){
    return outgoing_buffer;
}
uint16_t l2cap_max_mtu(void){
    return 1024;
}
int l2cap_reserve_packet_buffer(void){
    return 0;
}
int l2cap_send_prepared(uint16_t local_cid, uint16_t len){
    UNUSED(local_cid);
    UNUSED(len);
    send_prepared_count++;
    return 0;
}

void mock_l2cap_reset(void){
    disconnect_count = 0;
    send_prepared_count = 0;
}

uint16_t mock_l2cap_get_disconnect_count(void){
    return disconnect_count;
}

uint16_t mock_l2cap_get_send_prepared_count(void){
    return send_prepared_count;
}

uint16_t mock_l2cap_get_last_transaction_id(void){
    return big_endian_read_16(outgoing_buffer, 1);
}

void mock_l2cap_emit_channel_opened(uint16_t local_cid){
    uint8_t event[26] = { L2CAP_EVENT_CHANNEL_OPENED, sizeof(event) - 2 };
    little_endian_store_16(event, 13, local_cid);
    little_endian_store_16(event, 17, sizeof(outgoing_buffer));
    packet_handler(HCI_EVENT_PACKET, local_cid, event, sizeof(event));
}

void mock_l2cap_emit_data(uint16_t local_cid, uint8_t * packet, uint16_t size){
    packet_handler(L2CAP_DATA_PACKET, local_cid, packet, size);
}
