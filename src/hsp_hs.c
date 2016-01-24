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
// Minimal setup for HSP Headset (!! UNDER DEVELOPMENT !!)
//
// *****************************************************************************

#include "btstack-config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "sdp_query_rfcomm.h"
#include "sdp.h"
#include "debug.h"
#include "hsp_hs.h"


#define HSP_AG_OK "OK"
#define HSP_AG_ERROR "ERROR"
#define HSP_AG_RING "RING"
#define HSP_MICROPHONE_GAIN "+VGM="
#define HSP_SPEAKER_GAIN "+VGS="

#define HSP_HS_AT_CKPD "AT+CKPD=200\r\n"
#define HSP_HS_MICROPHONE_GAIN "AT+VGM"
#define HSP_HS_SPEAKER_GAIN "AT+VGS"

static const char default_hsp_hs_service_name[] = "Headset";

static bd_addr_t remote = {0x04, 0x0C, 0xCE, 0xE4, 0x85, 0xD3};
static uint8_t channel_nr = 0;

static uint16_t mtu;
static uint16_t rfcomm_cid = 0;
static uint16_t sco_handle = 0;
static uint16_t rfcomm_handle = 0;

// static uint8_t connection_state = 0;

static int hs_microphone_gain = -1;
static int hs_speaker_gain = -1;

static uint8_t hs_send_button_press = 0;
static uint8_t hs_support_custom_indications = 0;
static uint8_t hs_outgoing_connection = 0;

typedef enum {
    HSP_IDLE,
    HSP_SDP_QUERY_RFCOMM_CHANNEL,
    HSP_W4_SDP_QUERY_COMPLETE,
    HSP_W4_RFCOMM_CONNECTED,
    HSP_W4_USER_ACTION,
    HSP_W2_CONNECT_SCO,
    HSP_W4_SCO_CONNECTED,
    HSP_ACTIVE,
    HSP_W2_DISCONNECT_SCO,
    HSP_W4_SCO_DISCONNECTED,
    HSP_W2_DISCONNECT_RFCOMM,
    HSP_W4_RFCOMM_DISCONNECTED, 
    HSP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN
} hsp_state_t;

static hsp_state_t hsp_state = HSP_IDLE;

static void hsp_run(void);
static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_query_rfcomm_event(sdp_query_event_t * event, void * context);

static hsp_hs_callback_t hsp_hs_callback;
static void dummy_notify(uint8_t * event, uint16_t size){}

void hsp_hs_register_packet_handler(hsp_hs_callback_t callback){
    if (callback == NULL){
        callback = &dummy_notify;
    }
    hsp_hs_callback = callback;
}

static void emit_event(uint8_t event_subtype, uint8_t value){
    if (!hsp_hs_callback) return;
    uint8_t event[4];
    event[0] = HCI_EVENT_HSP_META;
    event[1] = sizeof(event) - 2;
    event[2] = event_subtype;
    event[3] = value; // status 0 == OK
    (*hsp_hs_callback)(event, sizeof(event));
}

static void emit_event_audio_connected(uint8_t status, uint16_t handle){
    if (!hsp_hs_callback) return;
    uint8_t event[6];
    event[0] = HCI_EVENT_HSP_META;
    event[1] = sizeof(event) - 2;
    event[2] = HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE;
    event[3] = status;
    bt_store_16(event, 4, handle);
    (*hsp_hs_callback)(event, sizeof(event));
}

// remote audio volume control
// AG +VGM=13 [0..15] ; HS AT+VGM=6 | AG OK

static int hsp_hs_send_str_over_rfcomm(uint16_t cid, const char * command){
    if (!rfcomm_can_send_packet_now(rfcomm_cid)) return 1;
    int err = rfcomm_send_internal(cid, (uint8_t*) command, strlen(command));
    if (err){
        printf("rfcomm_send_internal -> error 0X%02x", err);
    }
    return err;
}

void hsp_hs_support_custom_indications(int enable){
    hs_support_custom_indications = enable;
}

