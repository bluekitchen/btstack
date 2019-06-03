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

#define BTSTACK_FILE__ "hsp_ag.c"
 
// *****************************************************************************
//
// HSP Audio Gateway
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_memory.h"
#include "btstack_run_loop.h"
#include "classic/core.h"
#include "classic/sdp_server.h"
#include "classic/sdp_client_rfcomm.h"
#include "classic/sdp_util.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "hsp_ag.h"
#include "l2cap.h"

#define HSP_HS_BUTTON_PRESS "AT+CKPD=200"
#define HSP_HS_AT_CKPD "AT+CKPD\r\n"
#define HSP_AG_OK "\r\nOK\r\n"
#define HSP_AG_ERROR "\r\nERROR\r\n"
#define HSP_AG_RING "\r\nRING\r\n"
#define HSP_MICROPHONE_GAIN "+VGM"
#define HSP_SPEAKER_GAIN "+VGS"

#define HSP_HS_MICROPHONE_GAIN "AT+VGM="
#define HSP_HS_SPEAKER_GAIN "AT+VGS="

static const char default_hsp_ag_service_name[] = "Audio Gateway";

static bd_addr_t remote;
static bd_addr_t sco_event_addr;
static uint8_t channel_nr = 0;

static uint16_t mtu;
static uint16_t rfcomm_cid = 0;
static uint16_t sco_handle = 0;
static uint16_t rfcomm_handle = 0;
static btstack_timer_source_t hs_timeout;

static int ag_microphone_gain = -1;
static int ag_speaker_gain = -1;
static uint8_t ag_ring = 0;
static uint8_t ag_send_ok = 0;
static uint8_t ag_send_error = 0;
static uint8_t ag_num_button_press_received = 0;
static uint8_t ag_support_custom_commands = 0;
static uint8_t ag_establish_sco = 0;
static uint8_t hsp_disconnect_rfcomm = 0;
static uint8_t hsp_establish_audio_connection = 0;
static uint8_t hsp_release_audio_connection = 0;

static btstack_packet_callback_registration_t hci_event_callback_registration;

typedef enum {
    HSP_IDLE,
    HSP_SDP_QUERY_RFCOMM_CHANNEL,
    HSP_W4_SDP_EVENT_QUERY_COMPLETE,
    HSP_W4_RFCOMM_CONNECTED,

    HSP_RFCOMM_CONNECTION_ESTABLISHED,

    HSP_W4_RING_ANSWER,
    HSP_W4_USER_ACTION,
    HSP_W2_CONNECT_SCO,
    HSP_W4_SCO_CONNECTED,
    
    HSP_AUDIO_CONNECTION_ESTABLISHED,

    HSP_W2_DISCONNECT_SCO,
    HSP_W4_SCO_DISCONNECTED,

    HSP_W2_DISCONNECT_RFCOMM,
    HSP_W4_RFCOMM_DISCONNECTED, 
    HSP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN
} hsp_state_t;

static hsp_state_t hsp_state = HSP_IDLE;


static btstack_packet_handler_t hsp_ag_callback;

static void hsp_run(void);
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void dummy_notify(uint8_t packet_type, uint16_t channel, uint8_t * event, uint16_t size){
    UNUSED(packet_type);    // ok: no code
    UNUSED(channel);        // ok: no code
    UNUSED(event);          // ok: no code
    UNUSED(size);           // ok: no code
}

void hsp_ag_register_packet_handler(btstack_packet_handler_t callback){
    if (callback == NULL){
        callback = &dummy_notify;
    }
    hsp_ag_callback = callback;
}

