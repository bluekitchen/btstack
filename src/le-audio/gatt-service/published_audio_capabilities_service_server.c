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

#define BTSTACK_FILE__ "published_audio_capabilities_service_server.c"

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"

#include "le-audio/le_audio_util.h"
#include "le-audio/gatt-service/published_audio_capabilities_service_server.h"

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#define PACS_MAX_NOTIFY_BUFFER_SIZE                             200

#define PACS_TASK_SEND_SINK_AUDIO_LOCATIONS                    0x01
#define PACS_TASK_SEND_SOURCE_AUDIO_LOCATIONS                  0x02
#define PACS_TASK_SEND_AVAILABLE_AUDIO_CONTEXTS                0x04
#define PACS_TASK_SEND_SUPPORTED_AUDIO_CONTEXTS                0x08
#define PACS_TASK_SEND_SINK_PAC_RECORD                         0x10
#define PACS_TASK_SEND_SOURCE_PAC_RECORD                       0x20

// typedef enum {
//     PAC_RECORD_FIELD_RECORDS_NUM = 0,
//     PAC_RECORD_FIELD_CODEC_ID,
//     PAC_RECORD_FIELD_CODEC_SPECIFIC_CAPABILITIES_LENGTH,
//     PAC_RECORD_FIELD_CODEC_SPECIFIC_CAPABILITIES,
//     PAC_RECORD_FIELD_METADATA_LENGTH,
//     PAC_RECORD_FIELD_METADATA,
//     PAC_RECORD_FIELD_NUM_FIELDS
// } pac_record_field_t;

// typedef enum {
//     PAC_RECORD_CAPABILITY_FIELD_LENGTH = 0,
//     PAC_RECORD_CAPABILITY_FIELD_TYPE,
//     PAC_RECORD_CAPABILITY_FIELD_VALUE,
//     PAC_RECORD_CAPABILITY_FIELD_NUM_FIELDS
// } pac_record_capability_field_t;

static att_service_handler_t    published_audio_capabilities_service;
static hci_con_handle_t         pacs_server_con_handle;
static btstack_packet_handler_t pacs_server_event_callback;
static btstack_context_callback_registration_t  pacs_server_scheduled_tasks_callback;
static uint8_t pacs_server_scheduled_tasks;

// characteristic: SINK_PAC                      READ  | NOTIFY  |
static uint16_t  pacs_sink_pac_handle;
static uint16_t  pacs_sink_pac_client_configuration_handle;
static uint16_t  pacs_sink_pac_client_configuration;

// characteristic: SINK_AUDIO_LOCATIONS          READ  | NOTIFY  | WRITE 
static uint16_t  pacs_sink_audio_locations_handle;
static uint16_t  pacs_sink_audio_locations_client_configuration_handle;
static uint16_t  pacs_sink_audio_locations_client_configuration;

// characteristic: SOURCE_PAC                    READ  | NOTIFY  |
static uint16_t  pacs_source_pac_handle;
static uint16_t  pacs_source_pac_client_configuration_handle;
static uint16_t  pacs_source_pac_client_configuration;

// characteristic: SOURCE_AUDIO_LOCATIONS        READ  | NOTIFY  | WRITE 
static uint16_t  pacs_source_audio_locations_handle;
static uint16_t  pacs_source_audio_locations_client_configuration_handle;
static uint16_t  pacs_source_audio_locations_client_configuration;

// characteristic: AVAILABLE_AUDIO_CONTEXTS      READ  |     
static uint16_t  pacs_available_audio_contexts_handle;
static uint16_t  pacs_available_audio_contexts_client_configuration_handle;
static uint16_t  pacs_available_audio_contexts_client_configuration;

// characteristic: SUPPORTED_AUDIO_CONTEXTS      READ  | NOTIFY  |  
static uint16_t  pacs_supported_audio_contexts_handle;
static uint16_t  pacs_supported_audio_contexts_client_configuration_handle;
static uint16_t  pacs_supported_audio_contexts_client_configuration;

