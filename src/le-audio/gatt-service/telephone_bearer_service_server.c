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

#define BTSTACK_FILE__ "telephone_bearer_service_server.c"

/**
 * Implementation of the GATT Telephone Bearer Service Server
 * To use with your application, add '#import <telephone_bearer_service.gatt' or '#import <generic_telephone_bearer_service.gatt' to your .gatt file
 */

#ifdef ENABLE_TESTING_SUPPORT
#include <stdio.h>
#endif

#include "ble/att_db.h"
#include "ble/att_server.h"
#include "bluetooth_gatt.h"
#include "btstack_debug.h"
#include "btstack_defines.h"
#include "btstack_event.h"
#include "btstack_util.h"

#include "le-audio/gatt-service/telephone_bearer_service_util.h"
#include "le-audio/gatt-service/telephone_bearer_service_server.h"

#include "le-audio/le_audio_util.h"

#define TELEPHONE_BEARER_SERVICE_BUFFER_SIZE    (200)

typedef enum {
	HANDLE_TYPE_CHARACTERISTIC_VALUE = 0,
	HANDLE_TYPE_CHARACTERISTIC_CCD,
} handle_type_t;

static btstack_linked_list_t    tbs_bearers;
static uint16_t tbs_bearer_counter = 0;
static uint16_t tbs_services_start_handle;
static uint8_t buf[TELEPHONE_BEARER_SERVICE_BUFFER_SIZE];

telephone_bearer_service_server_t *telephone_bearer_service_server_get_bearer_by_id(uint16_t bearer_id) {
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &tbs_bearers);
    while (btstack_linked_list_iterator_has_next(&it)){
        telephone_bearer_service_server_t * tbs_bearer = (telephone_bearer_service_server_t *)btstack_linked_list_iterator_next(&it);
        if (bearer_id == tbs_bearer->bearer_id){
            return tbs_bearer;
        } 
    }
    return NULL;
}

tbs_call_data_t *telephone_bearer_service_server_get_call_by_id( telephone_bearer_service_server_t *bearer, uint16_t call_id ) {
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &bearer->calls);
    while (btstack_linked_list_iterator_has_next(&it)){
        tbs_call_data_t * call = (tbs_call_data_t *)btstack_linked_list_iterator_next(&it);
        if (call_id == call->id){
            return call;
        }
    }
    return NULL;
}

static uint16_t tbs_server_get_next_bearer_id(void){
	tbs_bearer_counter = btstack_next_cid_ignoring_zero(tbs_bearer_counter);
    return tbs_bearer_counter;
}

uint8_t btstack_next_byte_cid_ignoring_zero(uint8_t current_cid){
    uint8_t next_cid;
    if (current_cid == 0xff) {
        next_cid = 1;
    } else {
        next_cid = current_cid + 1;
    }
    return next_cid;
}

static uint16_t tbs_server_get_next_call_id( telephone_bearer_service_server_t *tbs_bearer ) {
    tbs_bearer->call_counter = btstack_next_byte_cid_ignoring_zero(tbs_bearer->call_counter);
    return tbs_bearer->call_counter;
}

static void tbs_server_schedule_task(telephone_bearer_service_server_t * tbs_bearer, tbs_characteristic_index_t characteristic_index);
static void tbs_server_check_for_deregister_call( telephone_bearer_service_server_t *bearer );

static void tbs_server_reset_bearer(telephone_bearer_service_server_t * tbs_bearer){
    tbs_bearer->con_handle = HCI_CON_HANDLE_INVALID;
    for( int i=0; i<TBS_CHARACTERISTICS_NUM; ++i ) {
        tbs_bearer->characteristics[i].client_configuration = 0;
    }
    tbs_bearer->characteristics_dirty = 0;
    tbs_bearer->scheduled_tasks = 0;
    btstack_run_loop_remove_timer(&tbs_bearer->signal_strength_timer);
}

static telephone_bearer_service_server_t * tbs_server_get_registered_bearer(uint16_t start_handle){
	btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &tbs_bearers);
    while (btstack_linked_list_iterator_has_next(&it)){
        telephone_bearer_service_server_t * tbs_bearer = (telephone_bearer_service_server_t *)btstack_linked_list_iterator_next(&it);
        if (tbs_bearer->service.start_handle != start_handle) continue;
        return tbs_bearer;
    }
    return NULL;
}

static telephone_bearer_service_server_t * tbs_server_find_bearer_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &tbs_bearers);
    while (btstack_linked_list_iterator_has_next(&it)){
        telephone_bearer_service_server_t * tbs_bearer = (telephone_bearer_service_server_t *)btstack_linked_list_iterator_next(&it);
        if (con_handle == tbs_bearer->con_handle){
        	return tbs_bearer;
        } 
    }
    return NULL;
}

static telephone_bearer_service_server_t * tbs_server_find_bearer_for_attribute_handle(uint16_t attribute_handle){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &tbs_bearers);
    while (btstack_linked_list_iterator_has_next(&it)){
        telephone_bearer_service_server_t * tbs_bearer = (telephone_bearer_service_server_t *)btstack_linked_list_iterator_next(&it);
        if ((attribute_handle > tbs_bearer->service.start_handle) && (attribute_handle <= tbs_bearer->service.end_handle)){
            return tbs_bearer;
        } 
    }
    return NULL;
}

static tbs_characteristic_index_t tbs_server_find_characteristic_index_for_attribute_handle(telephone_bearer_service_server_t * tbs_bearer, uint16_t attribute_handle, handle_type_t * type){
    uint8_t i;
    for (i = 0; i < TBS_CHARACTERISTICS_NUM; i++){
        if (attribute_handle == tbs_bearer->characteristics[i].client_configuration_handle){
            *type = HANDLE_TYPE_CHARACTERISTIC_CCD;
            return (tbs_characteristic_index_t)i;
        }
        if (attribute_handle == tbs_bearer->characteristics[i].value_handle){
            *type = HANDLE_TYPE_CHARACTERISTIC_VALUE;
            return (tbs_characteristic_index_t)i;
        }
    }
    btstack_assert(false);
    return -1;
}

static void tbs_server_set_con_handle(telephone_bearer_service_server_t * tbs_bearer, uint16_t characteristic_index, hci_con_handle_t con_handle, uint16_t configuration){
    tbs_bearer->characteristics[characteristic_index].client_configuration = configuration;
    if (configuration == 0){
        tbs_bearer->con_handle = HCI_CON_HANDLE_INVALID;
    } else {
        tbs_bearer->con_handle = con_handle;
    }
}

