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

#define BTSTACK_FILE__ "hid_host.c"

#include <string.h>

#include "bluetooth.h"
#include "bluetooth_psm.h"
#include "bluetooth_sdp.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_hid_parser.h"
#include "classic/hid.h"
#include "classic/hid_host.h"
#include "classic/sdp_util.h"
#include "l2cap.h"

typedef enum {
    HID_HOST_IDLE,
    HID_HOST_CONTROL_CONNECTION_ESTABLISHED,
    HID_HOST_W4_SET_BOOT_MODE,
    HID_HOST_W4_INTERRUPT_CONNECTION_ESTABLISHED,
    HID_HOST_CONNECTION_ESTABLISHED,
    HID_HOST_W2_SEND_GET_REPORT,
    HID_HOST_W4_GET_REPORT_RESPONSE,
    HID_HOST_W2_SEND_SET_REPORT,
    HID_HOST_W4_SET_REPORT_RESPONSE,
    HID_HOST_W2_SEND_GET_PROTOCOL,
    HID_HOST_W4_GET_PROTOCOL_RESPONSE,
    HID_HOST_W2_SEND_SET_PROTOCOL,
    HID_HOST_W4_SET_PROTOCOL_RESPONSE,
    HID_HOST_W2_SEND_REPORT,
    HID_HOST_W4_SEND_REPORT_RESPONSE
} hid_host_state_t;

typedef struct {
    uint16_t  cid;
    bd_addr_t bd_addr;
    hci_con_handle_t con_handle;
    uint16_t  control_cid;
    uint16_t  interrupt_cid;

    hid_host_state_t state;
    hid_protocol_mode_t protocol_mode;

    uint8_t   user_request_can_send_now; 

    // get report
    hid_report_type_t report_type;
    uint8_t           report_id;

    // set report
    uint8_t * report;
    uint16_t  report_len;
} hid_host_connection_t;

static const uint8_t * hid_host_descriptor_storage;
static uint16_t hid_host_descriptor_storage_len;

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    uint8_t   event;
    // bd_addr_t address;
    // uint8_t   status;
    // uint16_t  l2cap_cid;
    // hid_host_t * hid_host;
    // uint8_t param;
    // hid_message_type_t         message_type;
    // hid_handshake_param_type_t message_status;

    switch (packet_type) {
        case HCI_EVENT_PACKET:
            event = hci_event_packet_get_type(packet);
            switch (event) {            
                case BTSTACK_EVENT_STATE:
                    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING){
                        printf("BTstack up and running. \n");
                    }
                    break;
                default:
                    break;
            }
        default:
            break;
    }
}

void hid_host_init(const uint8_t * hid_descriptor_storage, uint16_t hid_descriptor_storage_len){
    hid_host_descriptor_storage = hid_descriptor_storage;
    hid_host_descriptor_storage_len = hid_descriptor_storage_len;

    // register L2CAP Services for reconnections
    l2cap_register_service(packet_handler, PSM_HID_INTERRUPT, 0xffff, gap_get_security_level());
    l2cap_register_service(packet_handler, PSM_HID_CONTROL, 0xffff, gap_get_security_level());
}

void hid_host_register_packet_handler(btstack_packet_handler_t callback){
    UNUSED(callback);
}

uint8_t hid_host_connect(bd_addr_t addr, hid_protocol_mode_t protocol_mode, uint16_t * hid_cid){
    UNUSED(hid_cid);
    UNUSED(protocol_mode);
    return ERROR_CODE_SUCCESS;
}


void hid_host_disconnect(uint16_t hid_cid){
    UNUSED(hid_cid);
}

void hid_host_request_can_send_now_event(uint16_t hid_cid){
    UNUSED(hid_cid);
}

void hid_host_send_interrupt_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len){
    UNUSED(hid_cid);
    UNUSED(message);
    UNUSED(message_len);
}

void hid_host_send_control_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len){
    UNUSED(hid_cid);
    UNUSED(message);
    UNUSED(message_len);
}