// Data
static const pacs_record_t * pacs_sink_pac_records;
static uint8_t  pacs_sink_pac_records_num;
static uint32_t pacs_sink_audio_locations;

static const  pacs_record_t * pacs_source_pac_records;
static uint8_t  pacs_source_pac_records_num;
static uint32_t pacs_source_audio_locations;

static uint16_t pacs_sink_available_audio_contexts_mask;
static uint16_t pacs_sink_supported_audio_contexts_mask;

static uint16_t pacs_source_available_audio_contexts_mask;
static uint16_t pacs_source_supported_audio_contexts_mask;

static void pacs_server_reset_values(void){
    pacs_server_con_handle = HCI_CON_HANDLE_INVALID;
    pacs_sink_audio_locations_client_configuration = 0;
    pacs_source_audio_locations_client_configuration = 0;
    pacs_supported_audio_contexts_client_configuration = 0; 
}

static void pacs_server_set_con_handle(hci_con_handle_t con_handle, uint16_t configuration){
    pacs_server_con_handle = (configuration == 0) ? HCI_CON_HANDLE_INVALID : con_handle;
}

static void pacs_server_emit_audio_locations_received(hci_con_handle_t con_handle, uint32_t audio_locations, le_audio_role_t role){
    btstack_assert(pacs_server_event_callback != NULL);
    
    uint8_t event[10];
    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_PACS_SERVER_AUDIO_LOCATIONS;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    little_endian_store_32(event, pos, audio_locations);
    pos += 4;
    event[pos++] = (uint8_t)role;
    (*pacs_server_event_callback)(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static uint8_t pacs_server_codec_capability_virtual_memcpy_capabilities(const pacs_codec_capability_t * codec_capability, uint16_t * records_offset, uint8_t * buffer, uint16_t buffer_size, uint16_t buffer_offset){
    uint8_t  field_data[6];
    uint16_t stored_bytes = 0;
    uint8_t bytes_to_copy;

    uint8_t codec_capability_length = 0;
    
    uint16_t codec_capability_length_pos = *records_offset;
    field_data[0] = codec_capability_length;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, codec_capability_length_pos, buffer, buffer_size,
                                                        buffer_offset);
    *records_offset += 1;

    uint8_t codec_capability_type;
    for (codec_capability_type = (uint8_t)LE_AUDIO_CODEC_CONFIGURATION_TYPE_SAMPLING_FREQUENCY; codec_capability_type < (uint8_t) LE_AUDIO_CODEC_CAPABILITY_TYPE_RFU; codec_capability_type++){
        if ((codec_capability->codec_capability_mask & (1 << codec_capability_type) ) != 0 ){
            
            field_data[0] = 0;
            field_data[1] = codec_capability_type;

            switch ((le_audio_codec_capability_type_t)codec_capability_type){
                case LE_AUDIO_CODEC_CAPABILITY_TYPE_SAMPLING_FREQUENCY:
                    field_data[0] = 3;
                    little_endian_store_16(field_data, 2, codec_capability->sampling_frequency_mask);
                    break;
                case LE_AUDIO_CODEC_CAPABILITY_TYPE_FRAME_DURATION:
                    field_data[0] = 2;
                    field_data[2] = codec_capability->supported_frame_durations_mask;
                    break;
                case LE_AUDIO_CODEC_CAPABILITY_TYPE_SUPPORTED_AUDIO_CHANNEL_COUNTS:
                    field_data[0] = 2;
                    field_data[2] = codec_capability->supported_audio_channel_counts_mask;
                    break;
                case LE_AUDIO_CODEC_CAPABILITY_TYPE_OCTETS_PER_CODEC_FRAME:
                    field_data[0] = 5;
                    little_endian_store_16(field_data, 2, codec_capability->octets_per_frame_min_num);
                    little_endian_store_16(field_data, 4, codec_capability->octets_per_frame_max_num);
                    break;
                case LE_AUDIO_CODEC_CAPABILITY_TYPE_CODEC_FRAME_BLOCKS_PER_SDU:
                    field_data[0] = 2;
                    field_data[2] = codec_capability->frame_blocks_per_sdu_max_num;
                    break;
                default:
                    btstack_assert(false);
                    break;
            } 
            bytes_to_copy = field_data[0] + 1;
            stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, bytes_to_copy, *records_offset, buffer,
                                                                buffer_size, buffer_offset);
            *records_offset += bytes_to_copy;
            codec_capability_length += bytes_to_copy;
        }
    }

    field_data[0] = *records_offset - codec_capability_length_pos - 1;
    le_audio_util_virtual_memcpy_helper(field_data, 1, codec_capability_length_pos, buffer, buffer_size, buffer_offset);

    return stored_bytes;
}