static void tbs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
	UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET){
        return;
    }

    hci_con_handle_t con_handle;
    telephone_bearer_service_server_t * tbs_bearer;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            con_handle = hci_event_disconnection_complete_get_connection_handle(packet);
            tbs_bearer = tbs_server_find_bearer_for_con_handle(con_handle);
            if (tbs_bearer != NULL){
                tbs_server_reset_bearer(tbs_bearer);
            }
            break;
        default:
            break;
    }
}

static uint16_t tbs_server_max_att_value_len(hci_con_handle_t con_handle){
    uint16_t att_mtu = att_server_get_mtu(con_handle);
    return (att_mtu >= 3) ? (att_mtu - 3) : 0;
}

static uint16_t tbs_server_max_value_len(hci_con_handle_t con_handle, uint16_t value_len){
    return btstack_min(tbs_server_max_att_value_len(con_handle), value_len);
}

typedef struct {
    uint16_t field_offset;
    uint8_t *buf;
    uint16_t buf_size;
    uint16_t buf_offset;
} memcat_t;

static uint16_t memcat( memcat_t *me, const uint8_t *data, uint16_t len ) {
    uint16_t stored_bytes = le_audio_util_virtual_memcpy_helper(data, len, me->field_offset, me->buf, me->buf_size, me->buf_offset );
    me->field_offset += len;
    return stored_bytes;
}

static uint16_t memcat_byte( memcat_t *me, uint8_t value ) {
    uint16_t stored_bytes = le_audio_util_virtual_memcpy_helper(&value, 1, me->field_offset, me->buf, me->buf_size, me->buf_offset );
    me->field_offset += 1;
    return stored_bytes;
}

btstack_linked_list_iterator_t bearer_calls_iterator_begin( telephone_bearer_service_server_t *bearer ) {
    btstack_assert( bearer != NULL );
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &bearer->calls);
    return it;
}

static uint16_t tbs_server_serialize_current_call_list( memcat_t *storage, telephone_bearer_service_server_t *bearer ) {
    uint16_t stored_bytes = 0;
    for( btstack_linked_list_iterator_t it = bearer_calls_iterator_begin(bearer);
         btstack_linked_list_iterator_has_next(&it);
         )
    {
        tbs_call_data_t * call = (tbs_call_data_t *)btstack_linked_list_iterator_next(&it);
        const char * string = call->call_uri;
        uint16_t uri_length = (uint16_t) strnlen(string, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH);
        uint8_t item_length = uri_length + 3;

        stored_bytes += memcat_byte( storage, item_length );
        stored_bytes += memcat_byte( storage, call->id );
        stored_bytes += memcat_byte( storage, call->state );
        stored_bytes += memcat_byte( storage, call->flags );
        stored_bytes += memcat( storage, (const uint8_t *) string, uri_length );
    }
    return stored_bytes;
}

static uint16_t tbs_server_serialize_call_state( memcat_t *storage, telephone_bearer_service_server_t *bearer ) {
    uint16_t stored_bytes = 0;
    for( btstack_linked_list_iterator_t it = bearer_calls_iterator_begin(bearer);
         btstack_linked_list_iterator_has_next(&it);
         )
    {
        tbs_call_data_t * call = (tbs_call_data_t *)btstack_linked_list_iterator_next(&it);
        stored_bytes += memcat_byte( storage, call->id );
        stored_bytes += memcat_byte( storage, call->state );
        stored_bytes += memcat_byte( storage, call->flags );
    }
    return stored_bytes;
}

static uint16_t tbs_server_serialize_incoming_call_target_bearer_uri( memcat_t *storage, telephone_bearer_service_server_t *bearer ) {
    uint16_t stored_bytes = 0;
    uint32_t index_mask = UINT32_C(1)<<TBS_CHARACTERISTIC_INDEX_INCOMING_CALL_TARGET_BEARER_URI;
    for( btstack_linked_list_iterator_t it = bearer_calls_iterator_begin(bearer);
         btstack_linked_list_iterator_has_next(&it);
         )
    {
        tbs_call_data_t * call = (tbs_call_data_t *)btstack_linked_list_iterator_next(&it);
        const char *string = call->target_uri;
        uint16_t uri_length = (uint16_t) strnlen(string, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH);
        if( (call->scheduled_tasks & index_mask) > 0 ) {
            call->scheduled_tasks &= ~index_mask;
            stored_bytes += memcat_byte( storage, call->id );
            stored_bytes += memcat( storage, (const uint8_t *) string, uri_length );
            break;
        }
    }
    return stored_bytes;
}

static uint16_t tbs_server_serialize_first_incoming_call_target_bearer_uri( memcat_t *storage, telephone_bearer_service_server_t *bearer ) {
    uint16_t stored_bytes = 0;
    for( btstack_linked_list_iterator_t it = bearer_calls_iterator_begin(bearer);
         btstack_linked_list_iterator_has_next(&it);
         )
    {
        tbs_call_data_t * call = (tbs_call_data_t *)btstack_linked_list_iterator_next(&it);
        const char *string = call->target_uri;
        uint16_t uri_length = (uint16_t) strnlen(string, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH);
        if( uri_length > 0 ) {
            stored_bytes += memcat_byte( storage, call->id );
            stored_bytes += memcat( storage, (const uint8_t *) string, uri_length );
            break;
        }
    }
    return stored_bytes;
}

static uint16_t tbs_server_serialize_incoming_call( memcat_t *storage, telephone_bearer_service_server_t *bearer ) {
    uint16_t stored_bytes = 0;
    uint32_t index_mask = UINT32_C(1)<<TBS_CHARACTERISTIC_INDEX_INCOMING_CALL;
    for( btstack_linked_list_iterator_t it = bearer_calls_iterator_begin(bearer);
         btstack_linked_list_iterator_has_next(&it);
         )
    {
        tbs_call_data_t * call = (tbs_call_data_t *)btstack_linked_list_iterator_next(&it);
        const char *string = call->call_uri;
        uint16_t uri_length = (uint16_t) strnlen(string, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH);
        if( (call->scheduled_tasks & index_mask) > 0 ) {
            call->scheduled_tasks &= ~index_mask;

            stored_bytes += memcat_byte( storage, call->id );
            stored_bytes += memcat( storage, (const uint8_t *) string, uri_length );
            break;
        }
    }
    return stored_bytes;
}