static void emit_event(uint8_t event_subtype, uint8_t value){
    if (!hsp_ag_callback) return;
    uint8_t event[4];
    event[0] = HCI_EVENT_HSP_META;
    event[1] = sizeof(event) - 2;
    event[2] = event_subtype;
    event[3] = value; // status 0 == OK
    (*hsp_ag_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void emit_event_audio_connected(uint8_t status, uint16_t handle){
    if (!hsp_ag_callback) return;
    uint8_t event[6];
    event[0] = HCI_EVENT_HSP_META;
    event[1] = sizeof(event) - 2;
    event[2] = HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE;
    event[3] = status;
    little_endian_store_16(event, 4, handle);
    (*hsp_ag_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

void hsp_ag_create_sdp_record(uint8_t * service, uint32_t service_record_handle, int rfcomm_channel_nr, const char * name){
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
        //  "UUID for PAN Service"
        de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_HEADSET_AUDIO_GATEWAY_AG);
        de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_GENERIC_AUDIO);
    }
    de_pop_sequence(service, attribute);

    // 0x0004 "Protocol Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PROTOCOL_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t* l2cpProtocol = de_push_sequence(attribute);
        {
            de_add_number(l2cpProtocol,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_L2CAP);
        }
        de_pop_sequence(attribute, l2cpProtocol);
        
        uint8_t* rfcomm = de_push_sequence(attribute);
        {
            de_add_number(rfcomm,  DE_UUID, DE_SIZE_16, BLUETOOTH_PROTOCOL_RFCOMM);  // rfcomm_service
            de_add_number(rfcomm,  DE_UINT, DE_SIZE_8,  rfcomm_channel_nr);  // rfcomm channel
        }
        de_pop_sequence(attribute, rfcomm);
    }
    de_pop_sequence(service, attribute);

    // 0x0005 "Public Browse Group"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BROWSE_GROUP_LIST); // public browse group
    attribute = de_push_sequence(service);
    {
        de_add_number(attribute,  DE_UUID, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_PUBLIC_BROWSE_ROOT);
    }
    de_pop_sequence(service, attribute);

    // 0x0009 "Bluetooth Profile Descriptor List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_BLUETOOTH_PROFILE_DESCRIPTOR_LIST);
    attribute = de_push_sequence(service);
    {
        uint8_t *sppProfile = de_push_sequence(attribute);
        {
            de_add_number(sppProfile,  DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_HEADSET); 
            de_add_number(sppProfile,  DE_UINT, DE_SIZE_16, 0x0102); // Verision 1.2
        }
        de_pop_sequence(attribute, sppProfile);
    }
    de_pop_sequence(service, attribute);

    // 0x0100 "Service Name"
    de_add_number(service,  DE_UINT, DE_SIZE_16, 0x0100);
    if (name){
        de_add_data(service,  DE_STRING, strlen(name), (uint8_t *) name);
    } else {
        de_add_data(service,  DE_STRING, strlen(default_hsp_ag_service_name), (uint8_t *) default_hsp_ag_service_name);
    }
}

static int hsp_ag_send_str_over_rfcomm(const uint16_t cid, const char * command){
    int err = rfcomm_send(cid, (uint8_t*) command, strlen(command));
    if (err){
        log_error("rfcomm_send_internal -> error 0X%02x", err);
        return err;
    }
    return err;
}

void hsp_ag_enable_custom_commands(int enable){
    ag_support_custom_commands = enable;
}

int hsp_ag_send_result(char * result){
    if (!ag_support_custom_commands) return 1;
    return hsp_ag_send_str_over_rfcomm(rfcomm_cid, result);
}

static void hsp_ag_reset_state(void){
    hsp_state = HSP_IDLE;
    
    rfcomm_cid = 0;
    rfcomm_handle = 0;
    sco_handle = 0;

    ag_send_ok = 0;
    ag_send_error = 0;
    ag_ring = 0;

    ag_num_button_press_received = 0;
    ag_support_custom_commands = 0;

    ag_microphone_gain = -1;
    ag_speaker_gain = -1;

    hsp_disconnect_rfcomm = 0;
    hsp_establish_audio_connection = 0;
    hsp_release_audio_connection = 0;
}

void hsp_ag_init(uint8_t rfcomm_channel_nr){
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    rfcomm_register_service(packet_handler, rfcomm_channel_nr, 0xffff);  // reserved channel, mtu limited by l2cap

    hsp_ag_reset_state();
}

void hsp_ag_connect(bd_addr_t bd_addr){
    if (hsp_state != HSP_IDLE) return;
    hsp_state = HSP_SDP_QUERY_RFCOMM_CHANNEL;
    memcpy(remote, bd_addr, 6);
    hsp_run();
}

void hsp_ag_disconnect(void){
    hsp_ag_release_audio_connection();
    if (hsp_state < HSP_W4_RFCOMM_CONNECTED){
        hsp_state = HSP_IDLE;
        return;
    }

    if (hsp_state == HSP_W4_RFCOMM_CONNECTED){
        hsp_state = HSP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN;
        return;
    }
    hsp_disconnect_rfcomm = 1;
    hsp_run();
}