// offset gives position into fully serialized pacs record
static uint16_t pacs_server_store_records(const pacs_record_t * pacs, uint8_t pac_records_num, uint8_t * buffer, uint16_t buffer_size, uint16_t buffer_offset){
    uint8_t  field_data[5];
    uint16_t pac_records_offset = 0;
    uint16_t stored_bytes = 0;
    
    memset(buffer, 0, buffer_size);

    field_data[0] = pac_records_num;
    stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 1, pac_records_offset, buffer, buffer_size,
                                                        buffer_offset);
    pac_records_offset++;

    uint8_t  i;
    for (i = 0; i < pac_records_num; i++){
        field_data[0] = pacs[i].codec_id.coding_format;
        little_endian_store_16(field_data, 1, pacs[i].codec_id.company_id);
        little_endian_store_16(field_data, 3, pacs[i].codec_id.vendor_specific_codec_id);
        stored_bytes += le_audio_util_virtual_memcpy_helper(field_data, 5, pac_records_offset, buffer, buffer_size,
                                                            buffer_offset);
        pac_records_offset += 5;

        stored_bytes += pacs_server_codec_capability_virtual_memcpy_capabilities(&pacs[i].codec_capability, &pac_records_offset, buffer, buffer_size, buffer_offset);
        stored_bytes += le_audio_util_metadata_virtual_memcpy(&pacs[i].metadata, pacs[i].metadata_length,
                                                              &pac_records_offset, buffer, buffer_size, buffer_offset);
    }
    return stored_bytes;
}

