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

#define BTSTACK_FILE__ "hsp_hs.c"
 
// *****************************************************************************
//
// HSP Headset
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
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
#include "hsp_hs.h"
#include "l2cap.h"

#define HSP_AG_OK "OK"
#define HSP_AG_ERROR "ERROR"
#define HSP_AG_RING "RING"
#define HSP_MICROPHONE_GAIN "+VGM="
#define HSP_SPEAKER_GAIN "+VGS="

#define HSP_HS_AT_CKPD "AT+CKPD=200\r\n"
#define HSP_HS_MICROPHONE_GAIN "AT+VGM"
#define HSP_HS_SPEAKER_GAIN "AT+VGS"

static const char default_hsp_hs_service_name[] = "Headset";

static bd_addr_t remote;
static uint8_t channel_nr = 0;

static uint16_t mtu;
static uint16_t rfcomm_cid = 0;
static hci_con_handle_t sco_handle;
static hci_con_handle_t rfcomm_handle;

static int hs_microphone_gain;
static int hs_speaker_gain;

static uint8_t hs_send_button_press;
static uint8_t wait_ok;
static uint8_t hs_accept_sco_connection;

static uint8_t hs_support_custom_indications;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static uint8_t hsp_establish_audio_connection;
static uint8_t hsp_release_audio_connection;

static uint16_t hsp_hs_sco_packet_types;

typedef enum {
    HSP_IDLE,
    HSP_W2_SEND_SDP_QUERY,
    HSP_W4_SDP_QUERY_COMPLETE,
    HSP_W4_RFCOMM_CONNECTED,
    
    HSP_RFCOMM_CONNECTION_ESTABLISHED,
    
    HSP_W2_CONNECT_SCO,
    HSP_W4_SCO_CONNECTED,
    
    HSP_AUDIO_CONNECTION_ESTABLISHED, 
    
    HSP_W2_DISCONNECT_SCO,
    HSP_W4_SCO_DISCONNECTED, 

    HSP_W2_DISCONNECT_RFCOMM,
    HSP_W4_RFCOMM_DISCONNECTED,  
    HSP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN
} hsp_state_t;

static btstack_context_callback_registration_t hsp_hs_handle_sdp_client_query_request;

static hsp_state_t hsp_state;

