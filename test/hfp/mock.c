/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */
 
// *****************************************************************************
//
// HFP BTstack Mocks
//
// *****************************************************************************

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/btstack.h>
#include "hci.h"
#include "hci_dump.h"
#include "sdp_query_rfcomm.h"
#include "rfcomm.h"
#include "hfp_hf.h"

#include "mock.h"

static void *registered_sdp_app_context;
static uint8_t sdp_rfcomm_channel_nr = 1;
const char sdp_rfcomm_service_name[] = "BTstackMock";
static uint16_t rfcomm_cid = 1;
static bd_addr_t dev_addr;
static uint16_t sco_handle = 10;
static uint8_t rfcomm_payload[200];
static uint16_t rfcomm_payload_len = 0;

static uint8_t outgoing_rfcomm_payload[200];
static uint16_t outgoing_rfcomm_payload_len = 0;

static uint8_t rfcomm_reserved_buffer[1000];

void * active_connection;
hfp_connection_t * hfp_context;

void (*registered_rfcomm_packet_handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
void (*registered_sdp_app_callback)(sdp_query_event_t * event, void * context);

uint8_t * get_rfcomm_payload(){
	return &rfcomm_payload[0];
}

uint16_t get_rfcomm_payload_len(){
	return rfcomm_payload_len;
}

static int hfp_command_start_index = 0;


int has_more_hfp_commands(int start_command_offset, int end_command_offset){
    int has_cmd = get_rfcomm_payload_len() - hfp_command_start_index >= 2 + start_command_offset + end_command_offset;
    //printf("has more: payload len %d, start %d, has more %d\n", get_rfcomm_payload_len(), hfp_command_start_index, has_cmd);
    return has_cmd; 
}

char * get_next_hfp_command(int start_command_offset, int end_command_offset){
    //printf("get next: payload len %d, start %d\n", get_rfcomm_payload_len(), hfp_command_start_index);
    char * data = (char *)(&get_rfcomm_payload()[hfp_command_start_index + start_command_offset]);
    int data_len = get_rfcomm_payload_len() - hfp_command_start_index - start_command_offset;

    int i;

    for (i = 0; i < data_len; i++){
        if ( *(data+i) == '\r' || *(data+i) == '\n' ) {
            data[i]=0;
            // update state
            //printf("!!! command %s\n", data);
            hfp_command_start_index = hfp_command_start_index + i + start_command_offset + end_command_offset;
            return data;
        } 
    }
    printf("should not got here\n");
    return NULL;
}

void print_without_newlines(uint8_t *data, uint16_t len);
void print_without_newlines(uint8_t *data, uint16_t len){
    int found_newline = 0;
    int found_item = 0;
    
    for (int i=0; i<len; i++){
        if (data[i] == '\r' || data[i] == '\n'){
            if (!found_newline && found_item) printf("\n");
            found_newline = 1;
        } else {
            printf("%c", data[i]);
            found_newline = 0;
            found_item = 1;
        }
    }
    printf("\n");
}

extern "C" void l2cap_init(void){}

extern "C" void l2cap_register_packet_handler(void (*handler)(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)){
}

int  rfcomm_send_internal(uint16_t rfcomm_cid, uint8_t *data, uint16_t len){
	int start_command_offset = 2;
    int end_command_offset = 2;
    
    if (strncmp((char*)data, "AT", 2) == 0){
		start_command_offset = 0;
	} 
    
    if (has_more_hfp_commands(start_command_offset, end_command_offset)){
        //printf("Buffer response: ");
        strncpy((char*)&rfcomm_payload[rfcomm_payload_len], (char*)data, len);
        rfcomm_payload_len += len;
    } else {
        hfp_command_start_index = 0;
        //printf("Copy response: ");
        strncpy((char*)&rfcomm_payload[0], (char*)data, len);
        rfcomm_payload_len = len;
    }
	
    //print_without_newlines(rfcomm_payload,rfcomm_payload_len);
    return 0;
}

int       rfcomm_reserve_packet_buffer(void){
    return 1;
};
void      rfcomm_release_packet_buffer(void){};
uint8_t * rfcomm_get_outgoing_buffer(void) {
    return rfcomm_reserved_buffer;
}
uint16_t rfcomm_get_max_frame_size(uint16_t rfcomm_cid){
    return sizeof(rfcomm_reserved_buffer);
}
int rfcomm_send_prepared(uint16_t rfcomm_cid, uint16_t len){
    printf("--- rfcomm_send_prepared with len %u ---\n", len);
    return rfcomm_send_internal(rfcomm_cid, rfcomm_reserved_buffer, len);
}

static void hci_event_sco_complete(){
    uint8_t event[19];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE;
    event[pos++] = sizeof(event) - 2;

    event[pos++] = 0; //status
    bt_store_16(event,  pos, sco_handle);   pos += 2; // sco handle
    bt_flip_addr(&event[pos], dev_addr);    pos += 6;

    event[pos++] = 0; // link_type
    event[pos++] = 0; // transmission_interval
    event[pos++] = 0; // retransmission_interval

    bt_store_16(event,  pos, 0);   pos += 2; // rx_packet_length
    bt_store_16(event,  pos, 0);   pos += 2; // tx_packet_length

    event[pos++] = 0; // air_mode
    (*registered_rfcomm_packet_handler)(active_connection, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

int hci_send_cmd(const hci_cmd_t *cmd, ...){
	//printf("hci_send_cmd opcode 0x%02x\n", cmd->opcode);	
    if (cmd->opcode == 0x428){
        hci_event_sco_complete();
    }
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
	active_connection = connection;
    uint8_t event[16];
    uint8_t pos = 0;
    event[pos++] = RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = 0;
    
    bt_flip_addr(&event[pos], addr);
    memcpy(dev_addr, addr, 6);
    pos += 6;
    
    bt_store_16(event,  pos, 1);   pos += 2;
	event[pos++] = 0;
	
	bt_store_16(event, pos, rfcomm_cid); pos += 2;       // channel ID
	bt_store_16(event, pos, 200); pos += 2;   // max frame size
    (*registered_rfcomm_packet_handler)(active_connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, pos);
}

int rfcomm_can_send_packet_now(uint16_t rfcomm_cid){
	return 1;
}

void rfcomm_disconnect_internal(uint16_t rfcomm_cid){
	uint8_t event[4];
	event[0] = RFCOMM_EVENT_CHANNEL_CLOSED;
    event[1] = sizeof(event) - 2;
    bt_store_16(event, 2, rfcomm_cid);
    (*registered_rfcomm_packet_handler)(active_connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
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

void run_loop_add_timer(timer_source_t *timer){
}

int  run_loop_remove_timer(timer_source_t *timer){
    return 0;
}
void run_loop_set_timer_handler(timer_source_t *ts, void (*process)(timer_source_t *_ts)){
}

void run_loop_set_timer(timer_source_t *a, uint32_t timeout_in_ms){
}


void hci_emit_disconnection_complete(uint16_t handle, uint8_t reason){
    uint8_t event[6];
    event[0] = HCI_EVENT_DISCONNECTION_COMPLETE;
    event[1] = sizeof(event) - 2;
    event[2] = 0; // status = OK
    bt_store_16(event, 3, handle);
    event[5] = reason;
    (*registered_rfcomm_packet_handler)(active_connection, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

le_command_status_t gap_disconnect(hci_con_handle_t handle){
    hci_emit_disconnection_complete(handle, 0);
    return BLE_PERIPHERAL_OK;
}

uint16_t hci_get_sco_voice_setting(){
    return 0x40;
}

int hci_remote_eSCO_supported(hci_con_handle_t handle){
    return 0;
}

static void add_new_lines_to_hfp_command(uint8_t * data, int len){
    if (len <= 0) return;
    memset(&outgoing_rfcomm_payload, 0, 200);
    int pos = 0;

    if (strncmp((char*)data, "AT", 2) == 0){
        strncpy((char*)&outgoing_rfcomm_payload[pos], (char*)data, len);
        pos += len;
    } else {
        outgoing_rfcomm_payload[pos++] = '\r';
        outgoing_rfcomm_payload[pos++] = '\n';
        strncpy((char*)&outgoing_rfcomm_payload[pos], (char*)data, len);
        pos += len;
    }
    outgoing_rfcomm_payload[pos++] = '\r';
    outgoing_rfcomm_payload[pos++] = '\n';
    outgoing_rfcomm_payload[pos] = 0;
    outgoing_rfcomm_payload_len = pos;
}   

void inject_hfp_command_to_hf(uint8_t * data, int len){
    if (memcmp((char*)data, "AT", 2) == 0) return;
    add_new_lines_to_hfp_command(data, len);
    // printf("inject_hfp_command_to_hf to HF: ");
    // print_without_newlines(outgoing_rfcomm_payload,outgoing_rfcomm_payload_len);
    (*registered_rfcomm_packet_handler)(active_connection, RFCOMM_DATA_PACKET, rfcomm_cid, (uint8_t *) &outgoing_rfcomm_payload[0], outgoing_rfcomm_payload_len);

}

void inject_hfp_command_to_ag(uint8_t * data, int len){
    if (data[0] == '+') return;
    
    add_new_lines_to_hfp_command(data, len);
    (*registered_rfcomm_packet_handler)(active_connection, RFCOMM_DATA_PACKET, rfcomm_cid, (uint8_t *) &outgoing_rfcomm_payload[0], outgoing_rfcomm_payload_len);
}