static uint16_t tbs_server_serialize_first_incoming_call( memcat_t *storage, telephone_bearer_service_server_t *bearer ) {
    uint16_t stored_bytes = 0;
    for( btstack_linked_list_iterator_t it = bearer_calls_iterator_begin(bearer);
         btstack_linked_list_iterator_has_next(&it);
         )
    {
        tbs_call_data_t * call = (tbs_call_data_t *)btstack_linked_list_iterator_next(&it);
        const char * string = call->call_uri;
        uint16_t uri_length = (uint16_t) strnlen(string, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH);
        if( uri_length > 0 ) {
            stored_bytes += memcat_byte( storage, call->id );
            stored_bytes += memcat( storage, (const uint8_t *) string, uri_length );
            break;
        }
    }
    return stored_bytes;
}

static uint16_t tbs_server_serialize_call_friendly_name( memcat_t *storage, telephone_bearer_service_server_t *bearer ) {
    uint16_t stored_bytes = 0;
    uint32_t index_mask = UINT32_C(1)<<TBS_CHARACTERISTIC_INDEX_CALL_FRIENDLY_NAME;
    for( btstack_linked_list_iterator_t it = bearer_calls_iterator_begin(bearer);
         btstack_linked_list_iterator_has_next(&it);
         )
    {
        tbs_call_data_t * call = (tbs_call_data_t *)btstack_linked_list_iterator_next(&it);
        const char *string = call->friendly_name;
        uint16_t uri_length = (uint16_t) strnlen(string, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH);
        if( (call->scheduled_tasks & index_mask) > 0 ) {
            call->scheduled_tasks &= ~index_mask;
            stored_bytes += memcat_byte( storage, call->id );
            stored_bytes += memcat( storage, (const uint8_t *) string, uri_length );
            break;
        }
    }
    return stored_bytes;
}

static uint16_t tbs_server_serialize_first_call_friendly_name( memcat_t *storage, telephone_bearer_service_server_t *bearer ) {
    uint16_t stored_bytes = 0;
    for( btstack_linked_list_iterator_t it = bearer_calls_iterator_begin(bearer);
         btstack_linked_list_iterator_has_next(&it);
         )
    {
        tbs_call_data_t * call = (tbs_call_data_t *)btstack_linked_list_iterator_next(&it);
        const char * string = call->friendly_name;
        uint16_t uri_length = (uint16_t) strnlen(string, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH);
        if( uri_length > 0 ) {
            stored_bytes += memcat_byte( storage, call->id );
            stored_bytes += memcat( storage, (const uint8_t *) string, uri_length );
            break;
        }
    }
    return stored_bytes;
}

static uint16_t tbs_server_serialize_call_control_point_notification( memcat_t *storage, telephone_bearer_service_server_t *bearer ) {
    uint16_t stored_bytes = 0;

    // if we have a notification without a valid call_id it is stored in the bearer
    if( bearer->data.call_control_point_notification_pending ) {
        stored_bytes += memcat( storage, bearer->data.call_control_point_notification, sizeof(bearer->data.call_control_point_notification) );
        bearer->data.call_control_point_notification_pending = false;
        return stored_bytes;
    }

    uint32_t index_mask = UINT32_C(1)<<TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT;
    for( btstack_linked_list_iterator_t it = bearer_calls_iterator_begin(bearer);
         btstack_linked_list_iterator_has_next(&it);
         )
    {
        tbs_call_data_t * call = (tbs_call_data_t *)btstack_linked_list_iterator_next(&it);
        if( (call->scheduled_tasks & index_mask) > 0 ) {
            call->scheduled_tasks &= ~index_mask;
            stored_bytes += memcat( storage, call->call_control_point_notification, sizeof(call->call_control_point_notification) );
            break;
        }
    }
    return stored_bytes;
}

static uint16_t tbs_server_serialize_termination_reason( memcat_t *storage, telephone_bearer_service_server_t *bearer ) {
    uint16_t stored_bytes = 0;
    uint32_t index_mask = UINT32_C(1)<<TBS_CHARACTERISTIC_INDEX_TERMINATION_REASON;
    for( btstack_linked_list_iterator_t it = bearer_calls_iterator_begin(bearer);
         btstack_linked_list_iterator_has_next(&it);
         )
    {
        tbs_call_data_t * call = (tbs_call_data_t *)btstack_linked_list_iterator_next(&it);
        if( (call->scheduled_tasks & index_mask) > 0 ) {
            call->scheduled_tasks &= ~index_mask;
            stored_bytes += memcat_byte( storage, call->id );
            stored_bytes += memcat_byte( storage, call->termination_reason );
            break;
        }
    }
    return stored_bytes;
}

static uint32_t tbs_server_merge_calls_scheduled_tasks( telephone_bearer_service_server_t *tbs_bearer, uint32_t tasks ) {
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &tbs_bearer->calls);
    while(btstack_linked_list_iterator_has_next(&it)){
        tbs_call_data_t * call = (tbs_call_data_t *)btstack_linked_list_iterator_next(&it);
        tasks |= call->scheduled_tasks;
    }
    return tasks;
}

// warning, value needs to be greater than zero otherwise behavior is undefined!
static uint8_t tbs_server_mask_to_index( uint32_t value ) {
    return 31-btstack_clz(value);
}