static void pacs_server_can_send_now(void * context){
    UNUSED(context);

    if ((pacs_server_scheduled_tasks & PACS_TASK_SEND_SINK_AUDIO_LOCATIONS) != 0) {
        pacs_server_scheduled_tasks &= ~PACS_TASK_SEND_SINK_AUDIO_LOCATIONS;
        uint8_t value[4];
        little_endian_store_32(value, 0, pacs_sink_audio_locations);
        att_server_notify(pacs_server_con_handle, pacs_sink_audio_locations_handle, &value[0], sizeof(value));

    } else if ((pacs_server_scheduled_tasks & PACS_TASK_SEND_SOURCE_AUDIO_LOCATIONS) != 0) {
        pacs_server_scheduled_tasks &= ~PACS_TASK_SEND_SOURCE_AUDIO_LOCATIONS;
        uint8_t value[4];
        little_endian_store_32(value, 0, pacs_source_audio_locations);
        att_server_notify(pacs_server_con_handle, pacs_source_audio_locations_handle, &value[0], sizeof(value));

    } else if ((pacs_server_scheduled_tasks & PACS_TASK_SEND_AVAILABLE_AUDIO_CONTEXTS) != 0) {
        pacs_server_scheduled_tasks &= ~PACS_TASK_SEND_AVAILABLE_AUDIO_CONTEXTS;
        uint8_t value[4];
        little_endian_store_16(value, 0, pacs_sink_available_audio_contexts_mask);
        little_endian_store_16(value, 2, pacs_source_available_audio_contexts_mask);
        att_server_notify(pacs_server_con_handle, pacs_available_audio_contexts_handle, &value[0], sizeof(value));

    } else if ((pacs_server_scheduled_tasks & PACS_TASK_SEND_SUPPORTED_AUDIO_CONTEXTS) != 0) {
        pacs_server_scheduled_tasks &= ~PACS_TASK_SEND_SUPPORTED_AUDIO_CONTEXTS;
        uint8_t value[4];
        little_endian_store_16(value, 0, pacs_sink_supported_audio_contexts_mask);
        little_endian_store_16(value, 2, pacs_source_supported_audio_contexts_mask);
        att_server_notify(pacs_server_con_handle, pacs_supported_audio_contexts_handle, &value[0], sizeof(value));
    
    } else if ((pacs_server_scheduled_tasks & PACS_TASK_SEND_SINK_PAC_RECORD) != 0) {
        pacs_server_scheduled_tasks &= ~PACS_TASK_SEND_SINK_PAC_RECORD;
        
        if (att_server_get_mtu(pacs_server_con_handle) >= PACS_MAX_NOTIFY_BUFFER_SIZE){
            uint8_t value[PACS_MAX_NOTIFY_BUFFER_SIZE];
            uint16_t bytes_stored = pacs_server_store_records(pacs_sink_pac_records, pacs_sink_pac_records_num, value, sizeof(value), 0);
            att_server_notify(pacs_server_con_handle, pacs_sink_pac_handle, &value[0], bytes_stored);
        }

    } else if ((pacs_server_scheduled_tasks & PACS_TASK_SEND_SOURCE_PAC_RECORD) != 0) {
        pacs_server_scheduled_tasks &= ~PACS_TASK_SEND_SOURCE_PAC_RECORD;
        if (att_server_get_mtu(pacs_server_con_handle) >= PACS_MAX_NOTIFY_BUFFER_SIZE){
            uint8_t value[PACS_MAX_NOTIFY_BUFFER_SIZE];
            uint16_t bytes_stored = pacs_server_store_records(pacs_source_pac_records, pacs_source_pac_records_num, value, sizeof(value), 0);
            att_server_notify(pacs_server_con_handle, pacs_source_pac_handle, &value[0], bytes_stored);
        }
    }

    if (pacs_server_scheduled_tasks != 0){
        att_server_register_can_send_now_callback(&pacs_server_scheduled_tasks_callback, pacs_server_con_handle);
    }
}

static void pacs_server_set_callback(uint8_t task){
    if (pacs_server_con_handle == HCI_CON_HANDLE_INVALID){
        pacs_server_scheduled_tasks &= ~task;
        return;
    }

    uint8_t scheduled_tasks = pacs_server_scheduled_tasks;
    pacs_server_scheduled_tasks |= task;
    if (scheduled_tasks == 0){
        pacs_server_scheduled_tasks_callback.callback = &pacs_server_can_send_now;
        att_server_register_can_send_now_callback(&pacs_server_scheduled_tasks_callback, pacs_server_con_handle);
    }
}

static uint16_t pacs_server_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(con_handle);


#ifdef ENABLE_TESTING_SUPPORT
    printf("read callback 0x%02x \n", attribute_handle);
