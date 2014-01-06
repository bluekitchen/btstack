/*
 * Copyright (C) 2011-2012 BlueKitchen GmbH
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
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 * Please inquire about commercial licensing options at contact@bluekitchen-gmbh.com
 *
 */

//
// ATT Server Globals
//

#include "att_server.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include <btstack/run_loop.h>
#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#include "l2cap.h"

#include "sm.h"
#include "att.h"
#include "att_server.h"
#include "gap_le.h"
#include "central_device_db.h"

static void att_run(void);

typedef enum {
    ATT_SERVER_IDLE,
    ATT_SERVER_REQUEST_RECEIVED,
    ATT_SERVER_W4_SIGNED_WRITE_VALIDATION,
} att_server_state_t;

static att_connection_t att_connection;
static att_server_state_t att_server_state;

static uint16_t  att_request_handle = 0;
static uint16_t  att_request_size   = 0;
static uint8_t   att_request_buffer[28];

static int       att_ir_central_device_db_index = -1;
static int       att_ir_lookup_active = 0;

static int       att_connection_encrypted;

static btstack_packet_handler_t att_client_packet_handler = NULL;

static void att_event_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    
    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                
                case DAEMON_EVENT_HCI_PACKET_SENT:
                    att_run();
                    break;
                    
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            // reset connection MTU
                            att_connection.mtu = 23;
                            att_connection_encrypted = 0;
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_ENCRYPTION_CHANGE: 
                	// check handle
                	if (att_request_handle != READ_BT_16(packet, 3)) break;
                	att_connection_encrypted = packet[5];
                	break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // restart advertising if we have been connected before
                    // -> avoid sending advertise enable a second time before command complete was received 
                    att_server_state = ATT_SERVER_IDLE;
                    att_request_handle = 0;
                    att_connection_encrypted = 0;
                    break;
                    
                case SM_IDENTITY_RESOLVING_STARTED:
                    att_ir_lookup_active = 1;
                    break;
                case SM_IDENTITY_RESOLVING_SUCCEEDED:
                    att_ir_lookup_active = 0;
                    att_ir_central_device_db_index = ((sm_event_t*) packet)->central_device_db_index;
                    att_run();
                    break;
                case SM_IDENTITY_RESOLVING_FAILED:
                    att_ir_lookup_active = 0;
                    att_ir_central_device_db_index = -1;
                    att_run();
                    break;
                default:
                    break;
            }
    }
    if (att_client_packet_handler){
        att_client_packet_handler(packet_type, channel, packet, size);
    }
}

static void att_signed_write_handle_cmac_result(uint8_t hash[8]){
    
    if (att_server_state != ATT_SERVER_W4_SIGNED_WRITE_VALIDATION) return;

    if (memcmp(hash, &att_request_buffer[att_request_size-8], 8)){
        printf("ATT Signed Write, invalid signature\n");
        att_server_state = ATT_SERVER_IDLE;
        return;
    }

    // update sequence number
    uint32_t counter_packet = READ_BT_32(att_request_buffer, att_request_size-12);
    central_device_db_counter_set(att_ir_central_device_db_index, counter_packet+1);
    // just treat signed write command as simple write command after validation
    att_request_buffer[0] = ATT_WRITE_COMMAND;
    att_server_state = ATT_SERVER_REQUEST_RECEIVED;
    att_run();
}

static void att_run(void){
    switch (att_server_state){
        case ATT_SERVER_IDLE:
        case ATT_SERVER_W4_SIGNED_WRITE_VALIDATION:
            return;
        case ATT_SERVER_REQUEST_RECEIVED:
            if (att_request_buffer[0] == ATT_SIGNED_WRITE_COMAND){
                printf("ATT Signed Write!\n");
                if (!sm_cmac_ready()) {
                    printf("ATT Signed Write, sm_cmac engine not ready. Abort\n");
                    att_server_state = ATT_SERVER_IDLE;
                     return;
                }  
                if (att_request_size < (3 + 12)) {
                    printf("ATT Signed Write, request to short. Abort.\n");
                    att_server_state = ATT_SERVER_IDLE;
                    return;
                }
                if (att_ir_lookup_active){
                    return;
                }
                if (att_ir_central_device_db_index < 0){
                    printf("ATT Signed Write, CSRK not available\n");
                    att_server_state = ATT_SERVER_IDLE;
                    return;
                }

                // check counter
                uint32_t counter_packet = READ_BT_32(att_request_buffer, att_request_size-12);
                uint32_t counter_db     = central_device_db_counter_get(att_ir_central_device_db_index);
                printf("ATT Signed Write, DB counter %u, packet counter %u\n", counter_db, counter_packet);
                if (counter_packet < counter_db){
                    printf("ATT Signed Write, db reports higher counter, abort\n");
                    att_server_state = ATT_SERVER_IDLE;
                    return;
                }

                // signature is { sequence counter, secure hash }
                sm_key_t csrk;
                central_device_db_csrk(att_ir_central_device_db_index, csrk);
                att_server_state = ATT_SERVER_W4_SIGNED_WRITE_VALIDATION;
                sm_cmac_start(csrk, att_request_size - 8, att_request_buffer, att_signed_write_handle_cmac_result);
                return;
            } 

            if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)) return;

            uint8_t  att_response_buffer[28];
            uint16_t att_response_size = att_handle_request(&att_connection, att_request_buffer, att_request_size, att_response_buffer);
            att_server_state = ATT_SERVER_IDLE;
            if (att_response_size == 0) return;

            l2cap_send_connectionless(att_request_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, att_response_buffer, att_response_size);
            break;
    }
}

static void att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
    if (packet_type != ATT_DATA_PACKET) return;

    // check size
    if (size > sizeof(att_request_buffer)) return;

    // last request still in processing?
    if (att_server_state != ATT_SERVER_IDLE) return;

    // store request
    att_server_state = ATT_SERVER_REQUEST_RECEIVED;
    att_request_size = size;
    att_request_handle = handle;
    memcpy(att_request_buffer, packet, size);

    att_run();
}

void att_server_init(uint8_t const * db, att_read_callback_t read_callback, att_write_callback_t write_callback){

    sm_register_packet_handler(att_event_packet_handler);

    l2cap_register_fixed_channel(att_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);

    att_server_state = ATT_SERVER_IDLE;
    att_set_db(db);
    att_set_read_callback(read_callback);
    att_set_write_callback(write_callback);

}

void att_server_register_packet_handler(btstack_packet_handler_t handler){
    att_client_packet_handler = handler;    
}