static void tbs_server_can_send_now(void * context){
    telephone_bearer_service_server_t * bearer = (telephone_bearer_service_server_t *) context;

    if (bearer->con_handle == HCI_CON_HANDLE_INVALID){
        bearer->scheduled_tasks = 0;
        return;
    }

    uint32_t scheduled_tasks = bearer->scheduled_tasks;
    // nothing to do here
    if( scheduled_tasks == 0 ) {
        return;
    }

    memcat_t storage = {
            .buf = buf,
            .buf_size = TELEPHONE_BEARER_SERVICE_BUFFER_SIZE,
    };

    uint8_t index = tbs_server_mask_to_index(scheduled_tasks);
    uint16_t value_handle = bearer->characteristics[index].value_handle;
    hci_con_handle_t con_handle = bearer->con_handle;

    switch( index ) {
    case TBS_CHARACTERISTIC_INDEX_BEARER_PROVIDER_NAME:
        att_server_notify(con_handle,
            value_handle,
            (uint8_t *)bearer->data.provider_name,
            tbs_server_max_value_len(con_handle, strlen(bearer->data.provider_name)));
        break;
    case TBS_CHARACTERISTIC_INDEX_BEARER_UCI:
        break;
    case TBS_CHARACTERISTIC_INDEX_BEARER_TECHNOLOGY: {
        att_server_notify(con_handle,
            value_handle,
            &bearer->data.technology,
            tbs_server_max_value_len(con_handle, sizeof(bearer->data.technology)));
        break;
    }
    case TBS_CHARACTERISTIC_INDEX_BEARER_URI_SCHEMES_SUPPORTED_LIST:
        att_server_notify(con_handle,
            value_handle,
            (uint8_t*)&bearer->data.uri_schemes_list,
            tbs_server_max_value_len(con_handle, sizeof(bearer->data.uri_schemes_list)));
        break;
    case TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH:
        att_server_notify(con_handle,
            value_handle,
            &bearer->data.signal_strength,
            tbs_server_max_value_len(con_handle, sizeof(bearer->data.signal_strength)));
        break;
    case TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL:
        att_server_notify(con_handle,
            value_handle,
            &bearer->data.signal_strength_reporting_interval,
            tbs_server_max_value_len(con_handle, sizeof(bearer->data.signal_strength_reporting_interval)));
        break;
    case TBS_CHARACTERISTIC_INDEX_BEARER_LIST_CURRENT_CALLS: {
        uint16_t stored_bytes = tbs_server_serialize_current_call_list( &storage, bearer );
        att_server_notify(con_handle,
                value_handle,
                buf,
                tbs_server_max_value_len(con_handle, stored_bytes));
        break;
    }
    case TBS_CHARACTERISTIC_INDEX_CONTENT_CONTROL_ID:
        break;
    case TBS_CHARACTERISTIC_INDEX_STATUS_FLAGS:
        att_server_notify(con_handle,
            value_handle,
            (uint8_t*)&(bearer->data.status_flags),
            tbs_server_max_value_len(con_handle, sizeof(bearer->data.status_flags)));
        break;
    case TBS_CHARACTERISTIC_INDEX_INCOMING_CALL_TARGET_BEARER_URI: {
        uint16_t stored_bytes = tbs_server_serialize_incoming_call_target_bearer_uri( &storage, bearer );
        att_server_notify(con_handle,
                value_handle,
                buf,
                tbs_server_max_value_len(con_handle, stored_bytes));
        break;
    }
    case TBS_CHARACTERISTIC_INDEX_CALL_STATE: {
        uint16_t stored_bytes = tbs_server_serialize_call_state( &storage, bearer );
        att_server_notify(con_handle,
                value_handle,
                buf,
                tbs_server_max_value_len(con_handle, stored_bytes));
        break;
    }
    case TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT: {
        uint16_t stored_bytes = tbs_server_serialize_call_control_point_notification( &storage, bearer );
        att_server_notify(con_handle,
                value_handle,
                buf,
                tbs_server_max_value_len(con_handle, stored_bytes));
        break;
    }
    case TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT_OPTIONAL_OPCODES:
        break;
    case TBS_CHARACTERISTIC_INDEX_TERMINATION_REASON: {
        uint16_t stored_bytes = tbs_server_serialize_termination_reason( &storage, bearer );
        att_server_notify(con_handle,
                value_handle,
                buf,
                tbs_server_max_value_len(con_handle, stored_bytes));
        break;
    }
    case TBS_CHARACTERISTIC_INDEX_INCOMING_CALL: {
        uint16_t stored_bytes = tbs_server_serialize_incoming_call( &storage, bearer );
        att_server_notify(con_handle,
                value_handle,
                buf,
                tbs_server_max_value_len(con_handle, stored_bytes));
        break;
    }
    case TBS_CHARACTERISTIC_INDEX_CALL_FRIENDLY_NAME: {
        uint16_t stored_bytes = tbs_server_serialize_call_friendly_name( &storage, bearer );
        att_server_notify(con_handle,
                value_handle,
                buf,
                tbs_server_max_value_len(con_handle, stored_bytes));
        break;
    }
    default:
        break;
    }
    scheduled_tasks &= ~(UINT32_C(1)<<index);
    scheduled_tasks = tbs_server_merge_calls_scheduled_tasks(bearer, scheduled_tasks);

    bearer->scheduled_tasks = scheduled_tasks;
    tbs_server_check_for_deregister_call(bearer);
    if (bearer->scheduled_tasks != 0){
        att_server_register_can_send_now_callback(&bearer->scheduled_tasks_callback, bearer->con_handle);
    }
}

static void tbs_server_schedule_task(telephone_bearer_service_server_t * tbs_bearer, tbs_characteristic_index_t characteristic_index){
    btstack_assert( characteristic_index < TBS_CHARACTERISTICS_NUM );
    if (tbs_bearer->characteristics[characteristic_index].client_configuration == 0){
        return;
    }

    if (tbs_bearer->con_handle == HCI_CON_HANDLE_INVALID){
        return;
    }

    uint32_t task_bit_mask = UINT32_C(1) << ((uint8_t)characteristic_index);

    // skip if already scheduled
    if ((tbs_bearer->scheduled_tasks & task_bit_mask) != 0){
        return;
    }

    uint32_t scheduled_tasks = tbs_bearer->scheduled_tasks;
    tbs_bearer->scheduled_tasks |= task_bit_mask;

    // flag characteristic as dirty
    tbs_bearer->characteristics_dirty |= UINT32_C(1) << characteristic_index;

    if( scheduled_tasks == 0 ) {
        tbs_bearer->scheduled_tasks_callback.callback = &tbs_server_can_send_now;
        tbs_bearer->scheduled_tasks_callback.context  = (void*) tbs_bearer;
        att_server_register_can_send_now_callback(&tbs_bearer->scheduled_tasks_callback, tbs_bearer->con_handle);
    }
}

static void tbs_server_call_schedule_task(tbs_call_data_t *call, tbs_characteristic_index_t characteristic_index){
    btstack_assert( characteristic_index < TBS_CHARACTERISTICS_NUM );

    uint32_t task_bit_mask = UINT32_C(1) << ((uint8_t)characteristic_index);

    call->scheduled_tasks |= task_bit_mask;
}

static void tbs_server_emit_call_deregister_done(telephone_bearer_service_server_t * tbs_bearer, uint8_t call_id){
    btstack_assert(tbs_bearer->event_callback != NULL);

    uint8_t event[8];
    memset(event, 0, sizeof(event));

    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = GATTSERVICE_SUBEVENT_TBS_SERVER_CALL_DEREGISTER_DONE;
    little_endian_store_16(event, pos, tbs_bearer->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, tbs_bearer->bearer_id);
    pos += 2;
    event[pos++] = call_id;

    (*tbs_bearer->event_callback)(HCI_EVENT_PACKET, 0, event, pos);
}

