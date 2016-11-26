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


int avdtp_initiator_stream_config_subsm(avdtp_connection_t * connection, avdtp_stream_endpoint_t * stream_endpoint, avdtp_signaling_packet_header_t * signaling_header, uint8_t *packet, uint16_t size){
    //if (!stream_endpoint) return 0;
    if (avdtp_initiator_stream_config_subsm_run(connection, stream_endpoint)) return 1;

    int responded = 1;
    
    switch (stream_endpoint->initiator_config_state){
        case AVDTP_INITIATOR_W4_CAPABILITIES:
            printf("    INT : AVDTP_INITIATOR_W4_CAPABILITIES, Received basic capabilities -> NOT IMPLEMENTED\n");
            return 0;
        case AVDTP_INITIATOR_W4_ALL_CAPABILITIES:
            printf("    INT : AVDTP_INITIATOR_W4_ALL_CAPABILITIES -> NOT IMPLEMENTED\n");
            return 0;
        default:
            printf("    INT : NOT IMPLEMENTED sig. ID %02x\n", signaling_header->signal_identifier);
            //printf_hexdump( packet, size );
            return 0;
    }
    return responded;
}

int avdtp_initiator_stream_config_subsm_run(avdtp_connection_t * connection, avdtp_stream_endpoint_t * stream_endpoint){
    int sent = 1;
    switch (stream_endpoint->initiator_config_state){
        case AVDTP_INITIATOR_W2_GET_CAPABILITIES:
            printf("    INT: AVDTP_INITIATOR_W2_GET_CAPABILITIES -> AVDTP_INITIATOR_W4_CAPABILITIES\n");
            stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W4_CAPABILITIES;
            avdtp_initiator_send_get_capabilities_cmd(connection->l2cap_signaling_cid, connection->initiator_transaction_label, connection->query_seid);
            break;
        case AVDTP_INITIATOR_W2_GET_ALL_CAPABILITIES:
            printf("    INT: AVDTP_INITIATOR_W2_GET_ALL_CAPABILITIES -> AVDTP_INITIATOR_W4_ALL_CAPABILITIES\n");
            stream_endpoint->initiator_config_state = AVDTP_INITIATOR_W4_ALL_CAPABILITIES;
            avdtp_initiator_send_get_all_capabilities_cmd(connection->l2cap_signaling_cid, connection->initiator_transaction_label, connection->query_seid);
            break;
        default:
            sent = 0;
            break;
    }  
    return sent;
}
