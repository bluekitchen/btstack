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

/**
 * @title HID Host
 *
 */

#ifndef HID_HOST_H
#define HID_HOST_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_hid.h"
#include "btstack_hid_parser.h"

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

    uint16_t host_max_latency;
    uint16_t host_min_timeout;

    uint16_t hid_descriptor_offset;
    uint16_t hid_descriptor_len;
    uint16_t hid_descriptor_max_len;
    uint8_t  hid_descriptor_status;     // ERROR_CODE_SUCCESS if descriptor available, 
                                        // ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if not, and 
                                        // ERROR_CODE_MEMORY_CAPACITY_EXCEEDED if descriptor is larger then the available space

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
 * @param hid_descriptor_storage
 * @param hid_descriptor_storage_len
 */
void hid_host_init(uint8_t * hid_descriptor_storage, uint16_t hid_descriptor_storage_len);

/**
 * @brief Register callback for the HID Host. 
 * @param callback
 */
void hid_host_register_packet_handler(btstack_packet_handler_t callback);

/*
 * @brief Create HID connection to HID Device and emit HID_SUBEVENT_CONNECTION_OPENED event with status code, 
 * followed by HID_SUBEVENT_DESCRIPTOR_AVAILABLE that informs if the HID Descriptor was found. In the case of incoming
 * connection, i.e. HID Device initiating the connection, the HID_SUBEVENT_DESCRIPTOR_AVAILABLE is delayed, and the reports 
 * may already come via HID_SUBEVENT_REPORT event. It is up to the application code if 
 * these reports should be buffered or ignored until the descriptor is available.
 * @note  HID_PROTOCOL_MODE_REPORT_WITH_FALLBACK_TO_BOOT will try ti set up REPORT mode, but fallback to BOOT mode if necessary. 
 * @note  HID_SUBEVENT_DESCRIPTOR_AVAILABLE possible status values are:
 * - ERROR_CODE_SUCCESS if descriptor available, 
 * - ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE if not, and 
 * - ERROR_CODE_MEMORY_CAPACITY_EXCEEDED if descriptor is larger then the available space
 * @param remote_addr
 * @param protocol_mode see hid_protocol_mode_t in btstack_hid.h
 * @param hid_cid to use for other commands
 * @result status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_COMMAND_DISALLOWED, BTSTACK_MEMORY_ALLOC_FAILED
 */
uint8_t hid_host_connect(bd_addr_t remote_addr, hid_protocol_mode_t protocol_mode, uint16_t * hid_cid);


/*
 * @brief Accept incoming HID connection, this should be called upon receiving HID_SUBEVENT_INCOMING_CONNECTION event.
 * @param hid_cid
 * @param protocol_mode see hid_protocol_mode_t in btstack_hid.h
 * @result status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, ERROR_CODE_COMMAND_DISALLOWED
 */
uint8_t hid_host_accept_connection(uint16_t hid_cid, hid_protocol_mode_t protocol_mode);

/*
 * @brief Decline incoming HID connection, this should be called upon receiving HID_SUBEVENT_INCOMING_CONNECTION event.
 * @param hid_cid
 * @result status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, ERROR_CODE_COMMAND_DISALLOWED
 */
uint8_t hid_host_decline_connection(uint16_t hid_cid);

/*
 * @brief Disconnect from HID Device and emit HID_SUBEVENT_CONNECTION_CLOSED event.
 * @param hid_cid
 */
void hid_host_disconnect(uint16_t hid_cid);

// Control messages:

/*
 * @brief Send SUSPEND control signal to connected HID Device. A Bluetooth HID Device which receives a SUSPEND control signal
 * may optionally disconnect from the Bluetooth HID Host.
 * @param hid_cid
 * @result status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, ERROR_CODE_COMMAND_DISALLOWED
 */
uint8_t hid_host_send_suspend(uint16_t hid_cid);

/*
 * @brief Order connected HID Device to exit suspend mode. 
 * The Bluetooth HID Device shall send a report to the Bluetooth HID Host.
 * @param hid_cid
 * @result status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, ERROR_CODE_COMMAND_DISALLOWED
 */