static void tbs_server_emit_call_control_notification_task(telephone_bearer_service_server_t * tbs_bearer, tbs_control_point_opcode_t opcode, uint8_t * buffer, uint16_t buffer_size){
    btstack_assert( tbs_bearer->event_callback != NULL );
    btstack_assert( buffer_size <= TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH );
    uint8_t event[TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH] = { 0 };

    uint8_t pos = 0;
    event[pos++] = HCI_EVENT_GATTSERVICE_META;
    pos++; // placeholder for event size, overwritten later
    event[pos++] = GATTSERVICE_SUBEVENT_TBS_SERVER_CALL_CONTROL_POINT_NOTIFICATION_TASK;
    little_endian_store_16(event, pos, tbs_bearer->con_handle);
    pos += 2;
    little_endian_store_16(event, pos, tbs_bearer->bearer_id);
    pos += 2;

    uint16_t data_size = btstack_min(TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH-pos, buffer_size);
    uint8_t event_size = pos+data_size;
    memcpy(&event[pos], buffer, data_size);

    event[1] = event_size - 2; // patch up event size
    (*tbs_bearer->event_callback)(HCI_EVENT_PACKET, 0, event, event_size);
}

static uint16_t tbs_server_read_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    telephone_bearer_service_server_t * bearer = tbs_server_find_bearer_for_attribute_handle(attribute_handle);
    if (bearer == NULL) {
        return 0;
    }

    handle_type_t type = -1;
    tbs_characteristic_index_t characteristic_index = tbs_server_find_characteristic_index_for_attribute_handle(bearer, attribute_handle, &type);
    uint32_t characteristic_mask = UINT32_C(1) << characteristic_index;

    switch (type){
        case HANDLE_TYPE_CHARACTERISTIC_CCD:
            return att_read_callback_handle_little_endian_16(bearer->characteristics[characteristic_index].client_configuration, offset, buffer, buffer_size);
        default:
            break;
    }

    if (buffer == NULL) {
        // get len and check if we have up to date value
        if ((offset > 0) && ((bearer->characteristics_dirty & characteristic_mask)>0)){
            return ATT_READ_ERROR_CODE_OFFSET + TBS_NAME_ATT_ERROR_RESPONSE_VALUE_CHANGED_DURING_READ_LONG;
        }
    } else {
        // actual readl (after everything was validated)
        if (offset == 0) {
            bearer->characteristics_dirty &= ~characteristic_mask;
        }
    }

    memcat_t storage = {
            .buf = buffer,
            .buf_size = buffer_size,
            .buf_offset = offset,
    };

	switch (characteristic_index){
        case TBS_CHARACTERISTIC_INDEX_BEARER_PROVIDER_NAME:
            return att_read_callback_handle_blob((const uint8_t *)bearer->data.provider_name, strlen(bearer->data.provider_name), offset, buffer, buffer_size);

        case TBS_CHARACTERISTIC_INDEX_BEARER_UCI:
            break;

        case TBS_CHARACTERISTIC_INDEX_BEARER_TECHNOLOGY:
            return att_read_callback_handle_byte(bearer->data.technology, offset, buffer, buffer_size);

        case TBS_CHARACTERISTIC_INDEX_BEARER_URI_SCHEMES_SUPPORTED_LIST:
            return att_read_callback_handle_blob((const uint8_t *)bearer->data.uri_schemes_list, strlen(bearer->data.uri_schemes_list), offset, buffer, buffer_size);

        case TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH:
            return att_read_callback_handle_byte(bearer->data.signal_strength, offset, buffer, buffer_size);

        case TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL:
            return att_read_callback_handle_byte(bearer->data.signal_strength_reporting_interval, offset, buffer, buffer_size);

        case TBS_CHARACTERISTIC_INDEX_BEARER_LIST_CURRENT_CALLS:
            return tbs_server_serialize_current_call_list( &storage, bearer );

        case TBS_CHARACTERISTIC_INDEX_CONTENT_CONTROL_ID:
            return att_read_callback_handle_byte(bearer->data.content_control_id, offset, buffer, buffer_size);

        case TBS_CHARACTERISTIC_INDEX_STATUS_FLAGS:
            return att_read_callback_handle_little_endian_16(bearer->data.status_flags, offset, buffer, buffer_size);

        case TBS_CHARACTERISTIC_INDEX_INCOMING_CALL_TARGET_BEARER_URI:
            return tbs_server_serialize_first_incoming_call_target_bearer_uri( &storage, bearer );

        case TBS_CHARACTERISTIC_INDEX_CALL_STATE:
            return tbs_server_serialize_call_state( &storage, bearer );

        case TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT_OPTIONAL_OPCODES:
            return att_read_callback_handle_little_endian_16(bearer->data.optional_opcodes_supported_bitmap, offset, buffer, buffer_size);

        case TBS_CHARACTERISTIC_INDEX_INCOMING_CALL:
            return tbs_server_serialize_first_incoming_call( &storage, bearer );

        case TBS_CHARACTERISTIC_INDEX_CALL_FRIENDLY_NAME:
            return tbs_server_serialize_first_call_friendly_name( &storage, bearer );

        default:
            break;
	}
	return 0;
}


static int tbs_server_write_callback(hci_con_handle_t con_handle, uint16_t attribute_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    telephone_bearer_service_server_t * tbs_bearer = tbs_server_find_bearer_for_attribute_handle(attribute_handle);
    if (tbs_bearer == NULL) {
        return 0;
    }

    handle_type_t type = -1;
    tbs_characteristic_index_t characteristic_index = tbs_server_find_characteristic_index_for_attribute_handle(tbs_bearer, attribute_handle, &type);
    switch (type){
        case HANDLE_TYPE_CHARACTERISTIC_CCD:
            tbs_server_set_con_handle(tbs_bearer, (uint16_t)characteristic_index, con_handle, little_endian_read_16(buffer, 0));
            return 0;
        default:
            break;
    }

    switch (characteristic_index){
        case TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL:
            tbs_bearer->data.signal_strength_reporting_interval = buffer[0];
            btstack_run_loop_remove_timer(&tbs_bearer->signal_strength_timer);
            if( tbs_bearer->data.signal_strength_reporting_interval == 0 ) {
                break;
            }
            // reporting interval is in seconds
            btstack_run_loop_set_timer(&tbs_bearer->signal_strength_timer, tbs_bearer->data.signal_strength_reporting_interval * 1000);
            btstack_run_loop_add_timer(&tbs_bearer->signal_strength_timer);
            break;

        case TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT: {
            if (buffer_size < 1){
                return 0;
            }
            tbs_control_point_opcode_t opcode = (tbs_control_point_opcode_t)buffer[0];
            uint16_t opcode_mask = tbs_bearer->data.optional_opcodes_supported_bitmap;
            // reject opcodes if not supported
            if( (0 == (opcode_mask & TBS_OPCODE_MASK_JOIN)) && (opcode == TBS_CONTROL_POINT_OPCODE_JOIN) ) {
                telephone_bearer_service_server_call_control_point_notification(tbs_bearer->bearer_id, 0, TBS_CONTROL_POINT_OPCODE_JOIN, TBS_CONTROL_POINT_RESULT_OPCODE_NOT_SUPPORTED);
                break;
            }
            if( (0 == (opcode_mask & TBS_OPCODE_MASK_LOCAL_HOLD)) && (opcode == TBS_CONTROL_POINT_OPCODE_LOCAL_HOLD) ) {
                telephone_bearer_service_server_call_control_point_notification(tbs_bearer->bearer_id, 0, TBS_CONTROL_POINT_OPCODE_LOCAL_HOLD, TBS_CONTROL_POINT_RESULT_OPCODE_NOT_SUPPORTED);
                break;
            }
            tbs_server_emit_call_control_notification_task(tbs_bearer, opcode, buffer, buffer_size);
            break;
        }
        default:
            break;
    }
    return 0;
}

