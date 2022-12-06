/*
 * Copyright (C) 2022 BlueKitchen GmbH
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

/**
 * @title Published Audio Capabilities Service Client
 * 
 */

#ifndef PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_H
#define PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_H

#include <stdint.h>
#include "le-audio/le_audio.h"
#include "le-audio/gatt-service/published_audio_capabilities_service_util.h"

#if defined __cplusplus
extern "C" {
#endif

#define PACS_CLIENT_MAX_ATT_BUFFER_SIZE             512
#define PACS_ENDPOINT_MAX_NUM                        10

/* API_START */
typedef enum {
    PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_IDLE,
    PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE,
    PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT,
    PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTICS,
    PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_RESULT,

    PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_QUERY_CHARACTERISTIC_DESCRIPTORS,
    PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_CHARACTERISTIC_DESCRIPTORS_RESULT,
    PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_REGISTER_NOTIFICATION,
    PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_NOTIFICATION_REGISTERED,

    PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_CONNECTED,

    PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_WRITE_SERVER,
    PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_SERVER_WRITE_RESPONSE,

    PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W2_READ_SERVER,
    PUBLISHED_AUDIO_CAPABILITIES_SERVICE_CLIENT_STATE_W4_SERVER_READ_RESPONSE
} published_audio_capabilities_service_client_state_t;

typedef enum {
    PACS_CLIENT_CHARACTERISTIC_INDEX_SINK_AUDIO_LOCATIONS = 0,
    PACS_CLIENT_CHARACTERISTIC_INDEX_SOURCE_AUDIO_LOCATIONS  ,
    PACS_CLIENT_CHARACTERISTIC_INDEX_AVAILABLE_AUDIO_CONTEXTS,
    PACS_CLIENT_CHARACTERISTIC_INDEX_SUPPORTED_AUDIO_CONTEXTS,
    PACS_CLIENT_CHARACTERISTIC_INDEX_SINK_PACK,
    PACS_CLIENT_CHARACTERISTIC_INDEX_SOURCE_PACK,  
    PACS_CLIENT_CHARACTERISTIC_INDEX_RFU  
} pacs_client_characteristic_index_t;

typedef struct {
    uint16_t value_handle;
    uint16_t ccc_handle; 
    uint16_t properties;
    uint16_t uuid16; 
    uint16_t end_handle;
} pacs_client_characteristic_t;

typedef struct {
    btstack_linked_item_t item;
    
    hci_con_handle_t  con_handle;
    uint16_t          cid;
    uint16_t          mtu;
    published_audio_capabilities_service_client_state_t  state;
    
    // service
    uint16_t start_handle;
    uint16_t end_handle;
    
    // pacs_client_characteristic_t characteristics_general[4 + PACS_ENDPOINT_MAX_NUM];
    pacs_client_characteristic_t pacs_characteristics[4 + PACS_ENDPOINT_MAX_NUM];
    // used for notification registration
    uint8_t  pacs_characteristics_index;

    // used for memory capacity checking
    uint8_t  service_instances_num;
    uint8_t  pacs_characteristics_num;
    
    // used for write segmentation
    uint8_t  buffer[PACS_CLIENT_MAX_ATT_BUFFER_SIZE];
    uint16_t buffer_offset;
    uint16_t data_size;

    gatt_client_notification_t notification_listener;
    
    // user for queries
    
    uint8_t value[4];
    uint8_t value_len;
    pacs_client_characteristic_index_t query_characteristic_index;
} pacs_client_connection_t;

/**
 * @brief Init Published Audio Capabilities Service Client
 */
void published_audio_capabilities_service_client_init(btstack_packet_handler_t packet_handler);

uint8_t published_audio_capabilities_service_client_connect(pacs_client_connection_t * connection, hci_con_handle_t con_handle, uint16_t * pacs_cid);

uint8_t published_audio_capabilities_service_client_get_sink_audio_locations(uint16_t pacs_cid);
uint8_t published_audio_capabilities_service_client_set_sink_audio_locations(uint16_t pacs_cid, uint32_t audio_locations_mask);

uint8_t published_audio_capabilities_service_client_get_source_audio_locations(uint16_t pacs_cid);
uint8_t published_audio_capabilities_service_client_set_source_audio_locations(uint16_t pacs_cid, uint32_t audio_locations_mask);

uint8_t published_audio_capabilities_service_client_get_available_audio_contexts(uint16_t pacs_cid);

uint8_t published_audio_capabilities_service_client_get_supported_audio_contexts(uint16_t pacs_cid);

uint8_t published_audio_capabilities_service_client_get_sink_pacs(uint16_t pacs_cid);

uint8_t published_audio_capabilities_service_client_get_source_pacs(uint16_t pacs_cid);


/**
 * @brief Deinit Published Audio Capabilities Service Client
 */
void published_audio_capabilities_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

