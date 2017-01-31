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

/*
 * avrcp.h
 * 
 * Audio/Video Remote Control Profile
 *
 */

#ifndef __AVRCP_H
#define __AVRCP_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

typedef enum{
    AVRCP_CMD_NONE  = 0,
    AVRCP_UNIT_INFO = 0x30
} avrcp_command_t;

typedef enum {
    AVCTP_CONNECTION_IDLE,
    AVCTP_CONNECTION_W4_L2CAP_CONNECTED,
    AVCTP_CONNECTION_OPENED,
    AVCTP_W2_SEND_COMMAND,
    AVCTP_W2_RECEIVE_RESPONSE,
    AVCTP_CONNECTION_W4_L2CAP_DISCONNECTED
} avctp_connection_state_t;

typedef struct {
    btstack_linked_item_t    item;
    bd_addr_t remote_addr;
    hci_con_handle_t con_handle;
    uint16_t l2cap_signaling_cid;
    uint16_t transaction_label;

    avctp_connection_state_t state;
    uint8_t wait_to_send;

    avrcp_command_t cmd_to_send;
    uint8_t cmd_operands[5];
    uint8_t cmd_operands_lenght;
} avrcp_connection_t;

/**
 * @brief AVDTP Sink service record. 
 * @param service
 * @param service_record_handle
 * @param browsing  1 - supported, 0 - not supported
 * @param supported_features 16-bit bitmap, see AVDTP_SINK_SF_* values in avdtp.h
 * @param service_name
 * @param service_provider_name
 */
void avrcp_controller_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, const char * service_name, const char * service_provider_name);

/**
 * @brief AVDTP Sink service record. 
 * @param service
 * @param service_record_handle
 * @param browsing  1 - supported, 0 - not supported
 * @param supported_features 16-bit bitmap, see AVDTP_SINK_SF_* values in avdtp.h
 * @param service_name
 * @param service_provider_name
 */
void avrcp_target_create_sdp_record(uint8_t * service, uint32_t service_record_handle, uint8_t browsing, uint16_t supported_features, const char * service_name, const char * service_provider_name);


/**
 * @brief Set up AVDTP Sink device.
 */
void avrcp_init(void);

/**
 * @brief Register callback for the AVDTP Sink client. 
 * @param callback
 */
void avrcp_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Connect to device with a bluetooth address.
 * @param bd_addr
 */
void avrcp_connect(bd_addr_t bd_addr);

/**
 * @brief Pause.
 * @param con_handle
 */
void avrcp_unit_info(uint16_t con_handle);


/* API_END */
#if defined __cplusplus
}
#endif

#endif // __AVRCP_H