static void tbs_bearer_signal_strength_timeout(btstack_timer_source_t *ts){
    void * context = btstack_run_loop_get_timer_context(ts);
    telephone_bearer_service_server_t *bearer = (telephone_bearer_service_server_t*)context;

    tbs_server_schedule_task(bearer, TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH);

    btstack_run_loop_remove_timer(&bearer->signal_strength_timer);
    if( bearer->data.signal_strength_reporting_interval == 0 ) {
        return;
    }
    // reporting interval is in seconds
    btstack_run_loop_set_timer(&bearer->signal_strength_timer, bearer->data.signal_strength_reporting_interval * 1000);
    btstack_run_loop_add_timer(&bearer->signal_strength_timer);
}

static uint8_t tbs_server_register_bearer(uint16_t service_uuid, telephone_bearer_service_server_t * tbs_bearer, 
    btstack_packet_handler_t packet_handler, uint16_t optional_opcodes_supported_bitmap, uint16_t * out_bearer_id){
    // search service with global start handle
    btstack_assert(tbs_bearer != NULL);
    btstack_assert(packet_handler != NULL);
   
#ifdef ENABLE_TESTING_SUPPORT
    const char * tbs_characteristic_uuid_names[]= {
        [TBS_CHARACTERISTIC_INDEX_BEARER_LIST_CURRENT_CALLS]                 = "bearer_list_current_calls                 ",
        [TBS_CHARACTERISTIC_INDEX_BEARER_PROVIDER_NAME]                      = "bearer_provider_name                      ",
        [TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH_REPORTING_INTERVAL] = "bearer_signal_strength_reporting_interval ",
        [TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH]                    = "bearer_signal_strength                    ",
        [TBS_CHARACTERISTIC_INDEX_BEARER_TECHNOLOGY]                         = "bearer_technology                         ",
        [TBS_CHARACTERISTIC_INDEX_BEARER_UCI]                                = "bearer_uci                                ",
        [TBS_CHARACTERISTIC_INDEX_BEARER_URI_SCHEMES_SUPPORTED_LIST]         = "bearer_uri_schemes_supported_list         ",
        [TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT_OPTIONAL_OPCODES]       = "call_control_point_optional_opcodes       ",
        [TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT]                        = "call_control_point                        ",
        [TBS_CHARACTERISTIC_INDEX_CALL_FRIENDLY_NAME]                        = "call_friendly_name                        ",
        [TBS_CHARACTERISTIC_INDEX_CALL_STATE]                                = "call_state                                ",
        [TBS_CHARACTERISTIC_INDEX_CONTENT_CONTROL_ID]                        = "content_control_id                        ",
        [TBS_CHARACTERISTIC_INDEX_INCOMING_CALL_TARGET_BEARER_URI]           = "incoming_call_target_bearer_uri           ",
        [TBS_CHARACTERISTIC_INDEX_INCOMING_CALL]                             = "incoming_call                             ",
        [TBS_CHARACTERISTIC_INDEX_STATUS_FLAGS]                              = "status_flags                              ",
        [TBS_CHARACTERISTIC_INDEX_TERMINATION_REASON]                        = "termination_reason                        ",
    };
#endif
    
    if (tbs_services_start_handle == 0xffff) {
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
     }
    
    uint16_t tbs_services_end_handle   = 0xffff;
    bool     service_found = gatt_server_get_handle_range_for_service_with_uuid16(service_uuid, &tbs_services_start_handle, &tbs_services_end_handle);
    
    if (!service_found){
        tbs_services_start_handle = 0xffff;
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
     }

    telephone_bearer_service_server_t * tbs_bearer_registered = tbs_server_get_registered_bearer(tbs_services_start_handle);
    if (tbs_bearer_registered != NULL){
        return ERROR_CODE_REPEATED_ATTEMPTS;
    }

    log_info("Found %s service 0x%02x-0x%02x", service_uuid == ORG_BLUETOOTH_SERVICE_TELEPHONE_BEARER_SERVICE ? "TBS":"GTBS", tbs_services_start_handle, tbs_services_end_handle);

#ifdef ENABLE_TESTING_SUPPORT
    printf("Found %s service 0x%02x-0x%02x\n", service_uuid == ORG_BLUETOOTH_SERVICE_TELEPHONE_BEARER_SERVICE ? "TBS":"GTBS", tbs_services_start_handle, tbs_services_end_handle);
#endif
    tbs_server_reset_bearer(tbs_bearer);
    
    tbs_bearer->service.start_handle   = tbs_services_start_handle;
    tbs_bearer->service.end_handle     = tbs_services_end_handle;
    tbs_bearer->data.optional_opcodes_supported_bitmap = optional_opcodes_supported_bitmap;
    // get characteristic value handles
    uint16_t i;
    for (i = 0; i < TBS_CHARACTERISTICS_NUM; i++){
        tbs_bearer->characteristics[i].value_handle = gatt_server_get_value_handle_for_characteristic_with_uuid16(tbs_services_start_handle, tbs_services_end_handle, tbs_characteristic_uuids[i]);
        tbs_bearer->characteristics[i].client_configuration_handle = gatt_server_get_client_configuration_handle_for_characteristic_with_uuid16(tbs_services_start_handle, tbs_services_end_handle, tbs_characteristic_uuids[i ]);

#ifdef ENABLE_TESTING_SUPPORT
        printf("    %s      0x%02x\n", tbs_characteristic_uuid_names[i], tbs_bearer->characteristics[i].value_handle);
        if (tbs_bearer->characteristics[i].client_configuration_handle != 0){
            printf("    %s CCC  0x%02x\n", tbs_characteristic_uuid_names[i], tbs_bearer->characteristics[i].client_configuration_handle);
        }
#endif
    }
    
    uint16_t bearer_id = tbs_server_get_next_bearer_id();
    if (out_bearer_id != NULL) {
        *out_bearer_id = bearer_id;
    }

    tbs_bearer->event_callback = packet_handler;

    // register service with ATT Server
    tbs_bearer->bearer_id = bearer_id;
    tbs_bearer->service.read_callback  = &tbs_server_read_callback;
    tbs_bearer->service.write_callback = &tbs_server_write_callback;
    tbs_bearer->service.packet_handler = tbs_server_packet_handler;
    att_server_register_service_handler(&tbs_bearer->service);
    
    // add to media player list
    btstack_linked_list_add(&tbs_bearers, &tbs_bearer->item);

    // init calls list
    tbs_bearer->calls = NULL;
    tbs_bearer->call_counter = 0;

    // init signal strength reporting
    tbs_bearer->data.signal_strength = 255;
    tbs_bearer->data.signal_strength_reporting_interval = 0;

    btstack_run_loop_set_timer_handler(&tbs_bearer->signal_strength_timer, tbs_bearer_signal_strength_timeout);
    btstack_run_loop_set_timer_context(&tbs_bearer->signal_strength_timer, tbs_bearer);
    btstack_run_loop_remove_timer(&tbs_bearer->signal_strength_timer);

    // clear call control point notification flag
    tbs_bearer->data.call_control_point_notification_pending = false;

    // prepare for the next service
    tbs_services_start_handle = tbs_bearer->service.end_handle;
    return ERROR_CODE_SUCCESS;
}