void hsp_ag_establish_audio_connection(void){
    switch (hsp_state){
        case HSP_RFCOMM_CONNECTION_ESTABLISHED:
            hsp_establish_audio_connection = 1;
            hsp_state = HSP_W4_SCO_CONNECTED;
            break;
        case HSP_W4_RFCOMM_CONNECTED:
            hsp_state = HSP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN;
            break;
        default:
            break;
    }
    hsp_run();
}

void hsp_ag_release_audio_connection(void){
    if (hsp_state >= HSP_W2_DISCONNECT_SCO) return;
    if (hsp_state < HSP_AUDIO_CONNECTION_ESTABLISHED) return;

    hsp_release_audio_connection = 1;
    hsp_run();
}


void hsp_ag_set_microphone_gain(uint8_t gain){
    if (gain >15) {
        log_error("Gain must be in interval [0..15], it is given %d", gain);
        return; 
    }
    ag_microphone_gain = gain;
    hsp_run();
}

// AG +VGS=5  [0..15] ; HS AT+VGM=6 | AG OK
void hsp_ag_set_speaker_gain(uint8_t gain){
    if (gain >15) {
        log_error("Gain must be in interval [0..15], it is given %d", gain);
        return; 
    }
    ag_speaker_gain = gain;
    hsp_run();
}  

static void hsp_ringing_timeout_handler(btstack_timer_source_t * timer){
    ag_ring = 1;
    btstack_run_loop_set_timer(timer, 2000); // 2 seconds timeout
    btstack_run_loop_add_timer(timer);
    hsp_run();
}

static void hsp_ringing_timer_start(void){
    btstack_run_loop_remove_timer(&hs_timeout);
    btstack_run_loop_set_timer_handler(&hs_timeout, hsp_ringing_timeout_handler);
    btstack_run_loop_set_timer(&hs_timeout, 2000); // 2 seconds timeout
    btstack_run_loop_add_timer(&hs_timeout);
}

static void hsp_ringing_timer_stop(void){
    btstack_run_loop_remove_timer(&hs_timeout);
} 

void hsp_ag_start_ringing(void){
    ag_ring = 1;
    if (hsp_state == HSP_W2_CONNECT_SCO) {
        hsp_state = HSP_W4_RING_ANSWER;
    }
    hsp_ringing_timer_start();
}

void hsp_ag_stop_ringing(void){
    ag_ring = 0;
    if (hsp_state == HSP_W4_RING_ANSWER){
        hsp_state = HSP_W2_CONNECT_SCO;
    }
    hsp_ringing_timer_stop();
}