static void hsp_run(void);
static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_query_rfcomm_event(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static btstack_packet_handler_t hsp_hs_callback;
static void dummy_notify(uint8_t packet_type, uint16_t channel, uint8_t * event, uint16_t size){
    // ok: no code
    UNUSED(packet_type);    
    UNUSED(channel);    
    UNUSED(event);    
    UNUSED(size);    
}

void hsp_hs_register_packet_handler(btstack_packet_handler_t callback){
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
    (*hsp_hs_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void emit_ring_event(void){
    if (!hsp_hs_callback) return;
    uint8_t event[3];
    event[0] = HCI_EVENT_HSP_META;
    event[1] = sizeof(event) - 2;
    event[2] = HSP_SUBEVENT_RING;
    (*hsp_hs_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void emit_event_audio_connected(uint8_t status, uint16_t handle){
    if (!hsp_hs_callback) return;
    uint8_t event[6];
    event[0] = HCI_EVENT_HSP_META;
    event[1] = sizeof(event) - 2;
    event[2] = HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE;
    event[3] = status;
    little_endian_store_16(event, 4, handle);
    (*hsp_hs_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

// remote audio volume control
// AG +VGM=13 [0..15] ; HS AT+VGM=6 | AG OK

static int hsp_hs_send_str_over_rfcomm(uint16_t cid, const char * command){
    if (!rfcomm_can_send_packet_now(rfcomm_cid)) return 1;
    int err = rfcomm_send(cid, (uint8_t*) command, strlen(command));
    if (err){
        log_info("rfcomm_send_internal -> error 0X%02x", err);
    }
    return err;
}

void hsp_hs_enable_custom_indications(int enable){
    hs_support_custom_indications = enable;
}

int hsp_hs_send_result(const char * result){
    if (!hs_support_custom_indications) return 1;
    return hsp_hs_send_str_over_rfcomm(rfcomm_cid, result);
}


void hsp_hs_create_sdp_record(uint8_t * service,  uint32_t service_record_handle, int rfcomm_channel_nr, const char * name, uint8_t have_remote_audio_control){
    uint8_t* attribute;
    de_create_sequence(service);

    // 0x0000 "Service Record Handle"
    de_add_number(service, DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_RECORD_HANDLE);
    de_add_number(service, DE_UINT, DE_SIZE_32, service_record_handle);

    // 0x0001 "Service Class ID List"
    de_add_number(service,  DE_UINT, DE_SIZE_16, BLUETOOTH_ATTRIBUTE_SERVICE_CLASS_ID_LIST);
    attribute = de_push_sequence(service);
    {
        //  see Bluetooth Erratum #3507
        de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_HEADSET);       // 0x1108
        de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_HEADSET_HS);    // 0x1131
        de_add_number(attribute, DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_GENERIC_AUDIO); // 0x1203
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
        uint8_t *hsp_profile = de_push_sequence(attribute);
        {
            de_add_number(hsp_profile,  DE_UUID, DE_SIZE_16, BLUETOOTH_SERVICE_CLASS_HEADSET); 
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
    hs_microphone_gain = -1;
    hs_speaker_gain = -1;
    rfcomm_cid = 0;
    channel_nr = 0;
    sco_handle    = HCI_CON_HANDLE_INVALID;
    rfcomm_handle = HCI_CON_HANDLE_INVALID;

    hs_send_button_press = 0;
    wait_ok = 0;
    hs_support_custom_indications = 0;

    hs_accept_sco_connection = 0;
    hsp_establish_audio_connection = 0;
    hsp_release_audio_connection = 0;
}

void hsp_hs_init(uint8_t rfcomm_channel_nr){
    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    rfcomm_register_service(packet_handler, rfcomm_channel_nr, 0xffff);  // reserved channel, mtu limited by l2cap

    hsp_hs_sco_packet_types = SCO_PACKET_TYPES_ALL;
    hsp_hs_reset_state();
}

void hsp_hs_deinit(void){
    (void)memset(remote, 0, 6);
    (void)memset(&hci_event_callback_registration, 0, sizeof(btstack_packet_callback_registration_t));
    (void)memset(&hsp_hs_handle_sdp_client_query_request, 0, sizeof(btstack_context_callback_registration_t));
    hsp_hs_callback = NULL;
}

static void hsp_hs_handle_start_sdp_client_query(void * context){
    UNUSED(context);
    if (hsp_state != HSP_W2_SEND_SDP_QUERY) return;
    
    hsp_state = HSP_W4_SDP_QUERY_COMPLETE;
    log_info("Start SDP query %s, 0x%02x", bd_addr_to_str(remote), BLUETOOTH_SERVICE_CLASS_HEADSET_AUDIO_GATEWAY_AG);
    sdp_client_query_rfcomm_channel_and_name_for_service_class_uuid(&handle_query_rfcomm_event, remote, BLUETOOTH_SERVICE_CLASS_HEADSET_AUDIO_GATEWAY_AG);
}

void hsp_hs_connect(bd_addr_t bd_addr){
    if (hsp_state != HSP_IDLE) return;

    (void)memcpy(remote, bd_addr, 6);
    hsp_state = HSP_W2_SEND_SDP_QUERY;
    hsp_hs_handle_sdp_client_query_request.callback = &hsp_hs_handle_start_sdp_client_query;
    // ignore ERROR_CODE_COMMAND_DISALLOWED because in that case, we already have requested an SDP callback
    (void) sdp_client_register_query_callback(&hsp_hs_handle_sdp_client_query_request);
}

void hsp_hs_disconnect(void){
    hsp_hs_release_audio_connection();

    if (hsp_state < HSP_W4_RFCOMM_CONNECTED){
        hsp_state = HSP_IDLE;
        return;
    }

    if (hsp_state == HSP_W4_RFCOMM_CONNECTED){
        hsp_state = HSP_W4_CONNECTION_ESTABLISHED_TO_SHUTDOWN;
        return;
    }

    hsp_establish_audio_connection = 0;
    rfcomm_disconnect(rfcomm_cid);
}


void hsp_hs_establish_audio_connection(void){
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

void hsp_hs_release_audio_connection(void){
    if (hsp_state >= HSP_W2_DISCONNECT_SCO) return;
    if (hsp_state < HSP_AUDIO_CONNECTION_ESTABLISHED) return;
    hsp_release_audio_connection = 1;
    hsp_run();
}

void hsp_hs_set_microphone_gain(uint8_t gain){
    if (gain >15) {
        log_info("Gain must be in interval [0..15], it is given %d", gain);
        return; 
    }
    hs_microphone_gain = gain;
    hsp_run();
}

// AG +VGS=5  [0..15] ; HS AT+VGM=6 | AG OK
void hsp_hs_set_speaker_gain(uint8_t gain){
    if (gain >15) {
        log_info("Gain must be in interval [0..15], it is given %d", gain);
        return; 
    }
    hs_speaker_gain = gain;
    hsp_run();
}  
    
static void hsp_run_handle_state(void){
    switch (hsp_state){
        case HSP_AUDIO_CONNECTION_ESTABLISHED:
        case HSP_RFCOMM_CONNECTION_ESTABLISHED:

            if (hs_microphone_gain >= 0){
                if (!rfcomm_can_send_packet_now(rfcomm_cid)) {
                    rfcomm_request_can_send_now_event(rfcomm_cid);
                    return;
                }
                char buffer[20];
                snprintf(buffer, sizeof(buffer), "%s=%d\r",
                         HSP_HS_MICROPHONE_GAIN, hs_microphone_gain);
                buffer[sizeof(buffer) - 1] = 0;
                hsp_hs_send_str_over_rfcomm(rfcomm_cid, buffer);
                hs_microphone_gain = -1;
                break;
            }

            if (hs_speaker_gain >= 0){
                if (!rfcomm_can_send_packet_now(rfcomm_cid)) {
                    rfcomm_request_can_send_now_event(rfcomm_cid);
                    return;
                }
                char buffer[20];
                snprintf(buffer, sizeof(buffer), "%s=%d\r",
                         HSP_HS_SPEAKER_GAIN, hs_speaker_gain);
                buffer[sizeof(buffer) - 1] = 0;
                hsp_hs_send_str_over_rfcomm(rfcomm_cid, buffer);
                hs_speaker_gain = -1;
                break;
            }
            break;
        case HSP_W4_RFCOMM_DISCONNECTED:
            rfcomm_disconnect(rfcomm_cid);
            break;
        default:
            break;
    }
}

static void hsp_run(void){

    if (wait_ok) return;

    if (hs_accept_sco_connection && hci_can_send_command_packet_now()){

        bool eSCO = hs_accept_sco_connection == 2;
        hs_accept_sco_connection = 0;

        log_info("HSP: sending hci_accept_connection_request.");

        // pick packet types based on SCO link type (SCO vs. eSCO)
        uint16_t packet_types;
        if (eSCO && hci_extended_sco_link_supported() && hci_remote_esco_supported(rfcomm_handle)){
            packet_types = 0x3F8;
        } else {
            packet_types = 0x0007;
        }

        // packet type override
        packet_types &= hsp_hs_sco_packet_types;

        // bits 6-9 are 'don't use'
        packet_types ^= 0x03c0;

        uint16_t sco_voice_setting = hci_get_sco_voice_setting();
        
        log_info("HFP: sending hci_accept_connection_request, sco_voice_setting %02x", sco_voice_setting);
        hci_send_cmd(&hci_accept_synchronous_connection, remote, 8000, 8000, 0xffff, sco_voice_setting, 0xff, packet_types);
        return;
    }

    if (hsp_release_audio_connection){
        if (!rfcomm_can_send_packet_now(rfcomm_cid)) {
            rfcomm_request_can_send_now_event(rfcomm_cid);
            return;
        }
        hsp_release_audio_connection = 0;
        wait_ok = 1;
        hsp_hs_send_str_over_rfcomm(rfcomm_cid, HSP_HS_AT_CKPD);
        return;
    }

    if (hsp_establish_audio_connection){
        if (!rfcomm_can_send_packet_now(rfcomm_cid)) {
            rfcomm_request_can_send_now_event(rfcomm_cid);
            return;
        }
        hsp_establish_audio_connection = 0;
        wait_ok = 1;
        hsp_hs_send_str_over_rfcomm(rfcomm_cid, HSP_HS_AT_CKPD);
        return;
    }
    
    if (hs_send_button_press){
        if (!rfcomm_can_send_packet_now(rfcomm_cid)) {
            rfcomm_request_can_send_now_event(rfcomm_cid);
            return;
        }
        hs_send_button_press = 0;
        wait_ok = 1;
        hsp_hs_send_str_over_rfcomm(rfcomm_cid, HSP_HS_AT_CKPD);
        return;
    }

    hsp_run_handle_state();
}


static void packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);    // ok: no channel for HCI_EVENT_PACKET and only single active RFCOMM channel
    if (packet_type == RFCOMM_DATA_PACKET){
        // skip over leading newline
        while ((size > 0) && ((packet[0] == '\n') || (packet[0] == '\r'))){
            size--;
            packet++;
        }
        if (strncmp((char *)packet, HSP_AG_RING, strlen(HSP_AG_RING)) == 0){
            emit_ring_event();
        } else if (strncmp((char *)packet, HSP_AG_OK, strlen(HSP_AG_OK)) == 0){
           wait_ok = 0;
        } else if (strncmp((char *)packet, HSP_MICROPHONE_GAIN, strlen(HSP_MICROPHONE_GAIN)) == 0){
            uint8_t gain = (uint8_t)btstack_atoi((char*)&packet[strlen(HSP_MICROPHONE_GAIN)]);
            emit_event(HSP_SUBEVENT_MICROPHONE_GAIN_CHANGED, gain);
        
        } else if (strncmp((char *)packet, HSP_SPEAKER_GAIN, strlen(HSP_SPEAKER_GAIN)) == 0){
            uint8_t gain = (uint8_t)btstack_atoi((char*)&packet[strlen(HSP_SPEAKER_GAIN)]);
            emit_event(HSP_SUBEVENT_SPEAKER_GAIN_CHANGED, gain);
        } else {
            if (!hsp_hs_callback) return;
            // strip trailing newline
            while ((size > 0) && ((packet[size-1] == '\n') || (packet[size-1] == '\r'))){
                size--;
            }
            // add trailing \0
            packet[size] = 0;
            // re-use incoming buffer to avoid reserving large buffers - ugly but efficient
            uint8_t * event = packet - 4;
            event[0] = HCI_EVENT_HSP_META;
            event[1] = size + 2;
            event[2] = HSP_SUBEVENT_AG_INDICATION;
            event[3] = size;
            (*hsp_hs_callback)(HCI_EVENT_PACKET, 0, event, size+4);
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
            switch(hci_event_connection_request_get_link_type(packet)){
                case 0: //  SCO
                case 2: // eSCO
                    hci_event_connection_request_get_bd_addr(packet, event_addr);
                    if (bd_addr_cmp(event_addr, remote) == 0){
                        if (hci_event_connection_request_get_link_type(packet) == 2){
                            hs_accept_sco_connection = 2;
                        } else {
                            hs_accept_sco_connection = 1;
                        }
                        log_info("hs_accept_sco_connection %u", hs_accept_sco_connection);
                    }
                    break;
                default:
                    break;                    
            }            
            break;

        case HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE:{
            if (hsp_state < HSP_RFCOMM_CONNECTION_ESTABLISHED) return;
            hci_event_synchronous_connection_complete_get_bd_addr(packet, event_addr);
            uint8_t status                   = hci_event_synchronous_connection_complete_get_status(packet);
            sco_handle                       = hci_event_synchronous_connection_complete_get_handle(packet);
            uint8_t  link_type               = hci_event_synchronous_connection_complete_get_link_type(packet);
            uint8_t  transmission_interval   = hci_event_synchronous_connection_complete_get_transmission_interval(packet);   // measured in slots
            uint8_t  retransmission_interval = hci_event_synchronous_connection_complete_get_retransmission_interval(packet); // measured in slots
            uint16_t rx_packet_length        = hci_event_synchronous_connection_complete_get_rx_packet_length(packet);        // measured in bytes
            uint16_t tx_packet_length        = hci_event_synchronous_connection_complete_get_tx_packet_length(packet);        // measured in bytes

            if (status != 0){
                log_error("(e)SCO Connection failed, status %u", status);
                emit_event_audio_connected(status, sco_handle);
                hsp_state = HSP_RFCOMM_CONNECTION_ESTABLISHED ;
                break;
            }

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
                 bd_addr_to_str(event_addr), transmission_interval, retransmission_interval, rx_packet_length, tx_packet_length, 
                 hci_event_synchronous_connection_complete_get_air_mode(packet));

            hsp_state = HSP_AUDIO_CONNECTION_ESTABLISHED;
            emit_event_audio_connected(status, sco_handle);
            break;                
        }

        case RFCOMM_EVENT_INCOMING_CONNECTION:
            if (hsp_state != HSP_IDLE) return;
            rfcomm_event_incoming_connection_get_bd_addr(packet, event_addr);
            rfcomm_cid = rfcomm_event_incoming_connection_get_rfcomm_cid(packet);
            log_info("RFCOMM channel %u requested for %s", rfcomm_event_incoming_connection_get_server_channel(packet), bd_addr_to_str(event_addr));
            hsp_state = HSP_W4_RFCOMM_CONNECTED;
            rfcomm_accept_connection(rfcomm_cid);
            break;

        case RFCOMM_EVENT_CHANNEL_OPENED:
            if (hsp_state != HSP_W4_RFCOMM_CONNECTED) return;
            if (rfcomm_event_channel_opened_get_status(packet)) {
                log_info("RFCOMM channel open failed, status %u", rfcomm_event_channel_opened_get_status(packet));
                hsp_state = HSP_IDLE;
                hsp_hs_reset_state();
            } else {
                rfcomm_event_channel_opened_get_bd_addr(packet, remote);
                rfcomm_handle = rfcomm_event_channel_opened_get_con_handle(packet);
                rfcomm_cid    = rfcomm_event_channel_opened_get_rfcomm_cid(packet);
                mtu           = rfcomm_event_channel_opened_get_max_frame_size(packet);
                log_info("RFCOMM channel open succeeded. New RFCOMM Channel ID %u, max frame size %u, handle %02x", rfcomm_cid, mtu, rfcomm_handle);
                hsp_state = HSP_RFCOMM_CONNECTION_ESTABLISHED;
            }
            emit_event(HSP_SUBEVENT_RFCOMM_CONNECTION_COMPLETE, rfcomm_event_channel_opened_get_status(packet));
            break;

        case RFCOMM_EVENT_CAN_SEND_NOW:
            hsp_run();
            break;
        
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            handle = hci_event_disconnection_complete_get_connection_handle(packet);
            if (handle == sco_handle){
                sco_handle = 0;
                hsp_state = HSP_RFCOMM_CONNECTION_ESTABLISHED;
                emit_event(HSP_SUBEVENT_AUDIO_DISCONNECTION_COMPLETE,0);
                break;
            } 
            break;

        case RFCOMM_EVENT_CHANNEL_CLOSED:
            hsp_state = HSP_IDLE;
            hsp_hs_reset_state();
            emit_event(HSP_SUBEVENT_RFCOMM_DISCONNECTION_COMPLETE,0);
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

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
            channel_nr = sdp_event_query_rfcomm_service_get_rfcomm_channel(packet);
            log_info("** Service name: '%s', RFCOMM port %u", sdp_event_query_rfcomm_service_get_name(packet), channel_nr);
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            if (channel_nr > 0){
                hsp_state = HSP_W4_RFCOMM_CONNECTED;
                log_info("HSP: SDP_QUERY_COMPLETE. RFCOMM create channel, addr %s, rfcomm channel nr %d", bd_addr_to_str(remote), channel_nr);
                rfcomm_create_channel(packet_handler, remote, channel_nr, NULL); 
                break;
            }
            hsp_hs_reset_state();
            log_info("Service not found, status %u.", sdp_event_query_complete_get_status(packet));
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

void hsp_hs_send_button_press(void){
    if ((hsp_state < HSP_RFCOMM_CONNECTION_ESTABLISHED) || (hsp_state >= HSP_W4_RFCOMM_DISCONNECTED)) return;
    hs_send_button_press = 1;
    hsp_run();
}

void hsp_hs_set_sco_packet_types(uint16_t packet_types){
    hsp_hs_sco_packet_types = packet_types;
}
