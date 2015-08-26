#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/btstack.h>
// #include "att.h"
#include "hci.h"
#include "hci_dump.h"
#include "sdp_query_rfcomm.h"
#include "rfcomm.h"

static void *registered_sdp_app_context;
static uint8_t sdp_rfcomm_channel_nr = 1;
const char sdp_rfcomm_service_name[] = "BTstackMock";
static rfcomm_channel_t rfcomm_channel;
static uint16_t rfcomm_cid = 1;
static uint8_t rfcomm_payload[200];
static uint16_t rfcomm_payload_len;
void * active_connection;

void (*registered_rfcomm_packet_handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
void (*registered_sdp_app_callback)(sdp_query_event_t * event, void * context);

uint8_t * get_rfcomm_payload(){
	return &rfcomm_payload[0];
}

uint16_t get_rfcomm_payload_len(){
	return rfcomm_payload_len;
}

void inject_rfcomm_command(uint8_t * data, int len){
	memset(&rfcomm_payload, 0, 200);
	rfcomm_payload_len = len;
	memcpy((char*)&rfcomm_payload[0], data, rfcomm_payload_len);
	(*registered_rfcomm_packet_handler)(active_connection, RFCOMM_DATA_PACKET, rfcomm_cid, (uint8_t *) &rfcomm_payload[0], rfcomm_payload_len);
}

int  rfcomm_send_internal(uint16_t rfcomm_cid, uint8_t *data, uint16_t len){
	printf("rfcomm_send_internal %s\n", data);
	memset(&rfcomm_payload, 0, 200);
	rfcomm_payload_len = len;
	memcpy((char*)&rfcomm_payload, data, rfcomm_payload_len);
	return 0;
}

int hci_send_cmd(const hci_cmd_t *cmd, ...){
	printf("hci_send_cmd opcode 0x%02x\n", cmd->opcode);	
	return 0;
}


void sdp_query_rfcomm_register_callback(void(*sdp_app_callback)(sdp_query_event_t * event, void * context), void * context){
	registered_sdp_app_callback = sdp_app_callback;
	registered_sdp_app_context = context;
}

static void sdp_query_complete_response(uint8_t status){
    sdp_query_complete_event_t complete_event = {
        SDP_QUERY_COMPLETE, 
        status
    };
    (*registered_sdp_app_callback)((sdp_query_event_t*)&complete_event, registered_sdp_app_context);
}

static void sdp_query_rfcomm_service_response(uint8_t status){
    sdp_query_rfcomm_service_event_t service_event = {
        SDP_QUERY_RFCOMM_SERVICE, 
        sdp_rfcomm_channel_nr,
        (uint8_t *)sdp_rfcomm_service_name
    };
    (*registered_sdp_app_callback)((sdp_query_event_t*)&service_event, registered_sdp_app_context);
}

void sdp_query_rfcomm_channel_and_name_for_uuid(bd_addr_t remote, uint16_t uuid){
	// printf("sdp_query_rfcomm_channel_and_name_for_uuid %p\n", registered_sdp_app_callback);
	sdp_query_rfcomm_service_response(0);
	sdp_query_complete_response(0);
}

void rfcomm_create_channel_internal(void * connection, bd_addr_t addr, uint8_t channel){
	// RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE
	// printf("rfcomm_create_channel_internal\n");
	active_connection = connection;
    uint8_t event[16];
    uint8_t pos = 0;
    event[pos++] = RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = 0;
    
    bt_flip_addr(&event[pos], addr); pos += 6;
    bt_store_16(event,  pos, 1);   pos += 2;
	event[pos++] = 0;
	
	bt_store_16(event, pos, rfcomm_cid); pos += 2;       // channel ID
	bt_store_16(event, pos, 200); pos += 2;   // max frame size
    (*registered_rfcomm_packet_handler)(connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, pos);
}



void rfcomm_disconnect_internal(uint16_t rfcomm_cid){
	printf("rfcomm_disconnect_internal\n");
}

void rfcomm_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)){
	registered_rfcomm_packet_handler = handler;
}

void rfcomm_register_service_internal(void * connection, uint8_t channel, uint16_t max_frame_size){
	printf("rfcomm_register_service_internal\n");
}


void sdp_query_rfcomm_channel_and_name_for_search_pattern(bd_addr_t remote, uint8_t * des_serviceSearchPattern){
	printf("sdp_query_rfcomm_channel_and_name_for_search_pattern\n");
}


void rfcomm_accept_connection_internal(uint16_t rfcomm_cid){
	printf("rfcomm_accept_connection_internal \n");
}

