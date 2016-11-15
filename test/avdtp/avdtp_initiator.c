/*
 * Copyright (C) 2016 BlueKitchen GmbH
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


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack.h"
#include "avdtp.h"
#include "avdtp_util.h"
#include "avdtp_initiator.h"

static int avdtp_initiator_send_signaling_cmd(uint16_t cid, avdtp_signal_identifier_t identifier, uint8_t transaction_label){
    uint8_t command[2];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_CMD_MSG);
    command[1] = (uint8_t)identifier;
    return l2cap_send(cid, command, sizeof(command));
}

static int avdtp_initiator_send_get_all_capabilities_cmd(uint16_t cid, uint8_t transaction_label, uint8_t sep_id){
    uint8_t command[3];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_CMD_MSG);
    command[1] = AVDTP_SI_GET_ALL_CAPABILITIES;
    command[2] = sep_id << 2;
    return l2cap_send(cid, command, sizeof(command));
}

static int avdtp_initiator_send_get_capabilities_cmd(uint16_t cid, uint8_t transaction_label, uint8_t sep_id){
    uint8_t command[3];
    command[0] = avdtp_header(transaction_label, AVDTP_SINGLE_PACKET, AVDTP_CMD_MSG);
    command[1] = AVDTP_SI_GET_CAPABILITIES;
    command[2] = sep_id << 2;
    return l2cap_send(cid, command, sizeof(command));
}

void avdtp_initiator_stream_config_subsm_init(avdtp_stream_endpoint_t * stream_endpoint){
    stream_endpoint->initiator_config_state = AVDTP_INITIATOR_STREAM_CONFIG_IDLE;
}

int avdtp_initiator_stream_config_subsm_is_done(avdtp_stream_endpoint_t * stream_endpoint){
    return stream_endpoint->initiator_config_state == AVDTP_INITIATOR_STREAM_CONFIG_DONE;
}

int avdtp_initiator_stream_config_subsm(avdtp_device_t * device, avdtp_stream_endpoint_t * stream_endpoint, uint8_t *packet, uint16_t size){
    return 0;
    if (avdtp_initiator_stream_config_subsm_run(device, stream_endpoint)) return 1;
    int i;
    int responded = 1;
    avdtp_sep_t sep;
    
    avdtp_signaling_packet_header_t signaling_header;
    avdtp_read_signaling_header(&signaling_header, packet, size);
    
    switch (stream_endpoint->initiator_config_state){
        case AVDTP_INITIATOR_W4_SEPS_DISCOVERED:
            printf("    AVDTP_INITIATOR_W4_SEPS_DISCOVERED -> AVDTP_INITIATOR_W2_GET_CAPABILITIES\n");
            
            if (signaling_header.transaction_label != device->initiator_transaction_label){
                printf("unexpected transaction label, got %d, expected %d\n", signaling_header.transaction_label, device->initiator_transaction_label);
                return 0;
            }
            if (signaling_header.signal_identifier != AVDTP_SI_DISCOVER) {
                printf("unexpected signal identifier ...\n");
                return 0;
            }
            if (signaling_header.message_type != AVDTP_RESPONSE_ACCEPT_MSG){
                printf("request rejected...\n");
                return 0;   
            }
            if (size == 3){
                printf("ERROR code %02x\n", packet[2]);
                return 0;
            }
            for (i = 2; i<size; i+=2){
                sep.seid = packet[i] >> 2;
                if (sep.seid < 0x01 || sep.seid > 0x3E){
                    printf("invalid sep id\n");
                    return 0;
                }
                sep.in_use = (packet[i] >> 1) & 0x01;
                sep.media_type = (avdtp_media_type_t)(packet[i+1] >> 4);
                sep.type = (avdtp_sep_type_t)((packet[i+1] >> 3) & 0x01);
                stream_endpoint->remote_seps[stream_endpoint->remote_seps_num++] = sep;
                // printf("found sep: seid %u, in_use %d, media type %d, sep type %d (1-SNK)\n", 
                //     sep.seid, sep.in_use, sep.media_type, sep.type);
            }
            stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W2_GET_CAPABILITIES;
            device->initiator_transaction_label++;
            l2cap_request_can_send_now_event(device->l2cap_signaling_cid);
            responded = 1;
            break;
        case AVDTP_INITIATOR_W4_CAPABILITIES:
            printf("    Received basic capabilities -> NOT IMPLEMENTED\n");
            responded = 0;
            break;
        case AVDTP_INITIATOR_W4_ALL_CAPABILITIES:
            printf("    Received all capabilities -> NOT IMPLEMENTED\n");
            responded = 0;
            break;
        default:
            printf("    INT : NOT IMPLEMENTED sig. ID %02x\n", signaling_header.signal_identifier);
            //printf_hexdump( packet, size );
            responded = 0;
            break;
    }
    return responded;
}

int avdtp_initiator_stream_config_subsm_run(avdtp_device_t * device, avdtp_stream_endpoint_t * stream_endpoint){
    return 0;
    int sent = 1;
    switch (stream_endpoint->initiator_config_state){
        case AVDTP_INITIATOR_STREAM_CONFIG_IDLE:
        case AVDTP_INITIATOR_W2_DISCOVER_SEPS:
            printf("    AVDTP_INITIATOR_STREAM_CONFIG_IDLE | AVDTP_INITIATOR_W2_DISCOVER_SEPS -> AVDTP_INITIATOR_W4_SEPS_DISCOVERED\n");
            stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W4_SEPS_DISCOVERED;
            avdtp_initiator_send_signaling_cmd(device->l2cap_signaling_cid, AVDTP_SI_DISCOVER, device->initiator_transaction_label);
            break;
        case AVDTP_INITIATOR_W2_GET_CAPABILITIES:
            printf("    AVDTP_INITIATOR_W2_GET_CAPABILITIES -> AVDTP_INITIATOR_W4_CAPABILITIES\n");
            stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W4_CAPABILITIES;
            avdtp_initiator_send_get_capabilities_cmd(device->l2cap_signaling_cid, device->initiator_transaction_label, device->query_seid);
            break;
        case AVDTP_INITIATOR_W2_GET_ALL_CAPABILITIES:
            printf("    AVDTP_INITIATOR_W2_GET_ALL_CAPABILITIES -> AVDTP_INITIATOR_W4_ALL_CAPABILITIES\n");
            stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W4_ALL_CAPABILITIES;
            avdtp_initiator_send_get_all_capabilities_cmd(device->l2cap_signaling_cid, device->initiator_transaction_label, device->query_seid);
            break;
        default:
            sent = 0;
            break;
    }  
    return sent;
}
