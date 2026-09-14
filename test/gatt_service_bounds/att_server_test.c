#define ENABLE_GATT_OVER_EATT
#define ENABLE_L2CAP_ENHANCED_CREDIT_BASED_FLOW_CONTROL_MODE
#include <assert.h>
#include <stdlib.h>
#include "ble/att_server.c"

static uint16_t sent_size;
uint8_t l2cap_ecbm_accept_channels(uint16_t cid, uint8_t count, uint16_t credits, uint16_t size, uint8_t ** buffers, uint16_t * cids){
    UNUSED(cid); UNUSED(count); UNUSED(credits); UNUSED(size); UNUSED(buffers); UNUSED(cids);
    assert(false);
    return ERROR_CODE_SUCCESS;
}
uint8_t l2cap_ecbm_decline_channels(uint16_t cid, uint16_t result){
    UNUSED(cid); UNUSED(result);
    assert(false);
    return ERROR_CODE_SUCCESS;
}
uint8_t l2cap_request_can_send_now_event(uint16_t cid){
    UNUSED(cid);
    return ERROR_CODE_SUCCESS;
}
uint8_t l2cap_send(uint16_t cid, const uint8_t * data, uint16_t size){
    UNUSED(cid);
    assert(size <= att_server_eatt_send_buffer_size);
    assert(data[0] == ATT_READ_RESPONSE);
    sent_size = size;
    return ERROR_CODE_SUCCESS;
}
uint8_t l2cap_ecbm_register_service(btstack_packet_handler_t handler, uint16_t psm, uint16_t mtu,
                                    gap_security_level_t security, bool authorization){
    UNUSED(handler); UNUSED(psm); UNUSED(mtu); UNUSED(security); UNUSED(authorization);
    return ERROR_CODE_SUCCESS;
}

int main(void){
    att_server_eatt_bearer_t bearer = {0};
    bearer.att_server.l2cap_cid = 0x40;
    bearer.att_server.bearer_type = ATT_BEARER_ENHANCED_LE;
    att_server_eatt_bearer_active = (btstack_linked_item_t *) &bearer;
    att_server_eatt_send_buffer_size = 64;
    bearer.send_buffer = malloc(att_server_eatt_send_buffer_size);
    assert(bearer.send_buffer != NULL);
    uint8_t event[23] = {L2CAP_EVENT_ECBM_CHANNEL_OPENED, 21, 0};
    little_endian_store_16(event, 15, 0x40);
    little_endian_store_16(event, 21, 512);
    att_server_eatt_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));

    // A readable 128-byte attribute is larger than the local 64-byte send buffer.
    uint8_t database[1 + 8 + 128 + 2] = {1};
    little_endian_store_16(database, 1, 8 + 128);
    little_endian_store_16(database, 3, ATT_PROPERTY_READ);
    little_endian_store_16(database, 5, 1);
    little_endian_store_16(database, 7, 0x2a00);
    memset(database + 9, 0x5a, 128);
    att_set_db(database);
    bearer.att_server.request_buffer[0] = ATT_READ_REQUEST;
    little_endian_store_16(bearer.att_server.request_buffer, 1, 1);
    bearer.att_server.request_size = 3;
    assert(att_server_process_validated_request(&bearer.att_server, &bearer.att_connection, bearer.send_buffer) == 1);
    assert(sent_size == 64);
    assert(bearer.att_connection.mtu == 64);
    assert(bearer.att_connection.max_mtu == 64);
    free(bearer.send_buffer);
    att_server_eatt_bearer_active = NULL;

    // Reject storage that cannot provide the minimum EATT receive/send MTUs.
    uint16_t storage_size = sizeof(att_server_eatt_bearer_t) + 2 * 64;
    uint8_t * storage = malloc(storage_size);
    assert(storage != NULL);
    assert(att_server_eatt_init(1, storage, storage_size - 1) == ERROR_CODE_MEMORY_CAPACITY_EXCEEDED);
    assert(att_server_eatt_init(1, storage, storage_size) == ERROR_CODE_SUCCESS);
    att_server_eatt_bearer_pool = NULL;
    free(storage);
    return 0;
}