static void hsp_run(void){
    if (ag_establish_sco && hci_can_send_command_packet_now()){
        ag_establish_sco = 0;
        
        log_info("HSP: sending hci_accept_connection_request.");
        // remote supported feature eSCO is set if link type is eSCO
        // eSCO: S4 - max latency == transmission interval = 0x000c == 12 ms, 
        uint16_t max_latency;
        uint8_t  retransmission_effort;
        uint16_t packet_types;
        
        if (hci_remote_esco_supported(rfcomm_handle)){
            max_latency = 0x000c;
            retransmission_effort = 0x02;
            packet_types = 0x388;
        } else {
            max_latency = 0xffff;
            retransmission_effort = 0xff;
            packet_types = 0x003f;
        }
        
        uint16_t sco_voice_setting = hci_get_sco_voice_setting();
        
        log_info("HFP: sending hci_accept_connection_request, sco_voice_setting %02x", sco_voice_setting);
        hci_send_cmd(&hci_accept_synchronous_connection, remote, 8000, 8000, max_latency, 
                        sco_voice_setting, retransmission_effort, packet_types);
        return;
    }

    if (ag_send_ok){
        if (!rfcomm_can_send_packet_now(rfcomm_cid)) {
            rfcomm_request_can_send_now_event(rfcomm_cid);
            return;
        }
        ag_send_ok = 0;  
        hsp_ag_send_str_over_rfcomm(rfcomm_cid, HSP_AG_OK);
        return;
    }

    if (ag_send_error){
        if (!rfcomm_can_send_packet_now(rfcomm_cid)) {
            rfcomm_request_can_send_now_event(rfcomm_cid);
            return;
        }
        ag_send_error = 0;
        hsp_ag_send_str_over_rfcomm(rfcomm_cid, HSP_AG_ERROR);
        return;
    }

    if (ag_ring){
        if (!rfcomm_can_send_packet_now(rfcomm_cid)) {
            rfcomm_request_can_send_now_event(rfcomm_cid);
            return;
        }
        ag_ring = 0;
        hsp_ag_send_str_over_rfcomm(rfcomm_cid, HSP_AG_RING);
        return;
    }

    if (hsp_establish_audio_connection){
        if (!hci_can_send_command_packet_now()) return;
        hsp_establish_audio_connection = 0;
        hci_send_cmd(&hci_setup_synchronous_connection, rfcomm_handle, 8000, 8000, 0xFFFF, hci_get_sco_voice_setting(), 0xFF, 0x003F);
        return;
    }

    if (hsp_release_audio_connection){
        hsp_release_audio_connection = 0;
        gap_disconnect(sco_handle);
        return;
    }
    
    if (hsp_disconnect_rfcomm){
        hsp_disconnect_rfcomm = 0;
        hsp_establish_audio_connection = 0;
        rfcomm_disconnect(rfcomm_cid);
        return;
    }

    switch (hsp_state){
        case HSP_SDP_QUERY_RFCOMM_CHANNEL:
            hsp_state = HSP_W4_SDP_EVENT_QUERY_COMPLETE;
            log_info("Start SDP query %s, 0x%02x", bd_addr_to_str(remote), BLUETOOTH_SERVICE_CLASS_HEADSET);
            sdp_client_query_rfcomm_channel_and_name_for_uuid(&handle_query_rfcomm_event, remote, BLUETOOTH_SERVICE_CLASS_HEADSET);
            break;

        case HSP_W4_RING_ANSWER:
            if (!ag_num_button_press_received) break;    

            if (!rfcomm_can_send_packet_now(rfcomm_cid)) {
                rfcomm_request_can_send_now_event(rfcomm_cid);
                return;
            }

            ag_send_ok = 0;
            ag_num_button_press_received = 0;
            hsp_state = HSP_W2_CONNECT_SCO;

            hsp_ag_send_str_over_rfcomm(rfcomm_cid, HSP_AG_OK);
            break;
        
        case HSP_W2_CONNECT_SCO:
            if (!hci_can_send_command_packet_now()) return;
            hsp_state = HSP_W4_SCO_CONNECTED;
            hci_send_cmd(&hci_setup_synchronous_connection, rfcomm_handle, 8000, 8000, 0xFFFF, hci_get_sco_voice_setting(), 0xFF, 0x003F);
            break;
        
        case HSP_W2_DISCONNECT_SCO:
            ag_num_button_press_received = 0;
        
            hsp_state = HSP_W4_SCO_DISCONNECTED;
            gap_disconnect(sco_handle);
            break;
        
        case HSP_W2_DISCONNECT_RFCOMM:
            rfcomm_disconnect(rfcomm_cid);
            break;
        
        case HSP_AUDIO_CONNECTION_ESTABLISHED:
        case HSP_RFCOMM_CONNECTION_ESTABLISHED:
            
            if (ag_microphone_gain >= 0){
                if (!rfcomm_can_send_packet_now(rfcomm_cid)) {
                    rfcomm_request_can_send_now_event(rfcomm_cid);
                    return;
                }
                int gain = ag_microphone_gain;
                ag_microphone_gain = -1;
                char buffer[10];
                sprintf(buffer, "%s=%d\r\n", HSP_MICROPHONE_GAIN, gain);
                hsp_ag_send_str_over_rfcomm(rfcomm_cid, buffer);
                break;
            }

            if (ag_speaker_gain >= 0){
                if (!rfcomm_can_send_packet_now(rfcomm_cid)) {
                    rfcomm_request_can_send_now_event(rfcomm_cid);
                    return;
                }
                int gain = ag_speaker_gain;
                ag_speaker_gain = -1;
                char buffer[10];
                sprintf(buffer, "%s=%d\r\n", HSP_SPEAKER_GAIN, gain);
                hsp_ag_send_str_over_rfcomm(rfcomm_cid, buffer);
                break;
            }
            break;
        default:
            break;
    }
}


