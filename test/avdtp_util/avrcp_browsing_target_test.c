#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "btstack_event.h"
#include "btstack_util.h"
#include "classic/avrcp.h"
#include "classic/avrcp_browsing.h"
#include "classic/avrcp_browsing_target.h"

avrcp_context_t avrcp_target_context;
avrcp_context_t avrcp_controller_context;
static avrcp_browsing_connection_t connection;
static btstack_packet_handler_t packet_handler;
static unsigned application_events;
static unsigned send_requests;
static uint8_t sent_packet[32];

avrcp_browsing_connection_t * avrcp_get_browsing_connection_for_l2cap_cid_for_role(avrcp_role_t role, uint16_t cid){
    (void)role;
    assert(cid == connection.l2cap_browsing_cid);
    return &connection;
}
avrcp_connection_t * avrcp_get_connection_for_browsing_cid_for_role(avrcp_role_t role, uint16_t cid){
    (void)role;
    (void)cid;
    return NULL;
}
void avrcp_browsing_register_target_packet_handler(btstack_packet_handler_t callback){
    packet_handler = callback;
}
void avrcp_browsing_request_can_send_now(avrcp_browsing_connection_t * conn, uint16_t cid){
    assert(conn == &connection);
    assert(cid == connection.l2cap_browsing_cid);
    send_requests++;
}
uint8_t l2cap_send(uint16_t cid, const uint8_t * data, uint16_t len){
    assert(cid == connection.l2cap_browsing_cid);
    assert(len <= sizeof(sent_packet));
    memcpy(sent_packet, data, len);
    return ERROR_CODE_SUCCESS;
}
static void application_callback(uint8_t type, uint16_t channel, uint8_t * packet, uint16_t size){
    (void)channel;
    (void)size;
    assert(type == HCI_EVENT_PACKET);
    assert(packet[2] == AVRCP_SUBEVENT_BROWSING_GET_FOLDER_ITEMS);
    application_events++;
}
static void check_range(uint32_t start, uint32_t end){
    memset(&connection, 0, sizeof(connection));
    connection.state = AVCTP_CONNECTION_OPENED;
    connection.l2cap_browsing_cid = 0x40;
    connection.start_item = 10;
    connection.end_item = 20;
    application_events = 0;
    send_requests = 0;
    uint8_t command[] = {
        0x30, 0x11, 0x0e, AVRCP_PDU_ID_GET_FOLDER_ITEMS, 0, 10,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    big_endian_store_32(command, 7, start);
    big_endian_store_32(command, 11, end);
    packet_handler(L2CAP_DATA_PACKET, 0x40, command, sizeof(command));
    // Range validation is performed by the application, which receives both
    // values even when end_item precedes start_item.
    assert(application_events == 1);
    assert(send_requests == 0);
    assert(connection.start_item == start);
    assert(connection.end_item == end);
    assert(connection.state == AVCTP_CONNECTION_OPENED);
}
int main(void){
    avrcp_browsing_target_init();
    avrcp_browsing_target_register_packet_handler(application_callback);
    check_range(5, 3);
    check_range(5, 4);
    check_range(UINT32_MAX, 0);
    check_range(0, 0);
    check_range(5, 5);
    check_range(3, 5);
    check_range(0, UINT32_MAX);
    puts("PASS: AVRCP browsing folder item range forwarding");
    return 0;
}