// When support custom commands is enabled, AG will send HSP_SUBEVENT_HS_COMMAND.
// On occurance of this event, client's packet handler must send the result back
// by calling hsp_hs_send_result function.
int hsp_hs_send_result(char * result){
    if (!hs_support_custom_indications) return 1;
    return hsp_hs_send_str_over_rfcomm(rfcomm_cid, result);
}


void hsp_hs_create_service(uint8_t * service, int rfcomm_channel_nr, const char * name, uint8_t have_remote_audio_control){
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, SDP_ServiceRecordHandle);
    de_add_number(service, DE_UINT, DE_SIZE_32, 0x10001);

    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ServiceClassIDList);
    attribute = de_push_sequence(service);
    {
        //  see Bluetooth Erratum #3507
        de_add_number(attribute, DE_UUID, DE_SIZE_16, SDP_HSP);          // 0x1108
        de_add_number(attribute, DE_UUID, DE_SIZE_16, SDP_Headset_HS);   // 0x1131
        de_add_number(attribute, DE_UUID, DE_SIZE_16, SDP_GenericAudio); // 0x1203
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_ProtocolDescriptorList);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, SDP_L2CAPProtocol);
        }
        de_pop_sequence(attribute, l2cpProtocol);
        
        uint8_t* rfcomm = de_push_sequence(attribute);
        {
            de_add_number(rfcomm,  DE_UUID, DE_SIZE_16, SDP_RFCOMMProtocol);  // rfcomm_service
            de_add_number(rfcomm,  DE_UINT, DE_SIZE_8,  rfcomm_channel_nr);  // rfcomm channel
        }
        de_pop_sequence(attribute, rfcomm);
    }
    de_pop_sequence(service, attribute);

    // 0x0005 "Public Browse Group"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BrowseGroupList); // public browse group
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute,  DE_UUID, DE_SIZE_16, SDP_PublicBrowseGroup);
    }
    de_pop_sequence(service, attribute);

    // 0x0009 "Bluetooth Profile Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, SDP_BluetoothProfileDescriptorList);
    attribute = de_push_sequence(service);
    {
        uint8_t *hsp_profile = de_push_sequence(attribute);
        {
            de_add_number(hsp_profile,  DE_UUID, DE_SIZE_16, SDP_HSP); 
            de_add_number(hsp_profile,  DE_UINT, DE_SIZE_16, 0x0102); // Verision 1.2
        }
        de_pop_sequence(attribute, hsp_profile);
    }
    de_pop_sequence(service, attribute);

    // 0x0100 "Service Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
    if (name){
        de_add_data(service,  DE_STRING, strlen(name), (uint8_t *) name);
    } else {
        de_add_data(service,  DE_STRING, strlen(default_hsp_hs_service_name), (uint8_t *) default_hsp_hs_service_name);
    }
    
    // Remote audio volume control
    de_add_number(service, DE_UINT, DE_SIZE_16, 0x030C);
    de_add_number(service, DE_BOOL, DE_SIZE_8, have_remote_audio_control);
}

static void hsp_hs_reset_state(void){
    hsp_state = HSP_IDLE;
    
    rfcomm_cid = 0;
    rfcomm_handle = 0;
    sco_handle = 0;

    hs_microphone_gain = -1;
    hs_speaker_gain = -1;
    
    hs_send_button_press = 0;
    hs_support_custom_indications = 0;
}

void hsp_hs_init(uint8_t rfcomm_channel_nr){
    // init L2CAP
    l2cap_init();
    l2cap_register_packet_handler(packet_handler);

    rfcomm_init();
    rfcomm_register_packet_handler(packet_handler);
    rfcomm_register_service_internal(NULL, rfcomm_channel_nr, 0xffff);  // reserved channel, mtu limited by l2cap

    sdp_query_rfcomm_register_callback(handle_query_rfcomm_event, NULL);

    hsp_hs_reset_state();
}


void hsp_hs_connect(bd_addr_t bd_addr){
    if (hsp_state != HSP_IDLE) return;
    hs_outgoing_connection = 1;
    hsp_state = HSP_SDP_QUERY_RFCOMM_CHANNEL;
    memcpy(remote, bd_addr, 6);
    hsp_run();
}

