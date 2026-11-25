#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../src/classic/bnep.c"

static uint8_t output[2000];
static uint16_t response;
static unsigned incoming_events, opened_events, requests;
static uint8_t decision;

void hci_dump_log(int level, const char * format, ...){ (void) level; (void) format; }
void hci_dump_btstack_event(const uint8_t * packet, uint16_t len){ (void) packet; (void) len; }
uint8_t l2cap_request_can_send_now_event(uint16_t cid){ (void) cid; requests++; return 0; }
void l2cap_reserve_packet_buffer(void){}
uint8_t * l2cap_get_outgoing_buffer(void){ return output; }
uint8_t l2cap_send_prepared(uint16_t cid, uint16_t len){
    (void) cid;
    if (len == 4 && output[1] == BNEP_CONTROL_TYPE_SETUP_CONNECTION_RESPONSE){
        response = big_endian_read_16(output, 2);
    }
    return 0;
}
int btstack_run_loop_remove_timer(btstack_timer_source_t * timer){ (void) timer; return 1; }

static void handler(uint8_t type, uint16_t channel, uint8_t * packet, uint16_t size){
    (void) channel; (void) size;
    assert(type == HCI_EVENT_PACKET);
    if (packet[0] == BNEP_EVENT_INCOMING_CONNECTION){
        incoming_events++;
        assert(bnep_event_incoming_connection_get_bnep_cid(packet) == 0x40);
        assert(bnep_event_incoming_connection_get_source_uuid(packet) == BLUETOOTH_SERVICE_CLASS_PANU);
        assert(bnep_event_incoming_connection_get_destination_uuid(packet) == BLUETOOTH_SERVICE_CLASS_NAP);
        assert(bnep_event_incoming_connection_get_con_handle(packet) == 0x123);
        if (decision == 1) assert(bnep_accept_connection(0x40) == ERROR_CODE_SUCCESS);
        if (decision == 2) assert(bnep_decline_connection(0x40) == ERROR_CODE_SUCCESS);
    } else if (packet[0] == BNEP_EVENT_CHANNEL_OPENED){
        opened_events++;
        assert(bnep_event_channel_opened_get_status(packet) == 0);
    }
}

static bnep_channel_t channel;
static bnep_service_t service;
static void reset(void){
    memset(&channel, 0, sizeof(channel));
    memset(&service, 0, sizeof(service));
    channel.state = BNEP_CHANNEL_STATE_WAIT_FOR_CONNECTION_REQUEST;
    channel.l2cap_cid = 0x40;
    channel.con_handle = 0x123;
    service.service_uuid = BLUETOOTH_SERVICE_CLASS_NAP;
    service.packet_handler = handler;
    bnep_channels = (btstack_linked_item_t *) &channel;
    bnep_services = (btstack_linked_item_t *) &service;
    incoming_events = opened_events = requests = 0;
    response = 0xffff;
    decision = 0;
}
static void setup_request(void){
    uint8_t request[] = { 1, 2, 0x11, 0x16, 0x11, 0x15 };
    assert(bnep_handle_connection_request(&channel, request, sizeof(request)) == sizeof(request));
}
static void ready(void){
    bnep_channel_event_t event = { BNEP_CH_EVT_READY_TO_SEND };
    bnep_channel_state_machine(&channel, &event);
}
int main(void){
    reset();
    assert(bnep_accept_connection(0x41) == ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER);
    assert(bnep_accept_connection(0x40) == ERROR_CODE_COMMAND_DISALLOWED);
    setup_request();
    assert(incoming_events == 1 && requests == 0 && opened_events == 0);
    assert(channel.state == BNEP_CHANNEL_STATE_WAIT_FOR_CLIENT);
    assert(bnep_accept_connection(0x40) == ERROR_CODE_SUCCESS);
    assert(bnep_decline_connection(0x40) == ERROR_CODE_COMMAND_DISALLOWED);
    ready();
    assert(response == BNEP_SETUP_CONNECTION_RESPONSE_SUCCESS && opened_events == 1);
    assert(channel.state == BNEP_CHANNEL_STATE_CONNECTED);
    setup_request();
    ready();
    assert(incoming_events == 1 && opened_events == 1);

    reset(); decision = 2;
    setup_request();
    assert(incoming_events == 1 && requests == 1);
    ready();
    assert(response == BNEP_SETUP_CONNECTION_RESPONSE_CONNECTION_NOT_ALLOWED);
    assert(opened_events == 0 && channel.state == BNEP_CHANNEL_STATE_WAIT_FOR_CONNECTION_REQUEST);

    reset(); decision = 1;
    setup_request();
    ready();
    assert(response == 0 && opened_events == 1);

    reset();
    uint8_t invalid[] = { 1, 2, 0x11, 0x16, 0x99, 0x99 };
    assert(bnep_handle_connection_request(&channel, invalid, sizeof(invalid)) == sizeof(invalid));
    assert(incoming_events == 0);
    ready();
    assert(response == BNEP_SETUP_CONNECTION_RESPONSE_INVALID_SOURCE_UUID && opened_events == 0);
    puts("BNEP incoming connection tests passed");
    return 0;
}
