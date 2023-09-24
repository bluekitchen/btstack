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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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
#include "classic/sdp_client.h"
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

static const char hsp_ag_default_service_name[] = "Audio Gateway";

typedef enum {
    HSP_IDLE,
    HSP_W2_SEND_SDP_QUERY,
    HSP_W4_SDP_QUERY_COMPLETE,
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

// higher-layer callbacks
static btstack_packet_handler_t hsp_ag_callback;

static hsp_state_t hsp_ag_state;

static int      hsp_ag_microphone_gain;
static int      hsp_ag_speaker_gain;
static uint8_t  hsp_ag_ring;
static uint8_t  hsp_ag_send_ok;
static uint8_t  hsp_ag_send_error;
static uint8_t  hsp_ag_support_custom_commands;
static uint8_t  hsp_ag_disconnect_rfcomm;
static uint8_t  hsp_ag_release_audio_connection_triggered;
static uint16_t hsp_ag_sco_packet_types;

static bd_addr_t hsp_ag_remote;
static uint8_t   hsp_ag_channel_nr;
static uint16_t  hsp_ag_rfcomm_cid;
static uint16_t  hsp_ag_mtu;

static hci_con_handle_t hsp_ag_sco_handle;
static hci_con_handle_t hsp_ag_rfcomm_handle;

static btstack_timer_source_t hsp_ag_timeout;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_context_callback_registration_t hsp_ag_handle_sdp_client_query_request;

#ifdef ENABLE_RTK_PCM_WBS
static bool hsp_ag_rtk_send_sco_config;
#endif

static void hsp_run(void);
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

void hsp_ag_register_packet_handler(btstack_packet_handler_t callback){
    btstack_assert(callback != NULL);
    hsp_ag_callback = callback;
}

static void emit_event(uint8_t event_subtype){
    if (!hsp_ag_callback) return;
    uint8_t event[5];
    event[0] = HCI_EVENT_HSP_META;
    event[1] = sizeof(event) - 2;
    event[2] = event_subtype;
    little_endian_store_16(event, 3, hsp_ag_rfcomm_handle);
    (*hsp_ag_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void emit_event_with_value(uint8_t event_subtype, uint8_t value){
    if (!hsp_ag_callback) return;
    uint8_t event[6];
    event[0] = HCI_EVENT_HSP_META;
    event[1] = sizeof(event) - 2;
    event[2] = event_subtype;
    little_endian_store_16(event, 3, hsp_ag_rfcomm_handle);
    event[5] = value;
    (*hsp_ag_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void emit_event_rfcomm_connected(uint8_t status){
    if (!hsp_ag_callback) return;
    uint8_t event[12];
    event[0] = HCI_EVENT_HSP_META;
    event[1] = sizeof(event) - 2;
    event[2] = HSP_SUBEVENT_RFCOMM_CONNECTION_COMPLETE;
    little_endian_store_16(event, 3, hsp_ag_rfcomm_handle);
    event[5] = status;
    reverse_bd_addr(hsp_ag_remote, &event[6]);
    (*hsp_ag_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void emit_event_audio_connected(uint8_t status, uint16_t sco_handle){
    if (!hsp_ag_callback) return;
    uint8_t event[8];
    event[0] = HCI_EVENT_HSP_META;
    event[1] = sizeof(event) - 2;
    event[2] = HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE;
    little_endian_store_16(event, 3, hsp_ag_rfcomm_handle);
    event[5] = status;
    little_endian_store_16(event, 6, sco_handle);
    (*hsp_ag_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void emit_event_audio_disconnected(uint16_t sco_handle){
    if (!hsp_ag_callback) return;
    uint8_t event[7];
    event[0] = HCI_EVENT_HSP_META;
    event[1] = sizeof(event) - 2;
    event[2] = HSP_SUBEVENT_AUDIO_DISCONNECTION_COMPLETE;
    little_endian_store_16(event, 3, hsp_ag_rfcomm_handle);
    little_endian_store_16(event, 5, sco_handle);
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
        de_add_data(service,  DE_STRING, (uint16_t) strlen(name), (uint8_t *) name);
    } else {
        de_add_data(service, DE_STRING, (uint16_t) strlen(hsp_ag_default_service_name), (uint8_t *) hsp_ag_default_service_name);
    }
}

static int hsp_ag_send_str_over_rfcomm(const uint16_t cid, const char * command){
    int err = rfcomm_send(cid, (uint8_t*) command, (uint16_t) strlen(command));
    if (err){
        log_error("rfcomm_send_internal -> error 0X%02x", err);
        return err;
    }
    return err;
}

void hsp_ag_enable_custom_commands(int enable){
    hsp_ag_support_custom_commands = enable;
}

int hsp_ag_send_result(char * result){
    if (!hsp_ag_support_custom_commands) return 1;
    return hsp_ag_send_str_over_rfcomm(hsp_ag_rfcomm_cid, result);
}

static void hsp_ag_reset_state(void){
    hsp_ag_state = HSP_IDLE;

    hsp_ag_rfcomm_cid = 0;
    hsp_ag_rfcomm_handle = HCI_CON_HANDLE_INVALID;
    hsp_ag_channel_nr = 0;
    hsp_ag_sco_handle = HCI_CON_HANDLE_INVALID;

    hsp_ag_send_ok = 0;
    hsp_ag_send_error = 0;
    hsp_ag_ring = 0;

    hsp_ag_support_custom_commands = 0;

    hsp_ag_microphone_gain = -1;
    hsp_ag_speaker_gain = -1;

    hsp_ag_disconnect_rfcomm = 0;
    hsp_ag_release_audio_connection_triggered = 0;
}

void hsp_ag_init(uint8_t rfcomm_channel_nr){
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    rfcomm_register_service(packet_handler, rfcomm_channel_nr, 0xffff);  // reserved channel, mtu limited by l2cap

    hsp_ag_sco_packet_types = SCO_PACKET_TYPES_EV3 | SCO_PACKET_TYPES_2EV3 | SCO_PACKET_TYPES_HV3;
    hsp_ag_reset_state();
}

void hsp_ag_deinit(void){
    hsp_ag_reset_state();
    hsp_ag_callback = NULL;
    (void)memset(hsp_ag_remote, 0, 6);
    (void)memset(&hsp_ag_timeout, 0, sizeof(btstack_timer_source_t));
    (void)memset(&hci_event_callback_registration, 0, sizeof(btstack_packet_callback_registration_t));
    (void)memset(&hsp_ag_handle_sdp_client_query_request, 0, sizeof(btstack_context_callback_registration_t));
}

static void hsp_ag_handle_start_sdp_client_query(void * context){
    UNUSED(context);
    if (hsp_ag_state != HSP_W2_SEND_SDP_QUERY) return;

    hsp_ag_state = HSP_W4_SDP_QUERY_COMPLETE;
    log_info("Start SDP query %s, 0x%02x", bd_addr_to_str(hsp_ag_remote), BLUETOOTH_SERVICE_CLASS_HEADSET);
    sdp_client_query_rfcomm_channel_and_name_for_service_class_uuid(&handle_query_rfcomm_event, hsp_ag_remote, BLUETOOTH_SERVICE_CLASS_HEADSET);
}

void hsp_ag_connect(bd_addr_t bd_addr){
    if (hsp_ag_state != HSP_IDLE) return;
    
    (void)memcpy(hsp_ag_remote, bd_addr, 6);
    hsp_ag_state = HSP_W2_SEND_SDP_QUERY;
    hsp_ag_handle_sdp_client_query_request.callback = &hsp_ag_handle_start_sdp_client_query;
    // ignore ERROR_CODE_COMMAND_DISALLOWED because in that case, we already have requested an SDP callback
    (void) sdp_client_register_query_callback(&hsp_ag_handle_sdp_client_query_request);
}

void hsp_ag_disconnect(void){
    hsp_ag_release_audio_connection();
    if (hsp_ag_state < HSP_W4_RFCOMM_CONNECTED){
        hsp_ag_state = HSP_IDLE;
        return;
    }

    if (hsp_ag_state == HSP_W4_RFCOMM_CONNECTED){
        hsp_ag_state = HSP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN;
        return;
    }
    hsp_ag_disconnect_rfcomm = 1;
    hsp_run();
}

void hsp_ag_establish_audio_connection(void){
    switch (hsp_ag_state){
        case HSP_RFCOMM_CONNECTION_ESTABLISHED:
            hsp_ag_state = HSP_W2_CONNECT_SCO;
            break;
        case HSP_W4_RFCOMM_CONNECTED:
            hsp_ag_state = HSP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN;
            break;
        default:
            break;
    }
    hsp_run();
}

void hsp_ag_release_audio_connection(void){
    if (hsp_ag_state >= HSP_W2_DISCONNECT_SCO) return;
    if (hsp_ag_state < HSP_AUDIO_CONNECTION_ESTABLISHED) return;

    hsp_ag_release_audio_connection_triggered = 1;
    hsp_run();
}


void hsp_ag_set_microphone_gain(uint8_t gain){
    if (gain >15) {
        log_error("Gain must be in interval [0..15], it is given %d", gain);
        return; 
    }
    hsp_ag_microphone_gain = gain;
    hsp_run();
}

// AG +VGS=5  [0..15] ; HS AT+VGM=6 | AG OK
void hsp_ag_set_speaker_gain(uint8_t gain){
    if (gain >15) {
        log_error("Gain must be in interval [0..15], it is given %d", gain);
        return; 
    }
    hsp_ag_speaker_gain = gain;
    hsp_run();
}  

static void hsp_ringing_timeout_handler(btstack_timer_source_t * timer){
    hsp_ag_ring = 1;
    btstack_run_loop_set_timer(timer, 2000); // 2 seconds timeout
    btstack_run_loop_add_timer(timer);
    hsp_run();
}

static void hsp_ringing_timer_start(void){
    btstack_run_loop_remove_timer(&hsp_ag_timeout);
    btstack_run_loop_set_timer_handler(&hsp_ag_timeout, hsp_ringing_timeout_handler);
    btstack_run_loop_set_timer(&hsp_ag_timeout, 2000); // 2 seconds timeout
    btstack_run_loop_add_timer(&hsp_ag_timeout);
}

static void hsp_ringing_timer_stop(void){
    btstack_run_loop_remove_timer(&hsp_ag_timeout);
} 

void hsp_ag_start_ringing(void){
    hsp_ag_ring = 1;
    if (hsp_ag_state == HSP_W2_CONNECT_SCO) {
        hsp_ag_state = HSP_W4_RING_ANSWER;
    }
    hsp_ringing_timer_start();
}

void hsp_ag_stop_ringing(void){
    hsp_ag_ring = 0;
    if (hsp_ag_state == HSP_W4_RING_ANSWER){
        hsp_ag_state = HSP_W2_CONNECT_SCO;
    }
    hsp_ringing_timer_stop();
}

static void hsp_run(void){
    uint16_t packet_types;

    if (hsp_ag_send_ok){
        if (!rfcomm_can_send_packet_now(hsp_ag_rfcomm_cid)) {
            rfcomm_request_can_send_now_event(hsp_ag_rfcomm_cid);
            return;
        }
        hsp_ag_send_ok = 0;
        hsp_ag_send_str_over_rfcomm(hsp_ag_rfcomm_cid, HSP_AG_OK);
        return;
    }

    if (hsp_ag_send_error){
        if (!rfcomm_can_send_packet_now(hsp_ag_rfcomm_cid)) {
            rfcomm_request_can_send_now_event(hsp_ag_rfcomm_cid);
            return;
        }
        hsp_ag_send_error = 0;
        hsp_ag_send_str_over_rfcomm(hsp_ag_rfcomm_cid, HSP_AG_ERROR);
        return;
    }

    if (hsp_ag_ring){
        if (!rfcomm_can_send_packet_now(hsp_ag_rfcomm_cid)) {
            rfcomm_request_can_send_now_event(hsp_ag_rfcomm_cid);
            return;
        }
        hsp_ag_ring = 0;
        hsp_ag_send_str_over_rfcomm(hsp_ag_rfcomm_cid, HSP_AG_RING);
        return;
    }

    if (hsp_ag_release_audio_connection_triggered){
        hsp_ag_release_audio_connection_triggered = 0;
        gap_disconnect(hsp_ag_sco_handle);
        return;
    }
    
    if (hsp_ag_disconnect_rfcomm){
        hsp_ag_disconnect_rfcomm = 0;
        rfcomm_disconnect(hsp_ag_rfcomm_cid);
        return;
    }

#ifdef ENABLE_RTK_PCM_WBS
    if (hsp_ag_rtk_send_sco_config){
        hsp_ag_rtk_send_sco_config = false;
        log_info("RTK SCO: 16k + CVSD");
        hci_send_cmd(&hci_rtk_configure_sco_routing, 0x81, 0x90, 0x00, 0x00, 0x1a, 0x0c, 0x0c, 0x00, 0x01);
        return;
    }
#endif

    switch (hsp_ag_state){

        case HSP_W4_RING_ANSWER:
            if (!rfcomm_can_send_packet_now(hsp_ag_rfcomm_cid)) {
                rfcomm_request_can_send_now_event(hsp_ag_rfcomm_cid);
                return;
            }

            hsp_ag_send_ok = 0;
            hsp_ag_state = HSP_W2_CONNECT_SCO;

            hsp_ag_send_str_over_rfcomm(hsp_ag_rfcomm_cid, HSP_AG_OK);
            break;
        
        case HSP_W2_CONNECT_SCO:
            if (!hci_can_send_command_packet_now()) return;
            hsp_ag_state = HSP_W4_SCO_CONNECTED;
            // bits 6-9 are 'don't use'
            packet_types = hsp_ag_sco_packet_types ^ 0x3c0;
            hci_send_cmd(&hci_setup_synchronous_connection, hsp_ag_rfcomm_handle, 8000, 8000, 0xFFFF, hci_get_sco_voice_setting(), 0xFF, packet_types);
            break;
        
        case HSP_W2_DISCONNECT_SCO:
            hsp_ag_state = HSP_W4_SCO_DISCONNECTED;
            gap_disconnect(hsp_ag_sco_handle);
            break;
        
        case HSP_W2_DISCONNECT_RFCOMM:
            rfcomm_disconnect(hsp_ag_rfcomm_cid);
            break;
        
        case HSP_AUDIO_CONNECTION_ESTABLISHED:
        case HSP_RFCOMM_CONNECTION_ESTABLISHED:
            
            if (hsp_ag_microphone_gain >= 0){
                if (!rfcomm_can_send_packet_now(hsp_ag_rfcomm_cid)) {
                    rfcomm_request_can_send_now_event(hsp_ag_rfcomm_cid);
                    return;
                }
                int gain = hsp_ag_microphone_gain;
                hsp_ag_microphone_gain = -1;
                char buffer[12];
                snprintf(buffer, sizeof(buffer), "\r\n%s=%d\r\n",
                         HSP_MICROPHONE_GAIN, gain);
                buffer[sizeof(buffer) - 1] = 0;
                hsp_ag_send_str_over_rfcomm(hsp_ag_rfcomm_cid, buffer);
                break;
            }

            if (hsp_ag_speaker_gain >= 0){
                if (!rfcomm_can_send_packet_now(hsp_ag_rfcomm_cid)) {
                    rfcomm_request_can_send_now_event(hsp_ag_rfcomm_cid);
                    return;
                }
                int gain = hsp_ag_speaker_gain;
                hsp_ag_speaker_gain = -1;
                char buffer[12];
                snprintf(buffer, sizeof(buffer), "\r\n%s=%d\r\n",
                         HSP_SPEAKER_GAIN, gain);
                buffer[sizeof(buffer) - 1] = 0;
                hsp_ag_send_str_over_rfcomm(hsp_ag_rfcomm_cid, buffer);
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
        while ((size > 0) && ((packet[0] == '\n') || (packet[0] == '\r'))){
            size--;
            packet++;
        }

        if (strncmp((char *)packet, HSP_HS_BUTTON_PRESS, strlen(HSP_HS_BUTTON_PRESS)) == 0){
            log_info("Received button press %s", HSP_HS_BUTTON_PRESS);
            hsp_ag_send_ok = 1;
            emit_event(HSP_SUBEVENT_BUTTON_PRESSED);
        } else if (strncmp((char *)packet, HSP_HS_MICROPHONE_GAIN, strlen(HSP_HS_MICROPHONE_GAIN)) == 0){
            uint8_t gain = (uint8_t)btstack_atoi((char*)&packet[strlen(HSP_HS_MICROPHONE_GAIN)]);
            hsp_ag_send_ok = 1;
            emit_event_with_value(HSP_SUBEVENT_MICROPHONE_GAIN_CHANGED, gain);
        
        } else if (strncmp((char *)packet, HSP_HS_SPEAKER_GAIN, strlen(HSP_HS_SPEAKER_GAIN)) == 0){
            uint8_t gain = (uint8_t)btstack_atoi((char*)&packet[strlen(HSP_HS_SPEAKER_GAIN)]);
            hsp_ag_send_ok = 1;
            emit_event_with_value(HSP_SUBEVENT_SPEAKER_GAIN_CHANGED, gain);

        } else if (strncmp((char *)packet, "AT+", 3) == 0){
            hsp_ag_send_error = 1;
            if (!hsp_ag_callback) return;
			if ((size + 4) > 255) return;
            // re-use incoming buffer to avoid reserving buffers/memcpy - ugly but efficient
			uint8_t * event = packet - 6;
            event[0] = HCI_EVENT_HSP_META;
            event[1] = (uint8_t) (size + 4);
            event[2] = HSP_SUBEVENT_HS_COMMAND;
            little_endian_store_16(event, 3, hsp_ag_rfcomm_handle);
            event[5] = (uint8_t) size;
            (*hsp_ag_callback)(HCI_EVENT_PACKET, 0, event, size+6);
        }

        hsp_run();
        return;
    }

    if (packet_type != HCI_EVENT_PACKET) return;
    uint8_t event = hci_event_packet_get_type(packet);
    bd_addr_t event_addr;
    uint16_t handle;
    uint8_t status;

    switch (event) {
        case HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE:{
            if (hsp_ag_state != HSP_W4_SCO_CONNECTED) break;
            status = hci_event_synchronous_connection_complete_get_status(packet);
            if (status != 0){
                hsp_ag_state = HSP_RFCOMM_CONNECTION_ESTABLISHED;
                log_error("(e)SCO Connection failed, status %u", status);
                emit_event_audio_connected(status, hsp_ag_sco_handle);
                break;
            }
            
            hci_event_synchronous_connection_complete_get_bd_addr(packet, event_addr);
            hsp_ag_sco_handle = hci_event_synchronous_connection_complete_get_handle(packet);
            uint8_t  link_type = hci_event_synchronous_connection_complete_get_link_type(packet);
            uint8_t  transmission_interval = hci_event_synchronous_connection_complete_get_transmission_interval(packet);  // measured in slots
            uint8_t  retransmission_interval = hci_event_synchronous_connection_complete_get_retransmission_interval(packet);// measured in slots
            uint16_t rx_packet_length = hci_event_synchronous_connection_complete_get_rx_packet_length(packet); // measured in bytes
            uint16_t tx_packet_length = hci_event_synchronous_connection_complete_get_tx_packet_length(packet); // measured in bytes

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
                 " rx_packet_length %u bytes, tx_packet_length %u bytes, air_mode 0x%2x (0x02 == CVSD)", hsp_ag_sco_handle,
                     bd_addr_to_str(event_addr), transmission_interval, retransmission_interval, rx_packet_length, tx_packet_length,
                     hci_event_synchronous_connection_complete_get_air_mode(packet));

            if (hsp_ag_state == HSP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN){
                hsp_ag_state = HSP_W2_DISCONNECT_SCO;
                break;
            }

            hsp_ag_state = HSP_AUDIO_CONNECTION_ESTABLISHED;
            emit_event_audio_connected(status, hsp_ag_sco_handle);
            break;                
        }

        case RFCOMM_EVENT_INCOMING_CONNECTION:
            if (hsp_ag_state != HSP_IDLE) return;

            rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);
            hsp_ag_rfcomm_cid = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
            log_info("RFCOMM channel %u requested for %s with cid 0x%04x", rfcomm_event_incoming_connection_get_server_channel(packet), bd_addr_to_str(event_addr), hsp_ag_rfcomm_cid);
            hsp_ag_state = HSP_W4_RFCOMM_CONNECTED;
            rfcomm_accept_connection(hsp_ag_rfcomm_cid);
            break;

        case RFCOMM_EVENT_CHANNEL_OPENED:
            log_info("RFCOMM_EVENT_CHANNEL_OPENED packet_handler type %u, packet[0] %x", packet_type, packet[0]);
            status = rfcomm_event_channel_opened_get_status(packet);
            if (status != ERROR_CODE_SUCCESS){
                log_info("RFCOMM channel open failed, status %u", status);
                hsp_ag_reset_state();
                hsp_ag_state = HSP_IDLE;
                memset(hsp_ag_remote, 0, 6);
            } else {
                hsp_ag_rfcomm_handle = rfcomm_event_channel_opened_get_con_handle(packet);
                hsp_ag_rfcomm_cid = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                rfcomm_event_channel_opened_get_bd_addr(packet, hsp_ag_remote);
                hsp_ag_mtu = rfcomm_event_channel_opened_get_max_frame_size(packet);
                log_info("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u, state %d", hsp_ag_rfcomm_cid, hsp_ag_mtu, hsp_ag_state);
                hsp_ag_state = HSP_RFCOMM_CONNECTION_ESTABLISHED;
#ifdef ENABLE_RTK_PCM_WBS
                hsp_ag_rtk_send_sco_config = true;
#endif
            }
            emit_event_rfcomm_connected(status);
            break;
        
        case RFCOMM_EVENT_CHANNEL_CLOSED:
            hsp_ag_reset_state();
            emit_event(HSP_SUBEVENT_RFCOMM_DISCONNECTION_COMPLETE);
            break;

        case RFCOMM_EVENT_CAN_SEND_NOW:
            hsp_run();
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            handle = little_endian_read_16(packet,3);
            if (handle == hsp_ag_sco_handle){
                hci_con_handle_t  sco_handle = hsp_ag_sco_handle;
                hsp_ag_sco_handle = HCI_CON_HANDLE_INVALID;
                hsp_ag_state = HSP_RFCOMM_CONNECTION_ESTABLISHED;
                emit_event_audio_disconnected(sco_handle);
                break;
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
            hsp_ag_channel_nr = sdp_event_query_rfcomm_service_get_rfcomm_channel(packet);
            log_info("** Service name: '%s', RFCOMM port %u", sdp_event_query_rfcomm_service_get_name(packet), hsp_ag_channel_nr);
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            if (hsp_ag_channel_nr > 0){
                hsp_ag_state = HSP_W4_RFCOMM_CONNECTED;
                log_info("RFCOMM create channel. state %d", HSP_W4_RFCOMM_CONNECTED);
                rfcomm_create_channel(packet_handler, hsp_ag_remote, hsp_ag_channel_nr, NULL);
                break;
            }
            hsp_ag_reset_state();
            log_info("Service not found, status %u.\n", sdp_event_query_complete_get_status(packet));
            if (sdp_event_query_complete_get_status(packet)){
                emit_event_with_value(HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE, sdp_event_query_complete_get_status(packet));
            } else {
                emit_event_with_value(HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE, SDP_SERVICE_NOT_FOUND);
            }
            break;
        default:
            break;
    }
}

void hsp_ag_set_sco_packet_types(uint16_t packet_types){
    hsp_ag_sco_packet_types = packet_types;
}