void hsp_hs_disconnect(bd_addr_t bd_addr){
    switch (hsp_state){
        case HSP_ACTIVE:
            printf("HSP_W4_USER_ACTION\n");
            hsp_state = HSP_W4_USER_ACTION;
            hs_send_button_press = 1;
            break;
        case HSP_W4_RFCOMM_CONNECTED:
            printf("HSP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN \n");
            hsp_state = HSP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN;
            break;
        default:
            return;
    }
    hsp_run();
}


void hsp_hs_set_microphone_gain(uint8_t gain){
    if (gain < 0 || gain >15) {
        printf("Gain must be in interval [0..15], it is given %d\n", gain);
        return; 
    }
    hs_microphone_gain = gain;
    hsp_run();
}

// AG +VGS=5  [0..15] ; HS AT+VGM=6 | AG OK
void hsp_hs_set_speaker_gain(uint8_t gain){
    if (gain < 0 || gain >15) {
        printf("Gain must be in interval [0..15], it is given %d\n", gain);
        return; 
    }
    hs_speaker_gain = gain;
    hsp_run();
}  
    

static void hsp_run(void){
    int err;

    if (hs_send_button_press){
        hs_send_button_press = 0;
        err = hsp_hs_send_str_over_rfcomm(rfcomm_cid, HSP_HS_AT_CKPD);
        if (err) {
            hs_send_button_press = 1;
        }
        return;
    }

    switch (hsp_state){
        case HSP_SDP_QUERY_RFCOMM_CHANNEL:
            hsp_state = HSP_W4_SDP_QUERY_COMPLETE;
            sdp_query_rfcomm_channel_and_name_for_uuid(remote, SDP_Headset_AG);
            break;
        
        case HSP_W2_CONNECT_SCO:
            hsp_state = HSP_W4_SCO_CONNECTED;
            break;
        
        case HSP_W2_DISCONNECT_SCO:
            hsp_state = HSP_W4_SCO_DISCONNECTED;
            break;

        case HSP_ACTIVE:

             if (hs_microphone_gain >= 0){
                int gain = hs_microphone_gain;
                hs_microphone_gain = -1;
                char buffer[20];
                sprintf(buffer, "%s=%d\r\n", HSP_HS_MICROPHONE_GAIN, gain);
                err = hsp_hs_send_str_over_rfcomm(rfcomm_cid, buffer);
                if (err) {
                    hs_microphone_gain = gain;
                }
                break;
            }

            if (hs_speaker_gain >= 0){
                int gain = hs_speaker_gain;
                hs_speaker_gain = -1;
                char buffer[20];
                sprintf(buffer, "%s=%d\r\n", HSP_HS_SPEAKER_GAIN, gain);
                err = hsp_hs_send_str_over_rfcomm(rfcomm_cid, buffer);
                if (err) {
                    hs_speaker_gain = gain;
                }
                break;
            }

            break;
        default:
            break;
    }
}