#endif 

    if (attribute_handle == pacs_sink_pac_handle){
        return pacs_server_store_records(pacs_sink_pac_records, pacs_sink_pac_records_num, buffer, buffer_size, offset);
    }

    if (attribute_handle == pacs_source_pac_handle){
        return pacs_server_store_records(pacs_source_pac_records, pacs_source_pac_records_num, buffer, buffer_size, offset);
    }

    if (attribute_handle == pacs_sink_audio_locations_handle){
        uint8_t value[4];
        little_endian_store_32(value, 0, pacs_sink_audio_locations);
        return att_read_callback_handle_blob(value, sizeof(value), offset, buffer, buffer_size);
    }
    
    if (attribute_handle == pacs_source_audio_locations_handle){
        uint8_t value[4];
        little_endian_store_32(value, 0, pacs_source_audio_locations);
        return att_read_callback_handle_blob(value, sizeof(value), offset, buffer, buffer_size);
    }
    
    if (attribute_handle == pacs_supported_audio_contexts_handle){
        uint8_t value[4];
        little_endian_store_16(value, 0, pacs_sink_supported_audio_contexts_mask);
        little_endian_store_16(value, 2, pacs_source_supported_audio_contexts_mask);
        return att_read_callback_handle_blob(value, sizeof(value), offset, buffer, buffer_size);
    }

    if (attribute_handle == pacs_available_audio_contexts_handle){
        uint8_t value[4];
        little_endian_store_16(value, 0, pacs_sink_available_audio_contexts_mask);
        little_endian_store_16(value, 2, pacs_source_available_audio_contexts_mask);
        return att_read_callback_handle_blob(value, sizeof(value), offset, buffer, buffer_size);
    }

    if (attribute_handle == pacs_sink_pac_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(pacs_sink_pac_client_configuration, offset, buffer, buffer_size);
    }

    if (attribute_handle == pacs_source_pac_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(pacs_source_pac_client_configuration, offset, buffer, buffer_size);
    }

    if (attribute_handle == pacs_sink_audio_locations_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(pacs_sink_audio_locations_client_configuration, offset, buffer, buffer_size);
    }

    if (attribute_handle == pacs_source_audio_locations_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(pacs_source_audio_locations_client_configuration, offset, buffer, buffer_size);
    }
    
    if (attribute_handle == pacs_supported_audio_contexts_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(pacs_supported_audio_contexts_client_configuration, offset, buffer, buffer_size);
    }

    if (attribute_handle == pacs_available_audio_contexts_client_configuration_handle){
        return att_read_callback_handle_little_endian_16(pacs_available_audio_contexts_client_configuration, offset, buffer, buffer_size);
    }
    return 0;
}

static int pacs_server_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(transaction_mode);
    UNUSED(offset);

    if (attribute_handle == pacs_sink_audio_locations_handle){
        if (buffer_size != 4){
            return ATT_ERROR_WRITE_REQUEST_REJECTED;
        }

        uint32_t locations = little_endian_read_32(buffer, 0);
        if (locations == LE_AUDIO_LOCATION_MASK_NOT_ALLOWED || ((locations & LE_AUDIO_LOCATION_MASK_RFU) != 0 )){
            return ATT_ERROR_WRITE_REQUEST_REJECTED;
        }
        published_audio_capabilities_service_server_set_sink_audio_locations(locations);
        pacs_server_emit_audio_locations_received(con_handle, locations, LE_AUDIO_ROLE_SINK);
    }

    else if (attribute_handle == pacs_source_audio_locations_handle){
        if (buffer_size != 4){
            return ATT_ERROR_WRITE_REQUEST_REJECTED;
        }

        uint32_t locations = little_endian_read_32(buffer, 0);
        if (locations == LE_AUDIO_LOCATION_MASK_NOT_ALLOWED || ((locations & LE_AUDIO_LOCATION_MASK_RFU) != 0 )){
            return ATT_ERROR_WRITE_REQUEST_REJECTED;
        }
        published_audio_capabilities_service_server_set_source_audio_locations(locations);
        pacs_server_emit_audio_locations_received(con_handle, locations, LE_AUDIO_ROLE_SOURCE);
    }

    else if (attribute_handle == pacs_sink_pac_client_configuration_handle){
        pacs_sink_pac_client_configuration = little_endian_read_16(buffer, 0);
        pacs_server_set_con_handle(con_handle, pacs_sink_pac_client_configuration);
    }

    else if (attribute_handle == pacs_source_pac_client_configuration_handle){
        pacs_source_pac_client_configuration = little_endian_read_16(buffer, 0);
        pacs_server_set_con_handle(con_handle, pacs_source_pac_client_configuration);
    }

    else if (attribute_handle == pacs_sink_audio_locations_client_configuration_handle){
        pacs_sink_audio_locations_client_configuration = little_endian_read_16(buffer, 0);
        pacs_server_set_con_handle(con_handle, pacs_sink_audio_locations_client_configuration);
    }

    else if (attribute_handle == pacs_source_audio_locations_client_configuration_handle){
        pacs_source_audio_locations_client_configuration = little_endian_read_16(buffer, 0);
        pacs_server_set_con_handle(con_handle, pacs_source_audio_locations_client_configuration);
    }
    
    else if (attribute_handle == pacs_supported_audio_contexts_client_configuration_handle){
        pacs_supported_audio_contexts_client_configuration = little_endian_read_16(buffer, 0);
        pacs_server_set_con_handle(con_handle, pacs_supported_audio_contexts_client_configuration);
    }

    else if (attribute_handle == pacs_available_audio_contexts_client_configuration_handle){
        pacs_available_audio_contexts_client_configuration = little_endian_read_16(buffer, 0);
        pacs_server_set_con_handle(con_handle, pacs_available_audio_contexts_client_configuration);
    }

    return 0;
}

