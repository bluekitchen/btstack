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
// minimal setup for SDP client over USB or UART
//
// *****************************************************************************

#include "btstack-config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdp_parser.h"
#include "sdp_client.h"
#include "sdp_query_util.h"

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "sdp_parser.h"

int record_id = -1;
int attribute_id = -1;

static uint8_t   attribute_value[1000];
static const int attribute_value_buffer_size = sizeof(attribute_value);

static bd_addr_t remote = {0x04,0x0C,0xCE,0xE4,0x85,0xD3};

static void handle_sdp_client_query_result(sdp_query_event_t * event);

static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    // printf("packet_handler type %u, packet[0] %x\n", packet_type, packet[0]);

    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = packet[0];

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (packet[2] == HCI_STATE_WORKING){
                sdp_general_query_for_uuid(remote, 0x1002);
            }
            break;
        default:
            break;
    }
}

static void assertBuffer(int size){
    if (size > attribute_value_buffer_size){
        printf("SDP attribute value buffer size exceeded: available %d, required %d", attribute_value_buffer_size, size);
    }
}

static void handle_sdp_client_query_result(sdp_query_event_t * event){
    sdp_query_attribute_value_event_t * ve;
    sdp_query_complete_event_t * ce;

    switch (event->type){
        case SDP_QUERY_ATTRIBUTE_VALUE:
            ve = (sdp_query_attribute_value_event_t*) event;
            
            // handle new record
            if (ve->record_id != record_id){
                record_id = ve->record_id;
                printf("\n---\nRecord nr. %u\n", record_id);
            }

            assertBuffer(ve->attribute_length);

            attribute_value[ve->data_offset] = ve->data;
            if ((uint16_t)(ve->data_offset+1) == ve->attribute_length){
               printf("Attribute 0x%04x: ", ve->attribute_id);
               de_dump_data_element(attribute_value);
            }
            break;
        case SDP_QUERY_COMPLETE:
            ce = (sdp_query_complete_event_t*) event;
            printf("General query done with status %d.\n\n", ce->status);
            exit(0);
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
 
    printf("Client HCI init done\r\n");
    
    // init L2CAP
    l2cap_init();
    l2cap_register_packet_handler(packet_handler);

    sdp_parser_init();
    sdp_parser_register_callback(handle_sdp_client_query_result);

    // turn on!
    hci_power_control(HCI_POWER_ON);
            
    // go!
    run_loop_execute(); 
    return 0;
}
