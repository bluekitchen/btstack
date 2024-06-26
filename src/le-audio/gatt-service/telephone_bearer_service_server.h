/*
 * Copyright (C) 2024 BlueKitchen GmbH
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
 * @title Telephone Bearer Service Server
 * 
 */

#ifndef TELEPHONE_BEARER_SERVICE_H
#define TELEPHONE_BEARER_SERVICE_H

#include <stdint.h>
#include "le-audio/le_audio.h"
#include "le-audio/gatt-service/telephone_bearer_service_util.h"

#if defined __cplusplus
extern "C" {
#endif

/**
 * @text The Telephone Bearer Service Server exposes a telephone call control interface and 
 * telephone call control status for bearers on devices that can make and receive phone calls.
 * 
 * To use with your application, add '#import <telephone_bearer_service.gatt' or
 *  '#import <generic_telephone_bearer_service.gatt' to your .gatt file
 */

#define TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH                         (255-3)
#define TELEPHONE_BEARER_SERVICE_PROVIDER_NAME_MAX_LENGTH               (30)
#define TELEPHONE_BEARER_SERVICE_UNIFORM_CALLER_IDENTIFIER_MAX_LENGTH   (10)
#define TELEPHONE_BEARER_SERVICE_URI_SCHEMES_LIST_MAX_LENGTH            (30)
#define TELEPHONE_BEARER_SERVICE_CALL_CONTROL_POINT_NOTIFICATION_LENGTH (3)

typedef enum {
    TBS_CALL_STATE_INCOMMING = 0,
    TBS_CALL_STATE_DIALING,
    TBS_CALL_STATE_ALERTING,
    TBS_CALL_STATE_ACTIVE,
    TBS_CALL_STATE_LOCALLY_HELD,
    TBS_CALL_STATE_REMOTELY_HELD,
    TBS_CALL_STATE_LOCALLY_AND_REMOTELY_HELD,
    TBS_CALL_STATE_RFU
} tbs_call_state_t;

typedef enum {
    TBS_CALL_FLAG_DIRECTION           = 1,
    TBS_CALL_FLAG_SERVER_INFORMATION  = 2,
    TBS_CALL_FLAG_NETWORK_INFORMATION = 4
} tbs_call_flag_t;

typedef enum {
    TBS_STATUS_FLAG_INBAND_RINGTONE = 1,
    TBS_STATUS_FLAG_SILENT_MODE     = 2
} tbs_status_flag_t;

typedef struct {
    uint16_t  value_handle;
    uint16_t  client_configuration_handle;
} tbs_characteristic_t;


/* API_START */
typedef struct {
    btstack_linked_item_t item;

    uint8_t id;

    tbs_call_state_t state;
    tbs_call_flag_t  flags;
    char call_uri[TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH];
    char target_uri[TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH];
    char friendly_name[TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH];
    uint8_t termination_reason;

    uint32_t scheduled_tasks;
    uint32_t characteristics_dirty;
    bool deregister;
} tbs_call_data_t;

typedef struct {
    uint16_t optional_opcodes_supported_bitmap;
    uint16_t status_flags;
    uint8_t  content_control_id;
    char uri_schemes_list[TELEPHONE_BEARER_SERVICE_URI_SCHEMES_LIST_MAX_LENGTH];
    char provider_name[TELEPHONE_BEARER_SERVICE_PROVIDER_NAME_MAX_LENGTH];
    char uniform_caller_identifier[TELEPHONE_BEARER_SERVICE_UNIFORM_CALLER_IDENTIFIER_MAX_LENGTH];
    uint8_t technology;
    uint8_t signal_strength;
    uint8_t signal_strength_reporting_interval;
} tbs_data_t;

struct telephone_bearer_service_server;

typedef struct {
    hci_con_handle_t            con_handle;

    uint32_t        characteristics_dirty;
    uint16_t        client_configuration[TBS_CHARACTERISTICS_NUM];

    // Call Control Point Notification is response to write request by client
    uint8_t call_control_point_notification[TELEPHONE_BEARER_SERVICE_CALL_CONTROL_POINT_NOTIFICATION_LENGTH];

    uint32_t        scheduled_tasks;
} tbs_server_connection_t;

typedef struct telephone_bearer_service_server {
    btstack_linked_item_t item;

    uint16_t                    bearer_id;

    uint8_t connections_num;
    tbs_server_connection_t * connections;
    
    att_service_handler_t       service;
    tbs_characteristic_t        characteristics[TBS_CHARACTERISTICS_NUM];
    

    btstack_packet_handler_t event_callback;

    uint8_t call_counter;
    tbs_data_t  data;
    btstack_linked_list_t calls;

    btstack_timer_source_t signal_strength_timer;

    // notification scheduler
    uint32_t                                scheduled_tasks;
    tbs_characteristic_index_t              scheduled_characteristic_to_notify;
    tbs_call_data_t *                       scheduled_call_to_notify;
    uint8_t                                 scheduled_connection_to_notify;
    btstack_context_callback_registration_t scheduled_tasks_callback;

} telephone_bearer_service_server_t;


/**
 * @brief Init Telephone Bearer Service Server with ATT DB
 */
void telephone_bearer_service_server_init(void);

/**
 * Register a generic bearer
 *
 * @param bearer
 * @param packet_handler
 * @param connections_num
 * @param connections
 * @param optional_opcodes_supported_bitmap
 * @param out_bearer_id
 * @return
 */
uint8_t telephone_bearer_service_server_register_generic_bearer(
        telephone_bearer_service_server_t * bearer,
        btstack_packet_handler_t packet_handler,
        uint8_t connections_num, tbs_server_connection_t * connections,
        uint16_t optional_opcodes_supported_bitmap,
        uint16_t * out_bearer_id );
/**
 * Register a individual bearer
 *
 * @param bearer
 * @param packet_handler
 * @param connections_num
 * @param connections
 * @param optional_opcodes_supported_bitmask
 * @param out_bearer_id
 * @return
 */
uint8_t telephone_bearer_service_server_register_individual_bearer(
        telephone_bearer_service_server_t * bearer,
        btstack_packet_handler_t packet_handler,
        uint8_t connections_num, tbs_server_connection_t * connections,
        uint16_t optional_opcodes_supported_bitmask,
        uint16_t * out_bearer_id );
/**
 * Register a call to a bearer
 *
 * @param bearer_id
 * @param call
 * @param out_call_id
 * @return
 */
uint8_t telephone_bearer_service_server_register_call(
        uint16_t bearer_id,
        tbs_call_data_t *const call,
        uint16_t *const out_call_id );
/**
 * De-register a call from a bearer
 *
 * @param bearer_id
 * @param call_id
 * @return
 */
uint8_t telephone_bearer_service_server_deregister_call(
        uint16_t bearer_id,
        uint16_t call_id );
/**
 * Set the call state
 *
 * @param bearer_id
 * @param call_id
 * @param state
 * @return
 */
uint8_t telephone_bearer_service_server_set_call_state(
        uint16_t bearer_id,
        uint16_t call_id,
        const tbs_call_state_t state );
/**
 * Generate a call control point notification
 *
 * @param bearer_id
 * @param con_handle
 * @param call_id
 * @param opcode
 * @param result
 * @return
 */
uint8_t telephone_bearer_service_server_call_control_point_notification(
        hci_con_handle_t con_handle,
        uint16_t bearer_id,
        uint16_t call_id,
        tbs_control_point_opcode_t opcode,
        tbs_control_point_notification_result_codes_t result);
/**
 * Get the bearer struct corresponding to a given id
 *
 * @param id
 * @return
 */
telephone_bearer_service_server_t *telephone_bearer_service_server_get_bearer_by_id( uint16_t id );
/**
 * Get the call struct corresponding to a given id from a bearer
 *
 * @param bearer
 * @param id
 * @return
 */
tbs_call_data_t *telephone_bearer_service_server_get_call_by_id( telephone_bearer_service_server_t *bearer, uint16_t id );
/**
 * Set call termination reason
 *
 * @param bearer_id
 * @param call_id
 * @param reason
 * @return
 */
uint8_t telephone_bearer_service_server_termination_reason(uint16_t bearer_id, uint16_t call_id, tbs_call_termination_reason_t reason);
/**
 * Set call friendly name
 *
 * @param bearer_id
 * @param call_id
 * @param uri
 * @return
 */
uint8_t telephone_bearer_service_server_call_friendly_name( uint16_t bearer_id, uint16_t call_id, const char * uri );
/**
 * Set call uri
 *
 * @param bearer_id
 * @param call_id
 * @param uri
 * @return
 */
uint8_t telephone_bearer_service_server_call_uri( uint16_t bearer_id, uint16_t call_id, const char * uri );
/**
 * Set incoming call target bearer uri
 *
 * @param bearer_id
 * @param call_id
 * @param uri
 * @return
 */
uint8_t telephone_bearer_service_server_incoming_call_target_bearer_uri( uint16_t bearer_id, uint16_t call_id, const char * uri );
/**
 * Get the call state of a given call struct
 *
 * @param call
 * @return
 */
tbs_call_state_t telephone_bearer_service_server_get_call_state( tbs_call_data_t *call );
/**
 * Set bearer provider name
 *
 * @param bearer_id
 * @param provider_name
 * @return
 */
uint8_t telephone_bearer_service_server_set_provider_name(uint16_t bearer_id, const char * provider_name);
/**
 * Set bearer technology
 *
 * @param bearer_id
 * @param technology
 * @return
 */
uint8_t telephone_bearer_service_server_set_technology(uint16_t bearer_id, const tbs_technology_t technology);
/**
 * Set bearer supported schemes list
 *
 * @param bearer_id
 * @param schemes_list
 * @return
 */
uint8_t telephone_bearer_service_server_set_supported_schemes_list(uint16_t bearer_id, const char * schemes_list);
/**
 * Set signal strength
 *
 * @param bearer_id
 * @param signal_strength
 * @return
 */
uint8_t telephone_bearer_service_server_set_signal_strength(uint16_t bearer_id, const uint8_t signal_strength);
/**
 * Set bearer status flags
 *
 * @param bearer_id
 * @param flags
 * @return
 */
uint8_t telephone_bearer_service_server_set_status_flags(uint16_t bearer_id, const uint16_t flags);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