static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    // printf("packet_handler type %u, packet[0] %x\n", packet_type, packet[0]);
    if (packet_type == RFCOMM_DATA_PACKET){
        // skip over leading newline
        while (size > 0 && (packet[0] == '\n' || packet[0] == '\r')){
            size--;
            packet++;
        }
        if (strncmp((char *)packet, HSP_AG_RING, strlen(HSP_AG_RING)) == 0){
            emit_event(HSP_SUBEVENT_RING, 0);
        } else if (strncmp((char *)packet, HSP_AG_OK, strlen(HSP_AG_OK)) == 0){
            printf("OK RECEIVED\n");
            switch (hsp_state){
                case HSP_W4_RFCOMM_CONNECTED:
                    hsp_state = HSP_W2_CONNECT_SCO;
                    break;
                case HSP_W4_USER_ACTION:
                    hsp_state = HSP_W2_DISCONNECT_SCO;
                    break;
                default:
                    break;
            }
        } else if (strncmp((char *)packet, HSP_MICROPHONE_GAIN, strlen(HSP_MICROPHONE_GAIN)) == 0){
            uint8_t gain = (uint8_t)atoi((char*)&packet[strlen(HSP_MICROPHONE_GAIN)]);
            emit_event(HSP_SUBEVENT_MICROPHONE_GAIN_CHANGED, gain);
        
        } else if (strncmp((char *)packet, HSP_SPEAKER_GAIN, strlen(HSP_SPEAKER_GAIN)) == 0){
            uint8_t gain = (uint8_t)atoi((char*)&packet[strlen(HSP_SPEAKER_GAIN)]);
            emit_event(HSP_SUBEVENT_SPEAKER_GAIN_CHANGED, gain);
        } else {
            if (!hsp_hs_callback) return;
            // strip trailing newline
            while (size > 0 && (packet[size-1] == '\n' || packet[size-1] == '\r')){
                size--;
            }
            // add trailing \0
            packet[size] = 0;
            // re-use incoming buffer to avoid reserving large buffers - ugly but efficient
            uint8_t * event = packet - 3;
            event[0] = HCI_EVENT_HSP_META;
            event[1] = size + 1;
            event[2] = HSP_SUBEVENT_AG_INDICATION;
            (*hsp_hs_callback)(event, size+3);
        }
        hsp_run();
        return;
    }

    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = packet[0];
    bd_addr_t event_addr;
    uint16_t handle;

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // bt stack activated, get started 
            if (packet[2] == HCI_STATE_WORKING){
                printf("BTstack activated, get started .\n");
            }
            hsp_hs_callback(packet, size);
            break;

        case HCI_EVENT_PIN_CODE_REQUEST:
            // inform about pin code request
            printf("Pin code request - using '0000'\n\r");
            bt_flip_addr(event_addr, &packet[2]);
            hci_send_cmd(&hci_pin_code_request_reply, &event_addr, 4, "0000");
            break;
        case HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE:{
            int index = 2;
            uint8_t status = packet[index++];
            sco_handle = READ_BT_16(packet, index);
            index+=2;
            bd_addr_t address; 
            memcpy(address, &packet[index], 6);
            index+=6;
            uint8_t link_type = packet[index++];
            uint8_t transmission_interval = packet[index++];  // measured in slots
            uint8_t retransmission_interval = packet[index++];// measured in slots
            uint16_t rx_packet_length = READ_BT_16(packet, index); // measured in bytes
            index+=2;
            uint16_t tx_packet_length = READ_BT_16(packet, index); // measured in bytes
            index+=2;
            uint8_t air_mode = packet[index];

            if (status != 0){
                log_error("(e)SCO Connection failed, status %u", status);
                emit_event_audio_connected(status, sco_handle);
                break;
            }
            switch (link_type){
                case 0x00:
                    printf("SCO Connection established. \n");
                    if (transmission_interval != 0) log_error("SCO Connection: transmission_interval not zero: %d.", transmission_interval);
                    if (retransmission_interval != 0) log_error("SCO Connection: retransmission_interval not zero: %d.", retransmission_interval);
                    if (rx_packet_length != 0) log_error("SCO Connection: rx_packet_length not zero: %d.", rx_packet_length);
                    if (tx_packet_length != 0) log_error("SCO Connection: tx_packet_length not zero: %d.", tx_packet_length);
                    break;
                case 0x02:
                    printf("eSCO Connection established. \n");
                    break;
                default:
                    log_error("(e)SCO reserved link_type 0x%2x", link_type);
                    break;
            }
            log_info("sco_handle 0x%2x, address %s, transmission_interval %u slots, retransmission_interval %u slots, " 
                 " rx_packet_length %u bytes, tx_packet_length %u bytes, air_mode 0x%2x (0x02 == CVSD)", sco_handle,
                 bd_addr_to_str(address), transmission_interval, retransmission_interval, rx_packet_length, tx_packet_length, air_mode);

            if (hsp_state == HSP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN){
                hsp_state = HSP_W2_DISCONNECT_SCO;
                break;
            }

            // forward event to app
            hsp_hs_callback(packet, size);

            hsp_state = HSP_ACTIVE;
            emit_event_audio_connected(0, sco_handle);
            break;                
        }

        case RFCOMM_EVENT_INCOMING_CONNECTION:
            // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
            if (hsp_state != HSP_IDLE) return;

            bt_flip_addr(event_addr, &packet[2]); 
            rfcomm_cid = READ_BT_16(packet, 9);
            printf("RFCOMM channel %u requested for %s\n", packet[8], bd_addr_to_str(event_addr));
            rfcomm_accept_connection_internal(rfcomm_cid);
            
            hsp_state = HSP_W4_RFCOMM_CONNECTED;
            break;

        case RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE:
            printf("RFCOMM_EVENT_OPEN_CHANNEL_COMPLETE packet_handler type %u, packet[0] %x\n", packet_type, packet[0]);
            // data: event(8), len(8), status (8), address (48), handle(16), server channel(8), rfcomm_cid(16), max frame size(16)
            if (packet[2]) {
                printf("RFCOMM channel open failed, status %u\n", packet[2]);
                hsp_hs_reset_state();
                emit_event(HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE, packet[2]);
                hs_outgoing_connection = 0;
            } else {
                // data: event(8) , len(8), status (8), address (48), handle (16), server channel(8), rfcomm_cid(16), max frame size(16)
                rfcomm_handle = READ_BT_16(packet, 9);
                rfcomm_cid = READ_BT_16(packet, 12);
                mtu = READ_BT_16(packet, 14);
                printf("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u\n", rfcomm_cid, mtu);

                if (hs_outgoing_connection){
                    hs_outgoing_connection = 0;
                    hs_send_button_press = 1;
                }

                switch (hsp_state){
                    case HSP_W4_RFCOMM_CONNECTED:
                        hsp_state = HSP_W2_CONNECT_SCO;
                        break;
                    case HSP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN:
                        hsp_state = HSP_W2_DISCONNECT_RFCOMM;
                        break;
                    default:
                        break;
                }
            }
            break;
        case DAEMON_EVENT_HCI_PACKET_SENT:
        case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS:
        case RFCOMM_EVENT_CREDITS:
            hsp_hs_callback(packet, size);
            break;
        
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("HCI_EVENT_DISCONNECTION_COMPLETE \n");
            // if (hsp_state != HSP_W4_SCO_DISCONNECTED){
            //     printf("received gap disconnect in wrong hsp state\n");
            // }
            handle = READ_BT_16(packet,3);
            if (handle == sco_handle){
                sco_handle = 0;
                hsp_state = HSP_W2_DISCONNECT_RFCOMM;
                printf(" HSP_W2_DISCONNECT_RFCOMM\n");
                break;
            }
            break;
        case RFCOMM_EVENT_CHANNEL_CLOSED:
            printf("RFCOMM_EVENT_CHANNEL_CLOSED\n");
            // if (hsp_state != HSP_W4_RFCOMM_DISCONNECTED){
            //     printf("received RFCOMM disconnect in wrong hsp state\n");
            // }
            printf("RFCOMM channel closed\n");
            hsp_hs_reset_state();
            emit_event(HSP_SUBEVENT_AUDIO_DISCONNECTION_COMPLETE,0);
            break;
        default:
            break;
    }
    hsp_run();
}

static void handle_query_rfcomm_event(sdp_query_event_t * event, void * context){
    sdp_query_rfcomm_service_event_t * ve;
    sdp_query_complete_event_t * ce;
            
    switch (event->type){
        case SDP_QUERY_RFCOMM_SERVICE:
            ve = (sdp_query_rfcomm_service_event_t*) event;
            channel_nr = ve->channel_nr;
            printf("** Service name: '%s', RFCOMM port %u\n", ve->service_name, channel_nr);
            break;
        case SDP_QUERY_COMPLETE:
            ce = (sdp_query_complete_event_t*) event;
            
            if (channel_nr > 0){
                hsp_state = HSP_W4_RFCOMM_CONNECTED;
                printf("RFCOMM create channel.\n");
                rfcomm_create_channel_internal(NULL, remote, channel_nr); 
                break;
            }
            hsp_hs_reset_state();
            printf("Service not found, status %u.\n", ce->status);
            exit(0);
            break;
    }
}

void hsp_hs_press_button(void){
    hs_send_button_press = 1;
    hsp_run();
}