uint8_t telephone_bearer_service_server_register_generic_bearer(
    telephone_bearer_service_server_t * bearer, 
    btstack_packet_handler_t packet_handler, 
    uint16_t optional_opcodes_supported_bitmap,
    uint16_t * out_bearer_id){

    return tbs_server_register_bearer(ORG_BLUETOOTH_SERVICE_GENERIC_TELEPHONE_BEARER_SERVICE,
                                      bearer, packet_handler, optional_opcodes_supported_bitmap, out_bearer_id);
}

uint8_t telephone_bearer_service_server_register_individual_bearer(
    telephone_bearer_service_server_t * bearer, 
    btstack_packet_handler_t packet_handler, 
    uint16_t optional_opcodes_supported_bitmap,
    uint16_t * out_bearer_id){

    return tbs_server_register_bearer(ORG_BLUETOOTH_SERVICE_TELEPHONE_BEARER_SERVICE,
                                      bearer, packet_handler, optional_opcodes_supported_bitmap, out_bearer_id);
}

void telephone_bearer_service_server_init(void){
    tbs_services_start_handle = 0;
    tbs_bearer_counter = 0;
    tbs_bearers = NULL;
}

uint8_t telephone_bearer_service_server_register_call( uint16_t bearer_id, tbs_call_data_t *const call, uint16_t *const out_call_id ) {
    telephone_bearer_service_server_t *tbs_bearer = telephone_bearer_service_server_get_bearer_by_id( bearer_id );
    if( tbs_bearer == NULL ) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    uint16_t call_id = tbs_server_get_next_call_id(tbs_bearer);
    if( out_call_id != NULL ) {
        *out_call_id = call_id;
    }
    call->id = call_id;
    call->deregister = false;
    call->scheduled_tasks = 0;

    btstack_linked_list_add(&tbs_bearer->calls, &call->item);
    // TODO: check client implementation for update order, enable if necessary/useful
//    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_CALL_STATE);
//    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_BEARER_LIST_CURRENT_CALLS);
    return ERROR_CODE_SUCCESS;
}

static void tbs_server_check_for_deregister_call( telephone_bearer_service_server_t *bearer ) {
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &bearer->calls);
    while (btstack_linked_list_iterator_has_next(&it)){
        tbs_call_data_t * call = (tbs_call_data_t *)btstack_linked_list_iterator_next(&it);
        if (call->deregister && (call->scheduled_tasks==0)) {
            btstack_linked_list_iterator_remove(&it);
            tbs_server_schedule_task(bearer, TBS_CHARACTERISTIC_INDEX_CALL_STATE);
            tbs_server_schedule_task(bearer, TBS_CHARACTERISTIC_INDEX_BEARER_LIST_CURRENT_CALLS);
            tbs_server_emit_call_deregister_done(bearer, call->id);
        }
    }
}

uint8_t telephone_bearer_service_server_deregister_call( uint16_t bearer_id, uint16_t call_id ) {
    telephone_bearer_service_server_t *tbs_bearer = telephone_bearer_service_server_get_bearer_by_id( bearer_id );
    if( tbs_bearer == NULL ) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    tbs_call_data_t *call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, call_id );
    if( call == NULL ) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    call->deregister = true;

    // TODO: check client implementation for update order, enable if necessary/useful
//    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_CALL_STATE);
//    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_BEARER_LIST_CURRENT_CALLS);
    return ERROR_CODE_SUCCESS;
}

uint8_t telephone_bearer_service_server_set_call_state( uint16_t bearer_id, uint16_t call_id, const tbs_call_state_t state ) {
    telephone_bearer_service_server_t *tbs_bearer = telephone_bearer_service_server_get_bearer_by_id( bearer_id );
    if( tbs_bearer == NULL ) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    tbs_call_data_t *call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, call_id );
    if( call == NULL ) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    call->state = state;

    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_CALL_STATE);

    // update compound data, which depend on this
    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_BEARER_LIST_CURRENT_CALLS);
    return ERROR_CODE_SUCCESS;
}

tbs_call_state_t telephone_bearer_service_server_get_call_state( tbs_call_data_t *call ) {
    btstack_assert( call != NULL );
    return call->state;
}

uint8_t telephone_bearer_service_server_call_uri( uint16_t bearer_id, uint16_t call_id, const char * uri ) {
    telephone_bearer_service_server_t *tbs_bearer = telephone_bearer_service_server_get_bearer_by_id( bearer_id );
    if( tbs_bearer == NULL ) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    tbs_call_data_t *call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, call_id );
    if( call == NULL ) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    btstack_strcpy(call->call_uri, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH, uri);

    tbs_server_call_schedule_task(call, TBS_CHARACTERISTIC_INDEX_INCOMING_CALL);
    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_INCOMING_CALL);

    // update compound data, which depend on this
    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_BEARER_LIST_CURRENT_CALLS);
    return ERROR_CODE_SUCCESS;
}

