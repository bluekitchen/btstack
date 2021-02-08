/*
 * Copyright (C) 2020 BlueKitchen GmbH
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

#ifndef HID_HOST_H
#define HID_HOST_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_hid_parser.h"
#include "classic/hid.h"

#if defined __cplusplus
extern "C" {
#endif


typedef enum {
    HID_HOST_IDLE,
    HID_HOST_W2_SEND_SDP_QUERY,
    HID_HOST_W4_SDP_QUERY_RESULT,
    
    HID_HOST_W4_CONTROL_CONNECTION_ESTABLISHED,
    HID_HOST_CONTROL_CONNECTION_ESTABLISHED,
    
    HID_HOST_W4_SET_BOOT_MODE,
    HID_HOST_W4_INCOMING_INTERRUPT_CONNECTION,
    HID_HOST_W4_INTERRUPT_CONNECTION_ESTABLISHED,
    HID_HOST_W4_PROTOCOL_MODE_VERIFIED,
    HID_HOST_CONNECTION_ESTABLISHED,

    HID_HOST_W2_SEND_GET_REPORT,
    HID_HOST_W4_GET_REPORT_RESPONSE,
    HID_HOST_W2_SEND_SET_REPORT,
    HID_HOST_W4_SET_REPORT_RESPONSE,
    HID_HOST_W2_SEND_GET_PROTOCOL,
    HID_HOST_W4_GET_PROTOCOL_RESPONSE,

    HID_HOST_W2_SEND_REPORT,
    HID_HOST_W4_SEND_REPORT_RESPONSE,

    HID_HOST_W4_INTERRUPT_CONNECTION_DISCONNECTED,
    HID_HOST_W4_CONTROL_CONNECTION_DISCONNECTED
} hid_host_state_t;

typedef struct {
    btstack_linked_item_t item;

    uint16_t  hid_cid;
    hci_con_handle_t con_handle;
    
    bd_addr_t remote_addr;
    
    uint16_t  control_cid;
    uint16_t  control_psm;
    uint16_t  interrupt_cid;
    uint16_t  interrupt_psm;

    hid_host_state_t state;
    bool incoming;
    hid_protocol_mode_t protocol_mode;
    
    bool set_protocol;
    bool w4_set_protocol_response;
    hid_protocol_mode_t requested_protocol_mode;

    uint16_t hid_descriptor_offset;
    uint16_t hid_descriptor_len;
    uint16_t hid_descriptor_max_len;

    uint8_t   user_request_can_send_now; 

    // get report
    hid_report_type_t report_type;
    uint8_t           report_id;

    // control message, bit mask:
    // SUSSPEND             1
    // EXIT_SUSSPEND        2
    // VIRTUAL_CABLE_UNPLUG 4
    uint8_t control_tasks;
    
    // set report
    const uint8_t * report;
    uint16_t  report_len;
} hid_host_connection_t;

/* API_START */
/**
 * @brief Set up HID Host 
 * @param boot_protocol_mode_supported
 * @param hid_descriptor_storage
 * @param hid_descriptor_storage_len
 */
void hid_host_init(uint8_t * hid_descriptor_storage, uint16_t hid_descriptor_storage_len);

/**
 * @brief Register callback for the HID Device Host. 
 * @param callback
 */
void hid_host_register_packet_handler(btstack_packet_handler_t callback);

/*
 * @brief Create HID connection to HID Host
 * @param remote_addr
 * @param hid_cid to use for other commands
 * @result status
 */
uint8_t hid_host_connect(bd_addr_t remote_addr, hid_protocol_mode_t protocol_mode, uint16_t * hid_cid);

uint8_t hid_host_accept_connection(uint16_t hid_cid, hid_protocol_mode_t protocol_mode);
uint8_t hid_host_decline_connection(uint16_t hid_cid);

/*
 * @brief Disconnect from HID Host
 * @param hid_cid
 */
void hid_host_disconnect(uint16_t hid_cid);

// Control messages:
uint8_t hid_host_send_suspend(uint16_t hid_cid);
uint8_t hid_host_send_exit_suspend(uint16_t hid_cid);
uint8_t hid_host_send_virtual_cable_unplug(uint16_t hid_cid);

uint8_t hid_host_send_set_protocol_mode(uint16_t hid_cid, hid_protocol_mode_t protocol_mode);
uint8_t hid_host_send_get_protocol(uint16_t hid_cid);

uint8_t hid_host_send_set_report(uint16_t hid_cid, hid_report_type_t report_type, uint8_t report_id, const uint8_t * report, uint8_t report_len);
uint8_t hid_host_send_get_report(uint16_t hid_cid, hid_report_type_t report_type, uint8_t report_id);

/**
 * @brief Send HID message on interrupt channel
 * @param hid_cid
 */
// on interrupt channel, allways output
uint8_t hid_host_send_report(uint16_t hid_cid, uint8_t report_id, const uint8_t * report, uint8_t report_len);

const uint8_t * hid_descriptor_storage_get_descriptor_data(uint16_t hid_cid);
const uint16_t hid_descriptor_storage_get_descriptor_len(uint16_t hid_cid);
/* API_END */


#if defined __cplusplus
}
#endif

#endif
