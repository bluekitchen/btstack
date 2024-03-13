/*
 * Copyright (C) 2021 BlueKitchen GmbH
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
 * @title Media Control Service Server
 * 
 */

#ifndef OBJECT_TRANSFER_SERVICE_SERVER_H
#define OBJECT_TRANSFER_SERVICE_SERVER_H

#include <stdint.h>
#include "le-audio/le_audio.h"
#include "le-audio/gatt-service/object_transfer_service_util.h"


#if defined __cplusplus
extern "C" {
#endif

/* API_START */

typedef struct {
    bool (*find_object_with_name)(hci_con_handle_t con_handle, const char * name);
    ots_filter_t * (*get_filter)(hci_con_handle_t con_handle, uint8_t filter_index);
    bool (*can_allocate_object_of_size)(hci_con_handle_t con_handle, uint32_t object_size);

    oacp_result_code_t (*create_object)(hci_con_handle_t con_handle, uint32_t object_size, ots_object_type_t type_uuid16);
    oacp_result_code_t (*delete_object)(hci_con_handle_t con_handle);
    oacp_result_code_t (*calculate_checksum)(hci_con_handle_t con_handle, uint32_t offset, uint32_t length, uint32_t * crc_out);
    oacp_result_code_t (*execute)(hci_con_handle_t con_handle);
    oacp_result_code_t (*read) (hci_con_handle_t con_handle, uint32_t offset, uint32_t length, const uint8_t ** out_buffer);
    oacp_result_code_t (*write)(hci_con_handle_t con_handle, uint32_t offset, uint8_t *buffer, uint16_t buffer_size);
    oacp_result_code_t (*increase_allocated_size)(hci_con_handle_t con_handle, uint32_t allocated_size);
    oacp_result_code_t (*abort)(hci_con_handle_t con_handle);

    // View operations
    olcp_result_code_t (*first)(hci_con_handle_t con_handle);
    olcp_result_code_t (*last)(hci_con_handle_t con_handle);
    olcp_result_code_t (*next)(hci_con_handle_t con_handle);
    olcp_result_code_t (*previous)(hci_con_handle_t con_handle);
    olcp_result_code_t (*go_to)(hci_con_handle_t con_handle, ots_object_id_t * luid);
    olcp_result_code_t (*sort)(hci_con_handle_t con_handle,  olcp_list_sort_order_t order);
    olcp_result_code_t (*number_of_objects)(hci_con_handle_t con_handle, uint32_t * num_objects);
    olcp_result_code_t (*clear_marking)(hci_con_handle_t con_handle);
} ots_operations_t;

typedef struct {
    btstack_linked_item_t item;

    hci_con_handle_t            con_handle;

    // L2CAP data transfer channel
    uint8_t  receive_buffer[150];
    //uint16_t  receive_buffer_size;
    uint16_t initial_credits;
    uint16_t credit_based_cid;
    uint16_t remote_mtu;

    uint16_t oacp_configuration;
    uint16_t olcp_configuration;
    uint16_t object_changed_configuration;

    uint8_t                    scheduled_tasks;
    btstack_context_callback_registration_t scheduled_tasks_callback; 

    btstack_packet_handler_t event_callback;

    ots_object_t * current_object;
    bool current_object_locked;
    bool current_object_object_transfer_in_progress;
    bool current_object_object_read_transfer_in_progress;

    // used in write callback
    ots_filter_type_t long_write_filter_type;
    uint8_t           long_write_filter_index;
    uint8_t           long_write_data_length;
    uint8_t           long_write_data[OTS_MAX_STRING_LENGHT];
    uint16_t          long_write_attribute_handle;

    // used for OACP procedures
    oacp_opcode_t       oacp_opcode;
    oacp_result_code_t  oacp_result_code;
    uint32_t oacp_result_crc;
    uint32_t oacp_data_chunk_offset;
    uint32_t oacp_data_chunk_length;
    uint32_t oacp_write_offset;
    uint32_t oacp_data_bytes_read;
    bool oacp_abort_read;

    // used for OBJECT CHANGED indication
    uint8_t change_flags; // see ots_object_changed_flag_t

    bool oacp_truncate;

    // used for OLCP procedures
    olcp_opcode_t       olcp_opcode;
    olcp_result_code_t  olcp_result_code;
    uint32_t            olcp_result_num_objects;

    btstack_timer_source_t operation_timer;
} ots_server_connection_t;


/*
 * @brief Init Object Transfer Service Server with ATT DB
 */
uint8_t object_transfer_service_server_init(uint32_t oacp_features, uint32_t olcp_features, 
    uint8_t connections_num, ots_server_connection_t * connections, 
    const ots_operations_t * ots_operations);

void object_transfer_service_server_register_packet_handler(btstack_packet_handler_t packet_handler);

uint8_t object_transfer_service_server_set_current_object(hci_con_handle_t con_handle, ots_object_t * object);

uint8_t object_transfer_service_server_update_current_object_name(hci_con_handle_t con_handle, char * name);

uint8_t object_transfer_service_server_delete_current_object(hci_con_handle_t con_handle);
uint8_t object_transfer_service_server_update_current_object_filter(hci_con_handle_t con_handle, uint8_t filter_index, ots_filter_t * filter);

bool object_transfer_service_server_current_object_valid(hci_con_handle_t con_handle);
bool object_transfer_service_server_current_object_procedure_permitted(hci_con_handle_t con_handle, uint32_t object_property_mask);

bool object_transfer_service_server_current_object_locked(hci_con_handle_t con_handle);
bool object_transfer_service_server_current_object_transfer_in_progress(hci_con_handle_t con_handle);

uint8_t object_transfer_service_server_current_object_set_lock(hci_con_handle_t con_handle, bool locked);
uint8_t object_transfer_service_server_current_object_set_transfer_in_progress(hci_con_handle_t con_handle, bool transfer_in_progress);

uint32_t object_transfer_service_server_current_object_size(hci_con_handle_t con_handle);
uint8_t object_transfer_service_server_current_object_increase_allocated_size(hci_con_handle_t con_handle, uint32_t size);
uint32_t object_transfer_service_server_current_object_allocated_size(hci_con_handle_t con_handle);
char * object_transfer_service_server_current_object_name(hci_con_handle_t con_handle);
uint16_t object_transfer_service_server_get_cbm_channel_remote_mtu(hci_con_handle_t con_handle);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