static void pacs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(packet);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET){
        return;
    }

    hci_con_handle_t con_handle;
    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            if (pacs_server_con_handle == con_handle){
                pacs_server_reset_values();
            }
            break;
        default:
            break;
    }
}


static bool pacs_server_node_valid(pacs_streamendpoint_t * streamendpoint){
    if (streamendpoint == NULL){
        return true;
    }
    if ((streamendpoint->audio_locations_mask & LE_AUDIO_LOCATION_MASK_RFU) != 0){
        return false;
    }
    if ((streamendpoint->available_audio_contexts_mask & LE_AUDIO_CONTEXT_MASK_RFU) != 0){
        return false;
    }
    if ((streamendpoint->supported_audio_contexts_mask & LE_AUDIO_CONTEXT_MASK_RFU) != 0){
        return false;
    }

    uint8_t i;

    for (i = 0; i < streamendpoint->records_num; i++){
        pacs_codec_capability_t * codec_capability = &streamendpoint->records[i].codec_capability;

        uint8_t codec_capability_type;
        for (codec_capability_type = (uint8_t)LE_AUDIO_CODEC_CONFIGURATION_TYPE_SAMPLING_FREQUENCY; codec_capability_type < (uint8_t) LE_AUDIO_CODEC_CAPABILITY_TYPE_RFU; codec_capability_type++){
            if ((codec_capability->codec_capability_mask & (1 << codec_capability_type) ) != 0 ){
     
            
                switch (codec_capability_type){
                    case LE_AUDIO_CODEC_CAPABILITY_TYPE_SAMPLING_FREQUENCY:
                    case LE_AUDIO_CODEC_CAPABILITY_TYPE_FRAME_DURATION:
                    case LE_AUDIO_CODEC_CAPABILITY_TYPE_SUPPORTED_AUDIO_CHANNEL_COUNTS:
                    case LE_AUDIO_CODEC_CAPABILITY_TYPE_OCTETS_PER_CODEC_FRAME:
                    case LE_AUDIO_CODEC_CAPABILITY_TYPE_CODEC_FRAME_BLOCKS_PER_SDU:
                        break;
                    default:
                        return false;
                }
            }
        }
    }
    return true;
}

