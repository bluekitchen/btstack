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

#include "hci.h"
#include "hci_dump.h"
#include "classic/sdp_client_rfcomm.h"
#include "classic/rfcomm.h"
#include "classic/hfp_hf.h"
#include "classic/sdp_client.h"
#include "classic/sdp_client_rfcomm.h"
#include "hci.h"
#include "l2cap.h"

#include "mock.h"

static uint8_t sdp_rfcomm_channel_nr = 1;
const char sdp_rfcomm_service_name[] = "BTstackMock";
static uint16_t rfcomm_cid = 1;
static bd_addr_t dev_addr;
static uint16_t sco_handle = 10;
static uint8_t rfcomm_payload[1000];
static uint16_t rfcomm_payload_len = 0;

static uint8_t outgoing_rfcomm_payload[1000];
static uint16_t outgoing_rfcomm_payload_len = 0;

static uint8_t rfcomm_reserved_buffer[1000];

hfp_connection_t * hfp_context;

void (*registered_hci_packet_handler)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
void (*registered_rfcomm_packet_handler)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
void (*registered_sdp_app_callback)(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

uint8_t * get_rfcomm_payload(void){
	return &rfcomm_payload[0];
}

uint16_t get_rfcomm_payload_len(void){
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

void l2cap_init(void){}
void hci_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
    registered_hci_packet_handler = callback_handler->callback;
}

uint8_t rfcomm_send(uint16_t rfcomm_cid, uint8_t *data, uint16_t len){

    // printf("mock: rfcomm send: ");
    // print_without_newlines(data, len);

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
	
    // print_without_newlines(rfcomm_payload,rfcomm_payload_len);
    return ERROR_CODE_SUCCESS;
}

uint8_t rfcomm_request_can_send_now_event(uint16_t rfcomm_cid){

    // printf("mock: rfcomm_request_can_send_now_event\n");

    uint8_t event[] = { RFCOMM_EVENT_CAN_SEND_NOW, 2, 0, 0};
    little_endian_store_16(event, 2, rfcomm_cid);
    registered_rfcomm_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
    return ERROR_CODE_SUCCESS;
}

void rfcomm_reserve_packet_buffer(void){
    // printf("mock: rfcomm_reserve_packet_buffer\n");
};
void rfcomm_release_packet_buffer(void){};
uint8_t * rfcomm_get_outgoing_buffer(void) {
    return rfcomm_reserved_buffer;
}
uint16_t rfcomm_get_max_frame_size(uint16_t rfcomm_cid){
    return sizeof(rfcomm_reserved_buffer);
}
uint8_t rfcomm_send_prepared(uint16_t rfcomm_cid, uint16_t len){
    // printf("--- rfcomm_send_prepared with len %u ---\n", len);
    return rfcomm_send(rfcomm_cid, rfcomm_reserved_buffer, len);
}

static void hci_event_sco_complete(void){
    uint8_t event[19];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE;
    event[pos++] = sizeof(event) - 2;

    event[pos++] = 0; //status
    little_endian_store_16(event,  pos, sco_handle);   pos += 2; // sco handle
    reverse_bd_addr(dev_addr, &event[pos]);    pos += 6;

    event[pos++] = 0; // link_type
    event[pos++] = 0; // transmission_interval
    event[pos++] = 0; // retransmission_interval

    little_endian_store_16(event,  pos, 0);   pos += 2; // rx_packet_length
    little_endian_store_16(event,  pos, 0);   pos += 2; // tx_packet_length

    event[pos++] = 0; // air_mode
    (*registered_hci_packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

uint8_t hci_send_cmd(const hci_cmd_t *cmd, ...){
	//printf("hci_send_cmd opcode 0x%02x\n", cmd->opcode);	
    if (cmd->opcode == 0x428){
        hci_event_sco_complete();
    }
	return ERROR_CODE_SUCCESS;
}

bool hci_can_send_command_packet_now(void){
    return true;
}

static void sdp_query_complete_response(uint8_t status){
    uint8_t event[3];
    event[0] = SDP_EVENT_QUERY_COMPLETE;
    event[1] = 1;
    event[2] = status;
    (*registered_sdp_app_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void sdp_client_query_rfcomm_service_response(uint8_t status){
    int sdp_service_name_len = strlen(sdp_rfcomm_service_name);
    uint8_t event[3+SDP_SERVICE_NAME_LEN+1];
    event[0] = SDP_EVENT_QUERY_RFCOMM_SERVICE;
    event[1] = sdp_service_name_len + 1;
    event[2] = sdp_rfcomm_channel_nr;
    memcpy(&event[3], sdp_rfcomm_service_name, sdp_service_name_len);
    event[3+sdp_service_name_len] = 0;
    (*registered_sdp_app_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

uint8_t sdp_client_query_rfcomm_channel_and_name_for_service_class_uuid(btstack_packet_handler_t callback, bd_addr_t remote, uint16_t uuid){
	// printf("sdp_client_query_rfcomm_channel_and_name_for_uuid %p\n", registered_sdp_app_callback);
    registered_sdp_app_callback = callback;
	sdp_client_query_rfcomm_service_response(0);
	sdp_query_complete_response(0);
    return 0;
}

uint8_t sdp_client_register_query_callback(btstack_context_callback_registration_t * callback_registration){
    (callback_registration->callback)(callback_registration->context);
    return ERROR_CODE_SUCCESS;
}

uint8_t rfcomm_create_channel(btstack_packet_handler_t handler, bd_addr_t addr, uint8_t channel, uint16_t * out_cid){

    // printf("mock: rfcomm_create_channel addr %s\n", bd_addr_to_str(addr));

    registered_rfcomm_packet_handler = handler;

	// RFCOMM_EVENT_CHANNEL_OPENED
    uint8_t event[16];
    uint8_t pos = 0;
    event[pos++] = RFCOMM_EVENT_CHANNEL_OPENED;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = 0;
    
    reverse_bd_addr(addr, &event[pos]);
    memcpy(dev_addr, addr, 6);
    pos += 6;
    
    little_endian_store_16(event,  pos, 1);   pos += 2;
	event[pos++] = 0;
	
	little_endian_store_16(event, pos, rfcomm_cid); pos += 2;       // channel ID
	little_endian_store_16(event, pos, 200); pos += 2;   // max frame size
    (*registered_rfcomm_packet_handler)(HCI_EVENT_PACKET, 0, (uint8_t *) event, pos);

    if (out_cid){
        *out_cid = rfcomm_cid;
    }
    return ERROR_CODE_SUCCESS;
}

bool rfcomm_can_send_packet_now(uint16_t rfcomm_cid){
    // printf("mock: rfcomm_can_send_packet_now\n");
	return true;
}

uint8_t rfcomm_disconnect(uint16_t rfcomm_cid){
	uint8_t event[4];
	event[0] = RFCOMM_EVENT_CHANNEL_CLOSED;
    event[1] = sizeof(event) - 2;
    little_endian_store_16(event, 2, rfcomm_cid);
    (*registered_rfcomm_packet_handler)(HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
    return ERROR_CODE_SUCCESS;
}

uint8_t rfcomm_register_service(btstack_packet_handler_t handler, uint8_t channel, uint16_t max_frame_size){
	// printf("rfcomm_register_service\n");
    registered_rfcomm_packet_handler = handler;
    return ERROR_CODE_SUCCESS;
}


uint8_t sdp_client_query_rfcomm_channel_and_name_for_search_pattern(btstack_packet_handler_t callback, bd_addr_t remote, const uint8_t * des_serviceSearchPattern){
	// printf("sdp_client_query_rfcomm_channel_and_name_for_search_pattern\n");
    return 0;
}


uint8_t rfcomm_accept_connection(uint16_t rfcomm_cid){
	// printf("rfcomm_accept_connection \n");
    return ERROR_CODE_SUCCESS;
}

uint8_t rfcomm_decline_connection(uint16_t rfcomm_cid){
    // printf("rfcomm_accept_connection \n");
    return ERROR_CODE_SUCCESS;
}

void btstack_run_loop_add_timer(btstack_timer_source_t *timer){
}

int  btstack_run_loop_remove_timer(btstack_timer_source_t *timer){
    return 0;
}
void btstack_run_loop_set_timer_handler(btstack_timer_source_t *ts, void (*process)(btstack_timer_source_t *_ts)){
}

void btstack_run_loop_set_timer(btstack_timer_source_t *a, uint32_t timeout_in_ms){
}


static void hci_emit_disconnection_complete(uint16_t handle, uint8_t reason){
    uint8_t event[6];
    event[0] = HCI_EVENT_DISCONNECTION_COMPLETE;
    event[1] = sizeof(event) - 2;
    event[2] = 0; // status = OK
    little_endian_store_16(event, 3, handle);
    event[5] = reason;
    (*registered_hci_packet_handler)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

uint8_t gap_disconnect(hci_con_handle_t handle){
    hci_emit_disconnection_complete(handle, 0);
    return 0;
}

uint16_t hci_get_sco_voice_setting(void){
    return 0x40;
}

bool hci_remote_esco_supported(hci_con_handle_t handle){
    return false;
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
    (*registered_rfcomm_packet_handler)(RFCOMM_DATA_PACKET, rfcomm_cid, (uint8_t *) &outgoing_rfcomm_payload[0], outgoing_rfcomm_payload_len);

}

void inject_hfp_command_to_ag(uint8_t * data, int len){
    if (data[0] == '+') return;
    
    add_new_lines_to_hfp_command(data, len);

    // printf("mock: inject command to ag: ");
    // print_without_newlines(data, len);

    (*registered_rfcomm_packet_handler)(RFCOMM_DATA_PACKET, rfcomm_cid, (uint8_t *) &outgoing_rfcomm_payload[0], outgoing_rfcomm_payload_len);
}


bool hci_extended_sco_link_supported(void){
    return true;
}

bool gap_secure_connection(hci_con_handle_t con_handle){
    UNUSED(con_handle);
    return true;
}

uint16_t hci_remote_sco_packet_types(hci_con_handle_t con_handle){
    UNUSED(con_handle);
    return SCO_PACKET_TYPES_ALL;
}
uint16_t hci_usable_sco_packet_types(void){
    return SCO_PACKET_TYPES_ALL;
}
