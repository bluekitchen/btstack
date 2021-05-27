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

/**
 * @title HID Device
 *
 */

#ifndef HID_DEVICE_H
#define HID_DEVICE_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_hid.h"
#include "btstack_hid_parser.h"

#if defined __cplusplus
extern "C" {
#endif


/* API_START */

typedef struct {
    uint16_t        hid_device_subclass;
    uint8_t         hid_country_code;
    uint8_t         hid_virtual_cable;
    uint8_t         hid_remote_wake;
    uint8_t         hid_reconnect_initiate;
    bool            hid_normally_connectable;
    bool            hid_boot_device;
    uint16_t        hid_ssr_host_max_latency; 
    uint16_t        hid_ssr_host_min_timeout;
    uint16_t        hid_supervision_timeout;
    const uint8_t * hid_descriptor;
    uint16_t        hid_descriptor_size;
    const char *    device_name;
} hid_sdp_record_t;

/**
 * @brief Create HID Device SDP service record. 
 * @param service Empty buffer in which a new service record will be stored.
 * @param have_remote_audio_control 
 * @param service
 * @param service_record_handle
 * @param params
 */
void hid_create_sdp_record(uint8_t * service, uint32_t service_record_handle, const hid_sdp_record_t * params);

/**
 * @brief Set up HID Device 
 * @param boot_protocol_mode_supported
 * @param hid_descriptor_len
 * @param hid_descriptor
 */
void hid_device_init(bool boot_protocol_mode_supported, uint16_t hid_descriptor_len, const uint8_t * hid_descriptor);

/**
 * @brief Register callback for the HID Device client. 
 * @param callback
 */
void hid_device_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Register get report callback for the HID Device client. 
 * @param callback
 */
void hid_device_register_report_request_callback(int (*callback)(uint16_t hid_cid, hid_report_type_t report_type, uint16_t report_id, int * out_report_size, uint8_t * out_report));

/**
 * @brief Register set report callback for the HID Device client. 
 * @param callback
 */
void hid_device_register_set_report_callback(void (*callback)(uint16_t hid_cid, hid_report_type_t report_type, int report_size, uint8_t * report));

/**
 * @brief Register callback to receive report data for the HID Device client. 
 * @param callback
 */
void hid_device_register_report_data_callback(void (*callback)(uint16_t cid, hid_report_type_t report_type, uint16_t report_id, int report_size, uint8_t * report));

/*
 * @brief Create HID connection to HID Host
 * @param addr
 * @param hid_cid to use for other commands
 * @result status
 */
uint8_t hid_device_connect(bd_addr_t addr, uint16_t * hid_cid);

/*
 * @brief Disconnect from HID Host
 * @param hid_cid
 */
void hid_device_disconnect(uint16_t hid_cid);

/**
 * @brief Request can send now event to send HID Report
 * Generates an HID_SUBEVENT_CAN_SEND_NOW subevent
 * @param hid_cid
 */
void hid_device_request_can_send_now_event(uint16_t hid_cid);

/**
 * @brief Send HID message on interrupt channel
 * @param hid_cid
 */
void hid_device_send_interrupt_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len);

/**
 * @brief Send HID message on control channel
 * @param hid_cid
 */
void hid_device_send_control_message(uint16_t hid_cid, const uint8_t * message, uint16_t message_len);

/**
 * @brief Retutn 1 if boot protocol mode active
 * @param hid_cid
 */
int hid_device_in_boot_protocol_mode(uint16_t hid_cid);

/**
 * @brief De-Init HID Device
 */
void hid_device_deinit(void);

/* API_END */

/* Only needed for PTS Testing */
void hid_device_disconnect_interrupt_channel(uint16_t hid_cid);
void hid_device_disconnect_control_channel(uint16_t hid_cid);

#if defined __cplusplus
}
#endif

#endif