uint8_t hid_host_send_exit_suspend(uint16_t hid_cid);

/*
 * @brief Unplug connected HID Device. 
 * @note This is the only command that can be also received from HID Device. It will be indicated by receiving 
 * HID_SUBEVENT_VIRTUAL_CABLE_UNPLUG event, as well as disconnecting HID Host from device.
 * @param hid_cid
 * @result status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, ERROR_CODE_COMMAND_DISALLOWED
 */
uint8_t hid_host_send_virtual_cable_unplug(uint16_t hid_cid);

/*
 * @brief Set Protocol Mode on the Bluetooth HID Device and emit HID_SUBEVENT_SET_PROTOCOL_RESPONSE event with handshake_status, see hid_handshake_param_type_t
 * @param hid_cid
 * @param protocol_mode see hid_protocol_mode_t in btstack_hid.h
 * @result status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, ERROR_CODE_COMMAND_DISALLOWED
 */
uint8_t hid_host_send_set_protocol_mode(uint16_t hid_cid, hid_protocol_mode_t protocol_mode);

/*
 * @brief Retrieve the Protocol Mode of the Bluetooth HID Device and emit HID_SUBEVENT_GET_PROTOCOL_RESPONSE with handshake_status, see hid_handshake_param_type_t
 * @param hid_cid
 * @param protocol_mode see hid_protocol_mode_t in btstack_hid.h
 * @result status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, ERROR_CODE_COMMAND_DISALLOWED
 */
uint8_t hid_host_send_get_protocol(uint16_t hid_cid);

/*

 * @brief Send report to a Bluetooth HID Device and emit HID_SUBEVENT_SET_REPORT_RESPONSE with handshake_status, see hid_handshake_param_type_t. The Bluetooth HID Host shall send complete reports.
 * @param hid_cid
 * @param report_type see hid_report_type_t in btstack_hid.h
 * @param report_id
 * @param report
 * @param report_len
 * @result status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, ERROR_CODE_COMMAND_DISALLOWED
 */
uint8_t hid_host_send_set_report(uint16_t hid_cid, hid_report_type_t report_type, uint8_t report_id, const uint8_t * report, uint8_t report_len);

/*
 * @brief Request a HID report from the Bluetooth HID Device and emit HID_SUBEVENT_GET_REPORT_RESPONSE event with with handshake_status, see hid_handshake_param_type_t. 
 * Polling Bluetooth HID Devices using the GET_REPORT transfer is costly in terms of time and overhead, 
 * and should be avoided whenever possible. The GET_REPORT transfer is typically only used by applications 
 * to determine the initial state of a Bluetooth HID Device. If the state of a report changes frequently, 
 * then the report should be reported over the more efficient Interrupt channel, see hid_host_send_report.
 * @param hid_cid
 * @param report_type see hid_report_type_t in btstack_hid.h
 * @param report_id
 * @result status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, ERROR_CODE_COMMAND_DISALLOWED
 */
uint8_t hid_host_send_get_report(uint16_t hid_cid, hid_report_type_t report_type, uint8_t report_id);

/**
 * @brief Send HID output report on interrupt channel.
 * @param hid_cid
 * @param report_id
 * @param report
 * @param report_len
 * @result status ERROR_CODE_SUCCESS on success, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER, ERROR_CODE_COMMAND_DISALLOWED
 */
uint8_t hid_host_send_report(uint16_t hid_cid, uint8_t report_id, const uint8_t * report, uint8_t report_len);

/*
 * @brief Get descriptor data
 * @param hid_cid
 * @result data
 */
const uint8_t * hid_descriptor_storage_get_descriptor_data(uint16_t hid_cid);

/*
 * @brief Get descriptor length
 * @param hid_cid
 * @result length
 */
uint16_t hid_descriptor_storage_get_descriptor_len(uint16_t hid_cid);

/* API_END */


#if defined __cplusplus
}
#endif

#endif
