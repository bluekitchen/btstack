/*
 * Copyright (C) 2011-2013 by Matthias Ringwald
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
 * 4. This software may not be used in a commercial product
 *    without an explicit license granted by the copyright holder. 
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
 */

//*****************************************************************************
//
// att device demo
//
//*****************************************************************************

// TODO: seperate BR/EDR from LE ACL buffers
// ..

// NOTE: Supports only a single connection

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
#include "gap_le.h"
#include "central_device_db.h"


//
// ATT Server Globals
//

// test profile
#include "profile.h"

static att_connection_t att_connection;

typedef enum {
    ATT_SERVER_IDLE,
    ATT_SERVER_REQUEST_RECEIVED,
    ATT_SERVER_W4_SIGNED_WRITE_VALIDATION,
} att_server_state_t;

static void att_run(void);

static att_server_state_t att_server_state;

static uint16_t  att_request_handle = 0;
static uint16_t  att_request_size   = 0;
static uint8_t   att_request_buffer[28];

static int       att_advertisements_enabled = 0;

static int       att_ir_central_device_db_index = -1;
static int       att_ir_lookup_active = 0;

static void app_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    
    uint8_t adv_data[] = { 02, 01, 05,   03, 02, 0xf0, 0xff }; 

    switch (packet_type) {
            
        case HCI_EVENT_PACKET:
            switch (packet[0]) {
                
                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started
                    if (packet[2] == HCI_STATE_WORKING) {
                        printf("SM Init completed\n");
                        hci_send_cmd(&hci_le_set_advertising_data, sizeof(adv_data), adv_data);
                    }
                    break;
                
                case DAEMON_EVENT_HCI_PACKET_SENT:
                    att_run();
                    break;
                    
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            // reset connection MTU
                            att_connection.mtu = 23;
                            att_advertisements_enabled = 0;
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // restart advertising if we have been connected before
                    // -> avoid sending advertise enable a second time before command complete was received 
                    if (att_advertisements_enabled == 0) {
                        hci_send_cmd(&hci_le_set_advertise_enable, 1);
                        att_advertisements_enabled = 1;
                    }
                    att_server_state = ATT_SERVER_IDLE;
                    att_request_handle = 0;
                    break;
                    
                case HCI_EVENT_COMMAND_COMPLETE:
                    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_advertising_data)){
                         // only needed for BLE Peripheral
                       hci_send_cmd(&hci_le_set_scan_response_data, 10, adv_data);
                       break;
                    }
                    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_scan_response_data)){
                         // only needed for BLE Peripheral
                       hci_send_cmd(&hci_le_set_advertise_enable, 1);
                       att_advertisements_enabled = 1;
                       break;
                    }
                    break;
                case SM_IDENTITY_RESOLVING_STARTED:
                    att_ir_lookup_active = 1;
                    break;
                case SM_IDENTITY_RESOLVING_SUCCEEDED:
                    att_ir_lookup_active = 0;
                    att_ir_central_device_db_index = ((sm_event_identity_resolving_t*) packet)->central_device_db_index;
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

// write requests
static void att_write_callback(uint16_t handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size, signature_t * signature){
    printf("WRITE Callback, handle %04x\n", handle);
    switch(handle){
        case 0x000b:
            buffer[buffer_size]=0;
            printf("New text: %s\n", buffer);
            break;
        case 0x000d:
            printf("New value: %u\n", buffer[0]);
            break;
    }
}

void setup(void){
    /// GET STARTED with BTstack ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
        
    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);

    // init HCI
    hci_transport_t    * transport = hci_transport_usb_instance();
    hci_uart_config_t  * config    = NULL;
    bt_control_t       * control   = NULL;
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
        
    hci_init(transport, config, control, remote_db);

    // setup central device db
    central_device_db_init();

    // set up l2cap_le
    l2cap_init();
    
    // set up ATT
    l2cap_register_fixed_channel(att_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);
    att_server_state = ATT_SERVER_IDLE;
    att_set_db(profile_data);
    att_set_write_callback(att_write_callback);
    att_dump_attributes();
    att_connection.mtu = 27;

    // setup SM
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements( SM_AUTHREQ_BONDING );
    sm_set_request_security(1);
    sm_register_packet_handler(app_packet_handler);
}

int main(void)
{
    setup();
    // gap_random_address_set_update_period(5000);
    gap_random_address_set_mode(GAP_RANDOM_ADDRESS_RESOLVABLE);

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    // go!
    run_loop_execute(); 
    
    // happy compiler!
    return 0;
}
