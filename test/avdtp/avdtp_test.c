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

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "stdin_support.h"
#include "avdtp_sink.h"

static bd_addr_t remote = {0x04, 0x0C, 0xCE, 0xE4, 0x85, 0xD3};
static uint8_t sdp_avdtp_sink_service_buffer[150];

uint16_t local_cid = 0;
static btstack_packet_callback_registration_t hci_event_callback_registration;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    bd_addr_t event_addr;
    
    switch (packet_type) {
        case L2CAP_DATA_PACKET:
            // just dump data for now
            printf("source cid %x -- ", channel);
            // printf_hexdump( packet, size );
            break;
            
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {

                case BTSTACK_EVENT_STATE:
                    // bt stack activated, get started 
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        
                    }
                    break;
                case HCI_EVENT_PIN_CODE_REQUEST:
                    // inform about pin code request
                    printf("Pin code request - using '0000'\n");
                    hci_event_pin_code_request_get_bd_addr(packet, event_addr);
                    hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    // connection closed -> quit tes app
                    printf("HCI_EVENT_DISCONNECTION_COMPLETE\n");
                    
                    // exit(0);
                    break;
                    
                default:
                    // other event
                    break;
            }
            break;
            
        default:
            // other packet type
            break;
    }
}

static void show_usage(void){
    printf("\n--- CLI for L2CAP TEST ---\n");
    printf("c      - create connection to SDP at addr %s\n", bd_addr_to_str(remote));
    printf("d      - disconnect\n");
    printf("Ctrl-c - exit\n");
    printf("---\n");
}

static void stdin_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){
    char buffer;
    read(ds->fd, &buffer, 1);
    switch (buffer){
        case 'c':
            printf("Creating L2CAP Connection to %s, PSM_AVDTP\n", bd_addr_to_str(remote));
            avdtp_sink_connect(remote);
            break;
        case 'd':
            printf("L2CAP Channel Closed\n");
            // avdtp_sink_disconnect(local_cid);
            break;
        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;

    }
}

static const uint8_t media_sbc_codec_info[] = {
    (AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO,
    (AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) | AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS,
    2, 250
}; 


int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    /* Register for HCI events */
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    l2cap_init();
    // Initialize AVDTP Sink
    avdtp_sink_init();
    avdtp_sink_register_packet_handler(&packet_handler);
    uint8_t seid = avdtp_sink_register_stream_end_point(AVDTP_SINK, AVDTP_AUDIO);
    avdtp_sink_register_media_transport_category(seid);

    avdtp_sink_register_media_codec_category(seid, AVDTP_AUDIO, AVDTP_CODEC_SBC, media_sbc_codec_info, sizeof(media_sbc_codec_info));


    // Initialize SDP 
    sdp_init();
    memset(sdp_avdtp_sink_service_buffer, 0, sizeof(sdp_avdtp_sink_service_buffer));
    a2dp_sink_create_sdp_record(sdp_avdtp_sink_service_buffer, 0x10001, 0, NULL, NULL);
    sdp_register_service(sdp_avdtp_sink_service_buffer);

    // turn on!
    hci_power_control(HCI_POWER_ON);

    btstack_stdin_setup(stdin_process);
    return 0;
}