static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);    // ok: no channel for HCI_EVENT_PACKET and only single active RFCOMM channel

    if (packet_type == RFCOMM_DATA_PACKET){
        while (size > 0 && (packet[0] == '\n' || packet[0] == '\r')){
            size--;
            packet++;
        }

        if (strncmp((char *)packet, HSP_HS_BUTTON_PRESS, strlen(HSP_HS_BUTTON_PRESS)) == 0){
            log_info("Received button press %s", HSP_HS_BUTTON_PRESS);
            ag_send_ok = 1;
            switch (hsp_state){
                case HSP_AUDIO_CONNECTION_ESTABLISHED:
                    hsp_release_audio_connection = 1;
                    break;
                case HSP_RFCOMM_CONNECTION_ESTABLISHED:
                    hsp_establish_audio_connection = 1;
                    break;
                default:
                    break;
            } 
        } else if (strncmp((char *)packet, HSP_HS_MICROPHONE_GAIN, strlen(HSP_HS_MICROPHONE_GAIN)) == 0){
            uint8_t gain = (uint8_t)btstack_atoi((char*)&packet[strlen(HSP_HS_MICROPHONE_GAIN)]);
            ag_send_ok = 1;
            emit_event(HSP_SUBEVENT_MICROPHONE_GAIN_CHANGED, gain);
        
        } else if (strncmp((char *)packet, HSP_HS_SPEAKER_GAIN, strlen(HSP_HS_SPEAKER_GAIN)) == 0){
            uint8_t gain = (uint8_t)btstack_atoi((char*)&packet[strlen(HSP_HS_SPEAKER_GAIN)]);
            ag_send_ok = 1;
            emit_event(HSP_SUBEVENT_SPEAKER_GAIN_CHANGED, gain);

        } else if (strncmp((char *)packet, "AT+", 3) == 0){
            ag_send_error = 1;
            if (!hsp_ag_callback) return;
            // re-use incoming buffer to avoid reserving large buffers - ugly but efficient
            uint8_t * event = packet - 4;
            event[0] = HCI_EVENT_HSP_META;
            event[1] = size + 2;
            event[2] = HSP_SUBEVENT_HS_COMMAND;
            event[3] = size;
            (*hsp_ag_callback)(HCI_EVENT_PACKET, 0, event, size+4);
        }

        hsp_run();
        return;
    }

    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = hci_event_packet_get_type(packet);
    bd_addr_t event_addr;
    uint16_t handle;

    switch (event) {
        case HCI_EVENT_CONNECTION_REQUEST:
            hci_event_connection_request_get_bd_addr(packet, sco_event_addr);
            ag_establish_sco = 1;
            break;
        case HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE:{
            uint8_t status = hci_event_synchronous_connection_complete_get_status(packet);
            if (status != 0){
                log_error("(e)SCO Connection failed, status %u", status);
                emit_event_audio_connected(status, sco_handle);
                break;
            }
            
            hci_event_synchronous_connection_complete_get_bd_addr(packet, event_addr);
            sco_handle = hci_event_synchronous_connection_complete_get_handle(packet);
            uint8_t  link_type = hci_event_synchronous_connection_complete_get_link_type(packet);
            uint8_t  transmission_interval = hci_event_synchronous_connection_complete_get_transmission_interval(packet);  // measured in slots
            uint8_t  retransmission_interval = hci_event_synchronous_connection_complete_get_retransmission_interval(packet);// measured in slots
            uint16_t rx_packet_length = hci_event_synchronous_connection_complete_get_rx_packet_length(packet); // measured in bytes
            uint16_t tx_packet_length = hci_event_synchronous_connection_complete_get_tx_packet_length(packet); // measured in bytes
            uint8_t  air_mode = hci_event_synchronous_connection_complete_get_air_mode(packet);

            switch (link_type){
                case 0x00:
                    log_info("SCO Connection established.");
                    if (transmission_interval != 0) log_error("SCO Connection: transmission_interval not zero: %d.", transmission_interval);
                    if (retransmission_interval != 0) log_error("SCO Connection: retransmission_interval not zero: %d.", retransmission_interval);
                    if (rx_packet_length != 0) log_error("SCO Connection: rx_packet_length not zero: %d.", rx_packet_length);
                    if (tx_packet_length != 0) log_error("SCO Connection: tx_packet_length not zero: %d.", tx_packet_length);
                    break;
                case 0x02:
                    log_info("eSCO Connection established.");
                    break;
                default:
                    log_error("(e)SCO reserved link_type 0x%2x", link_type);
                    break;
            }
            log_info("sco_handle 0x%2x, address %s, transmission_interval %u slots, retransmission_interval %u slots, " 
                 " rx_packet_length %u bytes, tx_packet_length %u bytes, air_mode 0x%2x (0x02 == CVSD)", sco_handle,
                 bd_addr_to_str(event_addr), transmission_interval, retransmission_interval, rx_packet_length, tx_packet_length, air_mode);

            if (hsp_state == HSP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN){
                hsp_state = HSP_W2_DISCONNECT_SCO;
                break;
            }

            hsp_state = HSP_AUDIO_CONNECTION_ESTABLISHED;
            emit_event_audio_connected(status, sco_handle);
            break;                
        }

        case RFCOMM_EVENT_INCOMING_CONNECTION:
            // data: event (8), len(8), address(48), channel (8), rfcomm_cid (16)
            if (hsp_state != HSP_IDLE) return;

            rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);  
            rfcomm_cid = rfcomm_event_incoming_connection_get_server_channel(packet);
            log_info("RFCOMM channel %u requested for %s", packet[8], bd_addr_to_str(event_addr));
            hsp_state = HSP_W4_RFCOMM_CONNECTED;
            rfcomm_accept_connection(rfcomm_cid);
            break;

        case RFCOMM_EVENT_CHANNEL_OPENED:
            log_info("RFCOMM_EVENT_CHANNEL_OPENED packet_handler type %u, packet[0] %x", packet_type, packet[0]);
            // data: event(8), len(8), status (8), address (48), handle(16), server channel(8), rfcomm_cid(16), max frame size(16)
            if (rfcomm_event_channel_opened_get_status(packet)) {
                log_info("RFCOMM channel open failed, status %u§", rfcomm_event_channel_opened_get_status(packet));
                hsp_ag_reset_state();
                hsp_state = HSP_IDLE;
            } else {
                // data: event(8) , len(8), status (8), address (48), handle (16), server channel(8), rfcomm_cid(16), max frame size(16)
                rfcomm_handle = rfcomm_event_channel_opened_get_con_handle(packet);
                rfcomm_cid = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                log_info("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u, state %d", rfcomm_cid, mtu, hsp_state);
                hsp_state = HSP_RFCOMM_CONNECTION_ESTABLISHED;
            }
            emit_event(HSP_SUBEVENT_RFCOMM_CONNECTION_COMPLETE, packet[2]);
            break;
        
        case RFCOMM_EVENT_CHANNEL_CLOSED:
            rfcomm_handle = 0;
            hsp_state = HSP_IDLE;
            hsp_ag_reset_state();
            emit_event(HSP_SUBEVENT_RFCOMM_DISCONNECTION_COMPLETE,0);
            break;

        case RFCOMM_EVENT_CAN_SEND_NOW:
            hsp_run();
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            handle = little_endian_read_16(packet,3);
            if (handle == sco_handle){
                sco_handle = 0;
                hsp_state = HSP_RFCOMM_CONNECTION_ESTABLISHED;
                emit_event(HSP_SUBEVENT_AUDIO_DISCONNECTION_COMPLETE,0);
                break;
            } 
            if (handle == rfcomm_handle) {
                rfcomm_handle = 0;
                hsp_state = HSP_IDLE;
                hsp_ag_reset_state();
                emit_event(HSP_SUBEVENT_RFCOMM_DISCONNECTION_COMPLETE,0);
            }
            break;

        default:
            break;
    }

    hsp_run();
}

static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(packet_type);    // ok: handling own sdp events
    UNUSED(channel);        // ok: no channel
    UNUSED(size);           // ok: handling own sdp events

    switch (packet[0]){
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            channel_nr = sdp_event_query_rfcomm_service_get_rfcomm_channel(packet);
            log_info("** Service name: '%s', RFCOMM port %u", sdp_event_query_rfcomm_service_get_name(packet), channel_nr);
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            if (channel_nr > 0){
                hsp_state = HSP_W4_RFCOMM_CONNECTED;
                log_info("RFCOMM create channel. state %d", HSP_W4_RFCOMM_CONNECTED);
                rfcomm_create_channel(packet_handler, remote, channel_nr, NULL); 
                break;
            }
            hsp_ag_reset_state();
            log_info("Service not found, status %u.\n", sdp_event_query_complete_get_status(packet));
            if (sdp_event_query_complete_get_status(packet)){
                emit_event(HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE, sdp_event_query_complete_get_status(packet));
            } else {
                emit_event(HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE, SDP_SERVICE_NOT_FOUND);
            }
            break;
        default:
            break;
    }
}