void published_audio_capabilities_service_server_init(pacs_streamendpoint_t * sink_endpoint, pacs_streamendpoint_t * source_endpoint){
    // get service handle range
    uint16_t start_handle = 0;
    uint16_t end_handle   = 0xffff;
    int service_found = gatt_server_get_handle_range_for_service_with_uuid16(ORG_BLUETOOTH_SERVICE_PUBLISHED_AUDIO_CAPABILITIES_SERVICE, &start_handle, &end_handle);
    btstack_assert(service_found != 0);
    UNUSED(service_found);

    pacs_server_reset_values();

    btstack_assert((sink_endpoint != NULL) || (source_endpoint != NULL));

    if (sink_endpoint){
        btstack_assert((sink_endpoint->records != NULL) && (sink_endpoint->records_num != 0));
        btstack_assert(pacs_server_node_valid(sink_endpoint));
        pacs_sink_pac_records = sink_endpoint->records;
        pacs_sink_pac_records_num = sink_endpoint->records_num;
        pacs_sink_supported_audio_contexts_mask = sink_endpoint->supported_audio_contexts_mask;
        pacs_sink_available_audio_contexts_mask = sink_endpoint->available_audio_contexts_mask;
        pacs_sink_audio_locations = sink_endpoint->audio_locations_mask;
    }

    if (source_endpoint){
        btstack_assert((source_endpoint->records != NULL) && (source_endpoint->records_num != 0));
        btstack_assert(pacs_server_node_valid(source_endpoint));
        pacs_source_pac_records = source_endpoint->records;
        pacs_source_pac_records_num = source_endpoint->records_num;
        pacs_source_supported_audio_contexts_mask = source_endpoint->supported_audio_contexts_mask;
        pacs_source_available_audio_contexts_mask = source_endpoint->available_audio_contexts_mask;
        pacs_source_audio_locations = source_endpoint->audio_locations_mask;
    }

    pacs_sink_pac_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SINK_PAC);
    pacs_sink_pac_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SINK_PAC);

    pacs_sink_audio_locations_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SINK_AUDIO_LOCATIONS);
    pacs_sink_audio_locations_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SINK_AUDIO_LOCATIONS);

    pacs_source_pac_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SOURCE_PAC);
    pacs_source_pac_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SOURCE_PAC);

    pacs_source_audio_locations_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SOURCE_AUDIO_LOCATIONS);
    pacs_source_audio_locations_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SOURCE_AUDIO_LOCATIONS);

    pacs_available_audio_contexts_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_AVAILABLE_AUDIO_CONTEXTS);
    pacs_available_audio_contexts_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_AVAILABLE_AUDIO_CONTEXTS);

    pacs_supported_audio_contexts_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SUPPORTED_AUDIO_CONTEXTS);
    pacs_supported_audio_contexts_client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(start_handle, end_handle, ORG_BLUETOOTH_CHARACTERISTIC_SUPPORTED_AUDIO_CONTEXTS);

#ifdef ENABLE_TESTING_SUPPORT
    printf("PACS[sink %d, source %d] 0x%02x - 0x%02x \n", pacs_sink_pac_records_num, pacs_source_pac_records_num, start_handle, end_handle);
    printf("    pacs_sinc_pac                        0x%02x \n", pacs_sink_pac_handle);
    printf("    pacs_sink_pac CCC                    0x%02x \n", pacs_sink_pac_client_configuration_handle);
    
    printf("    pacs_sink_audio_locations            0x%02x \n", pacs_sink_audio_locations_handle);
    printf("    pacs_sink_audio_locations CCC        0x%02x \n", pacs_sink_audio_locations_client_configuration_handle);

    printf("    pacs_source_pac                      0x%02x \n", pacs_source_pac_handle);
    printf("    pacs_source_pac CCC                  0x%02x \n", pacs_source_pac_client_configuration_handle);

    printf("    pacs_source_audio_locations          0x%02x \n", pacs_source_audio_locations_handle);
    printf("    pacs_source_audio_locations CCC      0x%02x \n", pacs_source_audio_locations_client_configuration_handle);

    printf("    pacs_available_audio_contexts        0x%02x \n", pacs_available_audio_contexts_handle);
    printf("    pacs_available_audio_contexts CCC    0x%02x \n", pacs_available_audio_contexts_client_configuration_handle);

    printf("    pacs_supported_audio_contexts        0x%02x \n", pacs_supported_audio_contexts_handle);
    printf("    pacs_supported_audio_contexts CCC    0x%02x \n", pacs_supported_audio_contexts_client_configuration_handle);
