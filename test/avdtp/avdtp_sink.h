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
 * avdtp_sink.h
 * 
 * Audio/Video Distribution Transport Protocol (AVDTP) Sink
 *
 * AVDTP Sink is a device that accepts streamed media data.  
 */

#ifndef __AVDTP_SINK_H
#define __AVDTP_SINK_H

#include <stdint.h>
#include "hci.h"
#include "avdtp.h"

#if defined __cplusplus
extern "C" {
#endif

#define MAX_NUM_SEPS 10

typedef enum {
    AVDTP_SINK_IDLE,
    AVDTP_SINK_W4_L2CAP_CONNECTED,
    AVDTP_SINK_W2_DISCOVER_SEPS,
    AVDTP_SINK_W4_SEPS_DISCOVERED,
    AVDTP_SINK_W2_GET_CAPABILITIES,
    AVDTP_SINK_W4_CAPABILITIES,

    AVDTP_SINK_CONFIGURED,
    AVDTP_SINK_OPEN,
    AVDTP_SINK_STREAMING,
    AVDTP_SINK_CLOSING,
    AVDTP_SINK_ABORTING,
    AVDTP_SINK_W4_L2CAP_DISCONNECTED
} avdtp_sink_state_t;

typedef struct avdtp_sink_connection {
    btstack_linked_item_t    item;
    
    bd_addr_t remote_addr;
    hci_con_handle_t acl_handle;
    uint16_t l2cap_cid;

    avdtp_sink_state_t local_state;
    avdtp_sink_state_t remote_state;

    avdtp_sep_t remote_seps[MAX_NUM_SEPS];
    uint8_t remote_seps_num;
    uint8_t remote_sep_index_get_capabilities;
    
    uint8_t local_transaction_label;
    uint8_t remote_transaction_label;
    
    uint8_t release_l2cap_connection;

    avdtp_capabilities_t local_capabilities;
    avdtp_capabilities_t remote_capabilities;
} avdtp_sink_connection_t;


/* API_START */

/**
 * @brief AVDTP Sink service record. 
 * @param service
 * @param service_record_handle
 * @param supported_features 16-bit bitmap, see AVDTP_SINK_SF_* values in avdtp.h
 * @param service_name
 * @param service_provider_name
 */
void a2dp_sink_create_sdp_record(uint8_t * service,  uint32_t service_record_handle, uint16_t supported_features, const char * service_name, const char * service_provider_name);

/**
 * @brief Set up AVDTP Sink device.
 */
void avdtp_sink_init(void);

void avdtp_sink_register_sep(avdtp_sep_type_t sep_type, avdtp_media_type_t media_type);

/**
 * @brief Register callback for the AVDTP Sink client. 
 * @param callback
 */
void avdtp_sink_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Connect to device with a bluetooth address. (and perform configuration?)
 * @param bd_addr
 */
void avdtp_sink_connect(bd_addr_t bd_addr);

/**
 * @brief Disconnect from device with connection handle. 
 * @param l2cap_cid
 */
void avdtp_sink_disconnect(uint16_t l2cap_cid);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // __AVDTP_SINK_H