uint8_t telephone_bearer_service_server_incoming_call_target_bearer_uri( uint16_t bearer_id, uint16_t call_id, const char * uri ) {
    telephone_bearer_service_server_t *tbs_bearer = telephone_bearer_service_server_get_bearer_by_id( bearer_id );
    if( tbs_bearer == NULL ) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    tbs_call_data_t *call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, call_id );
    if( call == NULL ) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    btstack_strcpy( call->target_uri, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH, uri );

    // update itself
    tbs_server_call_schedule_task(call, TBS_CHARACTERISTIC_INDEX_INCOMING_CALL_TARGET_BEARER_URI);
    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_INCOMING_CALL_TARGET_BEARER_URI);
    return ERROR_CODE_SUCCESS;
}

uint8_t telephone_bearer_service_server_call_friendly_name( uint16_t bearer_id, uint16_t call_id, const char * uri ) {
    telephone_bearer_service_server_t *tbs_bearer = telephone_bearer_service_server_get_bearer_by_id( bearer_id );
    if( tbs_bearer == NULL ) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    tbs_call_data_t *call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, call_id );
    if( call == NULL ) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    btstack_strcpy( call->friendly_name, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH, uri );

    tbs_server_call_schedule_task(call, TBS_CHARACTERISTIC_INDEX_CALL_FRIENDLY_NAME);
    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_CALL_FRIENDLY_NAME);
    return ERROR_CODE_SUCCESS;
}

uint8_t telephone_bearer_service_server_set_provider_name(uint16_t bearer_id, const char * provider_name){
    telephone_bearer_service_server_t * tbs_bearer = telephone_bearer_service_server_get_bearer_by_id(bearer_id);
    if (tbs_bearer == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    btstack_strcpy(tbs_bearer->data.provider_name, sizeof(tbs_bearer->data.provider_name), provider_name);

    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_BEARER_PROVIDER_NAME);
    return ERROR_CODE_SUCCESS;
}

uint8_t telephone_bearer_service_server_set_signal_strength(uint16_t bearer_id, const uint8_t signal_strength){
    telephone_bearer_service_server_t * tbs_bearer = telephone_bearer_service_server_get_bearer_by_id(bearer_id);
    if (tbs_bearer == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    tbs_bearer->data.signal_strength = signal_strength;
    if( tbs_bearer->data.signal_strength_reporting_interval != 0 ) {
        return ERROR_CODE_SUCCESS;
    }

    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_BEARER_SIGNAL_STRENGTH);
    return ERROR_CODE_SUCCESS;
}

uint8_t telephone_bearer_service_server_set_supported_schemes_list(uint16_t bearer_id, const char * schemes_list){
    telephone_bearer_service_server_t * tbs_bearer = telephone_bearer_service_server_get_bearer_by_id(bearer_id);
    if (tbs_bearer == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    btstack_strcpy(tbs_bearer->data.uri_schemes_list, sizeof(tbs_bearer->data.uri_schemes_list), schemes_list);

    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_BEARER_URI_SCHEMES_SUPPORTED_LIST);
    return ERROR_CODE_SUCCESS;
}

uint8_t telephone_bearer_service_server_set_technology(uint16_t bearer_id, const tbs_technology_t technology){
    telephone_bearer_service_server_t * tbs_bearer = telephone_bearer_service_server_get_bearer_by_id(bearer_id);
    if (tbs_bearer == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    tbs_bearer->data.technology = technology;

    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_BEARER_TECHNOLOGY);
    return ERROR_CODE_SUCCESS;
}

uint8_t telephone_bearer_service_server_call_control_point_notification(uint16_t bearer_id, uint16_t call_id, tbs_control_point_opcode_t opcode, tbs_control_point_notification_result_codes_t result){
    telephone_bearer_service_server_t * tbs_bearer = telephone_bearer_service_server_get_bearer_by_id(bearer_id);
    if (tbs_bearer == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    tbs_call_data_t *call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, call_id );
    if( call == NULL ) {
        tbs_bearer->data.call_control_point_notification[0] = opcode;
        tbs_bearer->data.call_control_point_notification[1] = 0;
        tbs_bearer->data.call_control_point_notification[2] = result;
        tbs_bearer->data.call_control_point_notification_pending = true;

        tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT);
        return ERROR_CODE_SUCCESS;
    }

    call->call_control_point_notification[0] = opcode;
    call->call_control_point_notification[1] = call_id;
    call->call_control_point_notification[2] = result;
    if( result > TBS_CONTROL_POINT_RESULT_SUCCESS ) {
        call->call_control_point_notification[1] = 0;
    }

    tbs_server_call_schedule_task(call, TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT);
    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_CALL_CONTROL_POINT);
    return ERROR_CODE_SUCCESS;
}

uint8_t telephone_bearer_service_server_termination_reason(uint16_t bearer_id, uint16_t call_id, tbs_call_termination_reason_t reason){
    telephone_bearer_service_server_t * tbs_bearer = telephone_bearer_service_server_get_bearer_by_id(bearer_id);
    if (tbs_bearer == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    tbs_call_data_t *call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, call_id );
    if( call == NULL ) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    call->termination_reason = reason;

    tbs_server_call_schedule_task(call, TBS_CHARACTERISTIC_INDEX_TERMINATION_REASON);
    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_TERMINATION_REASON);
    return ERROR_CODE_SUCCESS;
}

uint8_t telephone_bearer_service_server_set_uniform_caller_identifier(uint16_t bearer_id, const char *uniform_caller_identifier) {
    telephone_bearer_service_server_t * tbs_bearer = telephone_bearer_service_server_get_bearer_by_id(bearer_id);
    if (tbs_bearer == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    btstack_strcpy(tbs_bearer->data.uniform_caller_identifier, sizeof(tbs_bearer->data.uniform_caller_identifier), uniform_caller_identifier);
    return ERROR_CODE_SUCCESS;
}

uint8_t telephone_bearer_service_server_set_status_flags(uint16_t bearer_id, const uint16_t flags) {
    telephone_bearer_service_server_t * tbs_bearer = telephone_bearer_service_server_get_bearer_by_id(bearer_id);
    if (tbs_bearer == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    tbs_bearer->data.status_flags = flags;
    tbs_server_schedule_task(tbs_bearer, TBS_CHARACTERISTIC_INDEX_STATUS_FLAGS);
    return ERROR_CODE_SUCCESS;
}