#endif

    log_info("Found PACS service 0x%02x-0x%02x", start_handle, end_handle);

    // register service with ATT Server
    published_audio_capabilities_service.start_handle   = start_handle;
    published_audio_capabilities_service.end_handle     = end_handle;
    published_audio_capabilities_service.read_callback  = &pacs_server_read_callback;
    published_audio_capabilities_service.write_callback = &pacs_server_write_callback;
    published_audio_capabilities_service.packet_handler = pacs_server_packet_handler;
    att_server_register_service_handler(&published_audio_capabilities_service);
}

void published_audio_capabilities_service_server_register_packet_handler(btstack_packet_handler_t packet_handler){
    btstack_assert(packet_handler != NULL);
    pacs_server_event_callback = packet_handler;
}


void published_audio_capabilities_service_server_set_sink_audio_locations(uint32_t audio_locations_bitmap){
    btstack_assert((audio_locations_bitmap   & LE_AUDIO_LOCATION_MASK_RFU) == 0);

    pacs_sink_audio_locations = audio_locations_bitmap;
    pacs_server_set_callback(PACS_TASK_SEND_SINK_AUDIO_LOCATIONS);
}

void published_audio_capabilities_service_server_set_source_audio_locations(uint32_t audio_locations_bitmap){
    btstack_assert((audio_locations_bitmap & LE_AUDIO_LOCATION_MASK_RFU) == 0);
    
    pacs_source_audio_locations = audio_locations_bitmap;
    pacs_server_set_callback(PACS_TASK_SEND_SOURCE_AUDIO_LOCATIONS);
}

void published_audio_capabilities_service_server_set_available_audio_contexts(
    uint16_t available_sink_audio_contexts_bitmap, 
    uint16_t available_source_audio_contexts_bitmap){
    
    btstack_assert((available_sink_audio_contexts_bitmap   & LE_AUDIO_CONTEXT_MASK_RFU) == 0);
    btstack_assert((available_source_audio_contexts_bitmap & LE_AUDIO_CONTEXT_MASK_RFU) == 0);

    pacs_sink_available_audio_contexts_mask = available_sink_audio_contexts_bitmap;
    pacs_source_available_audio_contexts_mask = available_source_audio_contexts_bitmap;
    pacs_server_set_callback(PACS_TASK_SEND_AVAILABLE_AUDIO_CONTEXTS);
}

void published_audio_capabilities_service_server_set_supported_audio_contexts(
    uint16_t supported_sink_audio_contexts_bitmap,
    uint16_t supported_source_audio_contexts_bitmap){

    btstack_assert((supported_sink_audio_contexts_bitmap   & LE_AUDIO_CONTEXT_MASK_RFU) == 0);
    btstack_assert((supported_source_audio_contexts_bitmap & LE_AUDIO_CONTEXT_MASK_RFU) == 0);

    pacs_sink_supported_audio_contexts_mask = supported_sink_audio_contexts_bitmap;
    pacs_source_supported_audio_contexts_mask = supported_source_audio_contexts_bitmap;
    pacs_server_set_callback(PACS_TASK_SEND_SUPPORTED_AUDIO_CONTEXTS);
}

/**
 * @brief Trigger notification of Sink PAC record values.
 */
void published_audio_capabilities_service_server_sink_pac_modified(void){
    pacs_server_set_callback(PACS_TASK_SEND_SINK_PAC_RECORD);
}

/**
 * @brief Trigger notification of Source PAC record values.
 */
void published_audio_capabilities_service_server_source_pac_modified(void){
    pacs_server_set_callback(PACS_TASK_SEND_SOURCE_PAC_RECORD);
}
