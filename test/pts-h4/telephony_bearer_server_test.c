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

#define BTSTACK_FILE__ "telephony_bearer_server_test.c"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "btstack_hsm.h"
#include "telephony_bearer_server_test.h"
#include "btstack.h"

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size);
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size);

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

typedef enum {
    BAP_APP_SERVER_STATE_IDLE = 0,
    BAP_APP_SERVER_STATE_CONNECTED
} bap_app_server_state_t; 

static bap_app_server_state_t  bap_app_server_state      = BAP_APP_SERVER_STATE_IDLE;
static hci_con_handle_t        bap_app_server_con_handle = HCI_CON_HANDLE_INVALID;
static bd_addr_t               bap_app_client_addr;

// Advertisement

#define APP_AD_FLAGS 0x06

static const uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, BLUETOOTH_DATA_TYPE_FLAGS, APP_AD_FLAGS,
    // RSI
    0x07, BLUETOOTH_DATA_TYPE_RESOLVABLE_SET_IDENTIFIER, 0x28, 0x31, 0xB6, 0x4C, 0x39, 0xCC,
    // CSIS, OTS
    0x05, BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS, 0x46, 0x18, 0x25, 0x18,
    // Name
    0x04, BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME, 'I', 'U', 'T', 
};

static const uint8_t adv_sid = 0;
static uint8_t       adv_handle = 0;
static le_advertising_set_t le_advertising_set;

static le_extended_advertising_parameters_t extended_params = {
        .advertising_event_properties = 0x13,  //  scannable & connectable
        .primary_advertising_interval_min = 0x030, // 30 ms
        .primary_advertising_interval_max = 0x030, // 30 ms
        .primary_advertising_channel_map = 7,
        .own_address_type = 0,
        .peer_address_type = 0,
        .peer_address =  { 0 },
        .advertising_filter_policy = 0,
        .advertising_tx_power = 10, // 10 dBm
        .primary_advertising_phy = 1, // LE 1M PHY
        .secondary_advertising_max_skip = 0,
        .secondary_advertising_phy = 1, // LE 1M PHY
        .advertising_sid = adv_sid,
        .scan_request_notification_enable = 0,
};

static void setup_advertising(void);

// Telephony Bearer Server (TBS)
#define TBS_BEARERS_MAX_NUM 2

// TBS HSM
#define TRAN( target ) btstack_hsm_transit( &me->super, (btstack_hsm_state_handler_t)target )
#define SUPER( target ) btstack_hsm_super( &me->super, (btstack_hsm_state_handler_t)target )
#define TBS_HEARER_CONNECTIONS_MAX_NUM 4

enum {
    ACCEPT_SIG = BTSTACK_HSM_USER_SIG,
    INCOMING_SIG,
    LOCAL_HOLD_SIG,
    LOCAL_RETRIEVE_SIG,
    ORIGINATE_SIG,
    REMOTE_ALERT_START_SIG,
    REMOTE_ANSWER_SIG,
    REMOTE_HOLD_SIG,
    REMOTE_RETRIEVE_SIG,
    TERMINATE_SIG,
};

typedef struct {
    btstack_hsm_event_t data;
    bool private;
} tbs_private_public_event_t;

typedef struct {
    telephone_bearer_service_server_t bearer;
    tbs_server_connection_t connections[TBS_HEARER_CONNECTIONS_MAX_NUM];
    uint16_t id;
    char *scheme;
} my_bearer_t;

typedef struct {
    btstack_hsm_t super;
    tbs_call_data_t data;
    uint16_t id;
    my_bearer_t *bearer;
    char uri[TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH];
    char target_uri[TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH];
    char caller_id[TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH];
    char friendly_name[TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH];
} my_call_data_t;

typedef struct {
    btstack_hsm_event_t data;
    char *caller_id;
    char *friendly_name;
    char *target_uri;
} tbs_call_event_t;

int signal_to_opcode[] = {
        [ACCEPT_SIG]         = TBS_CONTROL_POINT_OPCODE_ACCEPT,
//        [INCOMING_SIG]
        [LOCAL_HOLD_SIG]     = TBS_CONTROL_POINT_OPCODE_LOCAL_HOLD,
        [LOCAL_RETRIEVE_SIG] = TBS_CONTROL_POINT_OPCODE_LOCAL_RETRIEVE,
        [ORIGINATE_SIG]      = TBS_CONTROL_POINT_OPCODE_ORIGINATE,
//        [REMOTE_ALERT_START_SIG]
//        [REMOTE_ANSWER_SIG]
//        [REMOTE_HOLD_SIG]
//        [REMOTE_RETRIEVE_SIG]
        [TERMINATE_SIG] = TBS_CONTROL_POINT_OPCODE_TERMINATE
};

#define TBS_STATE_DEPTH (2)
btstack_hsm_state_handler_t path[TBS_STATE_DEPTH];

static void tbs_state_constructor( my_call_data_t *call_data );

static btstack_hsm_state_t tbs_state_initial(my_call_data_t * const me, btstack_hsm_event_t const * const e);

static btstack_hsm_state_t tbs_state_any_state(my_call_data_t * const me, btstack_hsm_event_t const * const e);
static btstack_hsm_state_t tbs_state_call_ended(my_call_data_t * const me, btstack_hsm_event_t const * const e);

static btstack_hsm_state_t tbs_state_dialing(my_call_data_t * const me, btstack_hsm_event_t const * const e);
static btstack_hsm_state_t tbs_state_alerting(my_call_data_t * const me, btstack_hsm_event_t const * const e);
static btstack_hsm_state_t tbs_state_incoming(my_call_data_t * const me, btstack_hsm_event_t const * const e);
static btstack_hsm_state_t tbs_state_active(my_call_data_t * const me, btstack_hsm_event_t const * const e);
static btstack_hsm_state_t tbs_state_locally_held(my_call_data_t * const me, btstack_hsm_event_t const * const e);
static btstack_hsm_state_t tbs_state_locally_and_remotely_held(my_call_data_t * const me, btstack_hsm_event_t const * const e);
static btstack_hsm_state_t tbs_state_remotely_held(my_call_data_t * const me, btstack_hsm_event_t const * const e);

static btstack_hsm_state_t tbs_state_initial(my_call_data_t * const me, btstack_hsm_event_t const * const e) {
    btstack_hsm_state_t status;
    uint16_t bearer_id = me->bearer->id;
    uint16_t call_id = me->id;
    switch( e->sig ) {
    case ORIGINATE_SIG: {
        tbs_call_event_t *out = (tbs_call_event_t*)e;
        int opcode = signal_to_opcode[e->sig];

        char *scheme = me->bearer->scheme;
        if( strncmp( out->target_uri, scheme, strlen(scheme) ) != 0 ) {
            printf("Wrong scheme. Declining call.\n");
            telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_INVALID_OUTGOING_URI);
            status = TRAN(tbs_state_call_ended);
            break;
        }

        btstack_strcpy(me->friendly_name, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH, out->friendly_name);
        btstack_strcpy(me->caller_id, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH, out->caller_id);
        btstack_strcpy(me->target_uri, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH, out->target_uri);

        telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_SUCCESS);
        status = TRAN(tbs_state_dialing);
        break;
    }
    case INCOMING_SIG: {
        tbs_call_event_t *in = (tbs_call_event_t*)e;
        printf("%s\n", in->friendly_name);
        btstack_strcpy(me->friendly_name, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH, in->friendly_name);
        btstack_strcpy(me->caller_id, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH, in->caller_id);
        btstack_strcpy(me->target_uri, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH, in->target_uri);

        me->uri[0] = '\0';
        btstack_strcat(me->uri, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH, me->bearer->scheme);
        btstack_strcat(me->uri, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH, ":" );
        btstack_strcat(me->uri, TELEPHONE_BEARER_SERVICE_URI_MAX_LENGTH, in->caller_id );

        status = TRAN(tbs_state_incoming);
        break;
    }
    default:
        status = BTSTACK_HSM_UNHANDLED_STATUS;
        break;
    }
    return status;
}

static void tbs_state_constructor( my_call_data_t *call_data ) {
    btstack_hsm_constructor( &call_data->super, (btstack_hsm_state_handler_t)tbs_state_initial, path, TBS_STATE_DEPTH );
}

static btstack_hsm_state_t tbs_state_any_state(my_call_data_t * const me, btstack_hsm_event_t const * const e) {
    btstack_hsm_state_t status;
    uint16_t bearer_id = me->bearer->id;
    uint16_t call_id = me->id;
    tbs_private_public_event_t *event = (tbs_private_public_event_t*)e;

    switch(e->sig) {
        case BTSTACK_HSM_ENTRY_SIG: {
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case BTSTACK_HSM_EXIT_SIG: {
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case TERMINATE_SIG: {
            int opcode = signal_to_opcode[e->sig];
            telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_SUCCESS);
            status = TRAN(tbs_state_call_ended);
            break;
        }
        case ACCEPT_SIG:
        case LOCAL_HOLD_SIG:
        case LOCAL_RETRIEVE_SIG: {
            if( !event->private ) {
                int opcode = signal_to_opcode[e->sig];
                telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_STATE_MISMATCH);
            }
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        default: {
            status = SUPER(btstack_hsm_top);
            break;
        }
    }
    return status;
}

static btstack_hsm_state_t tbs_state_call_ended(my_call_data_t * const me, btstack_hsm_event_t const * const e) {
    btstack_hsm_state_t status;
    uint16_t bearer_id = me->bearer->id;
    uint16_t call_id = me->id;
    switch(e->sig) {
        case BTSTACK_HSM_ENTRY_SIG: {
            telephone_bearer_service_server_termination_reason(bearer_id, call_id, TBS_CALL_TERMINATION_REASON_CLIENT_ENDED_CALL);
            telephone_bearer_service_server_deregister_call(bearer_id, call_id);
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case BTSTACK_HSM_EXIT_SIG: {
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        default: {
            status = SUPER(btstack_hsm_top);
            break;
        }
    }
    return status;
}

static btstack_hsm_state_t tbs_state_dialing(my_call_data_t * const me, btstack_hsm_event_t const * const e) {
    btstack_hsm_state_t status;
    uint16_t bearer_id = me->bearer->id;
    uint16_t call_id = me->id;
    switch(e->sig) {
        case BTSTACK_HSM_ENTRY_SIG: {
            telephone_bearer_service_server_set_call_state(bearer_id, call_id, TBS_CALL_STATE_DIALING);

            telephone_bearer_service_server_call_uri(bearer_id, call_id, me->target_uri);
            telephone_bearer_service_server_call_friendly_name(bearer_id, call_id, me->friendly_name);
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case BTSTACK_HSM_EXIT_SIG: {
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case REMOTE_ALERT_START_SIG: {
            status = TRAN(tbs_state_alerting);
            break;
        }
        default: {
            status = SUPER(tbs_state_any_state);
            break;
        }
    }
    return status;
}

static btstack_hsm_state_t tbs_state_alerting(my_call_data_t * const me, btstack_hsm_event_t const * const e) {
    btstack_hsm_state_t status;
    uint16_t bearer_id = me->bearer->id;
    uint16_t call_id = me->id;
    switch(e->sig) {
        case BTSTACK_HSM_ENTRY_SIG: {
            telephone_bearer_service_server_set_call_state(bearer_id, call_id, TBS_CALL_STATE_ALERTING);
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case BTSTACK_HSM_EXIT_SIG: {
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case REMOTE_ANSWER_SIG: {
            status = TRAN(tbs_state_active);
            break;
        }
        default: {
            status = SUPER(tbs_state_any_state);
            break;
        }
    }
    return status;
}

static btstack_hsm_state_t tbs_state_incoming(my_call_data_t * const me, btstack_hsm_event_t const * const e) {
    btstack_hsm_state_t status;
    uint16_t bearer_id = me->bearer->id;
    uint16_t call_id = me->id;
    tbs_private_public_event_t *event = (tbs_private_public_event_t*)e;
    switch(e->sig) {
        case BTSTACK_HSM_ENTRY_SIG: {
            telephone_bearer_service_server_set_call_state(bearer_id, call_id, TBS_CALL_STATE_INCOMMING);
            telephone_bearer_service_server_call_uri(bearer_id, call_id, me->uri);
            telephone_bearer_service_server_incoming_call_target_bearer_uri(bearer_id, call_id, me->target_uri);
            telephone_bearer_service_server_call_friendly_name(bearer_id, call_id, me->friendly_name);
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case BTSTACK_HSM_EXIT_SIG: {
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case INCOMING_SIG: {
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case ACCEPT_SIG: {
            if( !event->private ) {
                int opcode = signal_to_opcode[e->sig];
                telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_SUCCESS);
            }
            status = TRAN(tbs_state_active);
            break;
        }
        case LOCAL_HOLD_SIG: {
            if( !event->private ) {
                int opcode = signal_to_opcode[e->sig];
                telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_SUCCESS);
            }
            status = TRAN(tbs_state_locally_held);
            break;
        }
        default: {
            status = SUPER(tbs_state_any_state);
            break;
        }
    }
    return status;
}

static btstack_hsm_state_t tbs_state_active(my_call_data_t * const me, btstack_hsm_event_t const * const e) {
    btstack_hsm_state_t status;
    uint16_t bearer_id = me->bearer->id;
    uint16_t call_id = me->id;
    tbs_private_public_event_t *event = (tbs_private_public_event_t*)e;

    switch(e->sig) {
        case BTSTACK_HSM_ENTRY_SIG: {
            telephone_bearer_service_server_set_call_state(bearer_id, call_id, TBS_CALL_STATE_ACTIVE);
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case BTSTACK_HSM_EXIT_SIG: {
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case REMOTE_HOLD_SIG: {
            status = TRAN(tbs_state_remotely_held);
            break;
        }
        case LOCAL_HOLD_SIG: {
            if( !event->private ) {
                int opcode = signal_to_opcode[e->sig];
                telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_SUCCESS);
            }
            status = TRAN(tbs_state_locally_held);
            break;
        }
        default: {
            status = SUPER(tbs_state_any_state);
            break;
        }
    }
    return status;
}

static btstack_hsm_state_t tbs_state_locally_held(my_call_data_t * const me, btstack_hsm_event_t const * const e) {
    btstack_hsm_state_t status;
    uint16_t bearer_id = me->bearer->id;
    uint16_t call_id = me->id;
    tbs_private_public_event_t *event = (tbs_private_public_event_t*)e;

    switch(e->sig) {
        case BTSTACK_HSM_ENTRY_SIG: {
            telephone_bearer_service_server_set_call_state(bearer_id, call_id, TBS_CALL_STATE_LOCALLY_HELD);
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case BTSTACK_HSM_EXIT_SIG: {
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case REMOTE_HOLD_SIG: {
            printf("%s - REMOTE_HOLD_SIG\n", __func__);
            status = TRAN(tbs_state_locally_and_remotely_held);
            break;
        }
        case LOCAL_RETRIEVE_SIG: {
            if( !event->private ) {
                int opcode = signal_to_opcode[e->sig];
                telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_SUCCESS);
            }
            status = TRAN(tbs_state_active);
            break;
        }
        default: {
            status = SUPER(tbs_state_any_state);
            break;
        }
    }
    return status;
}

static btstack_hsm_state_t tbs_state_locally_and_remotely_held(my_call_data_t * const me, btstack_hsm_event_t const * const e) {
    btstack_hsm_state_t status;
    uint16_t bearer_id = me->bearer->id;
    uint16_t call_id = me->id;
    tbs_private_public_event_t *event = (tbs_private_public_event_t*)e;

    switch(e->sig) {
        case BTSTACK_HSM_ENTRY_SIG: {
            printf("%s - BTSTACK_HSM_ENTRY_SIG\n", __func__);
            telephone_bearer_service_server_set_call_state(bearer_id, call_id, TBS_CALL_STATE_LOCALLY_AND_REMOTELY_HELD);
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case BTSTACK_HSM_EXIT_SIG: {
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case REMOTE_RETRIEVE_SIG: {
            status = TRAN(tbs_state_locally_held);
            break;
        }
        case LOCAL_RETRIEVE_SIG: {
            if( !event->private ) {
                int opcode = signal_to_opcode[e->sig];
                telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_SUCCESS);
            }
            status = TRAN(tbs_state_remotely_held);
            break;
        }
        default: {
            status = SUPER(tbs_state_any_state);
            break;
        }
    }
    return status;
}

static btstack_hsm_state_t tbs_state_remotely_held(my_call_data_t * const me, btstack_hsm_event_t const * const e) {
    btstack_hsm_state_t status;
    uint16_t bearer_id = me->bearer->id;
    uint16_t call_id = me->id;
    tbs_private_public_event_t *event = (tbs_private_public_event_t*)e;

    switch(e->sig) {
        case BTSTACK_HSM_ENTRY_SIG: {
            telephone_bearer_service_server_set_call_state(bearer_id, call_id, TBS_CALL_STATE_REMOTELY_HELD);
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case BTSTACK_HSM_EXIT_SIG: {
            status = BTSTACK_HSM_HANDLED_STATUS;
            break;
        }
        case REMOTE_RETRIEVE_SIG: {
            status = TRAN(tbs_state_active);
            break;
        }
        case LOCAL_HOLD_SIG: {
            if( !event->private ) {
                int opcode = signal_to_opcode[e->sig];
                telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_SUCCESS);
            }
            status = TRAN(tbs_state_locally_and_remotely_held);
            break;
        }
        default: {
            status = SUPER(tbs_state_any_state);
            break;
        }
    }
    return status;
}


static my_bearer_t tbs_bearers[TBS_BEARERS_MAX_NUM];
static uint8_t tbs_bearer_index;

static void     setup_advertising(void) {
    bd_addr_t local_addr;
    gap_local_bd_addr(local_addr);
    bool local_address_invalid = btstack_is_null_bd_addr( local_addr );
    if( local_address_invalid ) {
        extended_params.own_address_type = BD_ADDR_TYPE_LE_RANDOM;
    }
    gap_extended_advertising_setup(&le_advertising_set, &extended_params, &adv_handle);
    if( local_address_invalid ) {
        bd_addr_t random_address = { 0xC1, 0x01, 0x01, 0x01, 0x01, 0x01 };
        gap_extended_advertising_set_random_address( adv_handle, random_address );
    }

    gap_extended_advertising_set_adv_data(adv_handle, sizeof(adv_data), adv_data);
    gap_extended_advertising_start(adv_handle, 0, 0);
}

// ATT Client Read Callback for Dynamic Data
// - if buffer == NULL, don't copy data, just return size of value
// - if buffer != NULL, copy data and return number bytes copied
// @param offset defines start of attribute value
static uint16_t att_read_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t offset, uint8_t * buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(att_handle);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);
    return 0;
}

static int att_write_callback(hci_con_handle_t connection_handle, uint16_t att_handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size){
    UNUSED(connection_handle);
    UNUSED(att_handle);
    UNUSED(transaction_mode);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);
    return 0;
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case ATT_EVENT_CAN_SEND_NOW:
            // att_server_notify(con_handle, ATT_CHARACTERISTIC_0000FF11_0000_1000_8000_00805F9B34FB_01_VALUE_HANDLE, (uint8_t*) counter_string, counter_string_len);
            break;
       case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Accept Just Works\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
       case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;

       case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)){
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    bap_app_server_con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    bap_app_server_state      = BAP_APP_SERVER_STATE_CONNECTED;
                    hci_subevent_le_connection_complete_get_peer_address(packet, bap_app_client_addr);
                    printf("BAP Server: Connection to %s established\n", bd_addr_to_str(bap_app_client_addr));
                    

                    break;
                case HCI_SUBEVENT_LE_ADVERTISING_SET_TERMINATED:
                    // restart advertising
                    gap_extended_advertising_start(adv_handle, 0, 0);
                    break;
                default:
                    return;
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            bap_app_server_con_handle = HCI_CON_HANDLE_INVALID;
            bap_app_server_state      = BAP_APP_SERVER_STATE_IDLE;
            printf("BAP Server: Disconnected from %s\n", bd_addr_to_str(bap_app_client_addr));
            break;

        default:
            break;
    }
}

#define CALL_POOL_SIZE  (10)
my_call_data_t calls[CALL_POOL_SIZE] = {0};

static my_call_data_t *find_call_by_id( int id ) {
    for( int i=0; i<CALL_POOL_SIZE; ++i ) {
        if( calls[i].id == id ) {
            return &calls[i];
        }
    }
    return NULL;
}

static my_call_data_t *find_active_call() {
    for( int i=0; i<CALL_POOL_SIZE; ++i ) {
        if( calls[i].id > 0 ) {
            return &calls[i];
        }
    }
    return NULL;
}

static my_call_data_t *find_unused_call() {
    for( int i=0; i<CALL_POOL_SIZE; ++i ) {
        if( calls[i].id == 0 ) {
            return &calls[i];
        }
    }
    return NULL;
}

#define container_of(ptr, type, member) ((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))

my_bearer_t *tbs_to_my_bearer( telephone_bearer_service_server_t *tbs_bearer ) {
    if( tbs_bearer == NULL ) {
        return NULL;
    }
    return container_of( tbs_bearer, my_bearer_t, bearer );
}

my_call_data_t *tbs_call_to_my_call( tbs_call_data_t *tbs_call ) {
    if( tbs_call == NULL ) {
        return NULL;
    }
    return container_of( tbs_call, my_call_data_t, data );
}

bool join_operation = true;
#define ENUM_TO_STRING(value) [value] = #value
static const char *opcode_to_string[] = {
        ENUM_TO_STRING(TBS_CONTROL_POINT_OPCODE_ACCEPT),
        ENUM_TO_STRING(TBS_CONTROL_POINT_OPCODE_TERMINATE),
        ENUM_TO_STRING(TBS_CONTROL_POINT_OPCODE_LOCAL_HOLD),
        ENUM_TO_STRING(TBS_CONTROL_POINT_OPCODE_LOCAL_RETRIEVE),
        ENUM_TO_STRING(TBS_CONTROL_POINT_OPCODE_ORIGINATE),
        ENUM_TO_STRING(TBS_CONTROL_POINT_OPCODE_JOIN),
};

static void tbs_server_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) return;

    switch (hci_event_gattservice_meta_get_subevent_code(packet)){
        case LEAUDIO_SUBEVENT_TBS_SERVER_CALL_CONTROL_POINT_NOTIFICATION_TASK: {
//            hci_con_handle_t con_handle = little_endian_read_16(packet, 3);
            uint16_t bearer_id = little_endian_read_16(packet, 5);
            uint8_t opcode = packet[7];

            telephone_bearer_service_server_t *tbs_bearer = telephone_bearer_service_server_get_bearer_by_id( bearer_id );
            my_bearer_t *bearer = tbs_to_my_bearer( tbs_bearer );
            uint8_t call_id = packet[8];
            uint8_t *data = &packet[8];
            uint16_t data_size = size - 8;
            tbs_call_data_t *tbs_call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, call_id );
            my_call_data_t *call = tbs_call_to_my_call( tbs_call );
#ifdef DEBUG
            for( int i=0; i<size; ++i ) {
                printf("%#04x, ", packet[i]);
            }
            printf("\n");
            printf("opcode: %d\ncall_id: %d\n", opcode, call_id);
            printf("bearer_id: %d\n", bearer_id);
#endif
            switch( opcode ) {
            case TBS_CONTROL_POINT_OPCODE_ACCEPT: {
                printf("%s( %d )\n", opcode_to_string[opcode], call_id);
                tbs_private_public_event_t sig_accept = { .data.sig=ACCEPT_SIG, .private = false };
                tbs_private_public_event_t sig_local_hold = { .data.sig=LOCAL_HOLD_SIG, .private = true };
                if( call == NULL ) {
                    telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_INVALID_CALL_INDEX);
                    break;
                }
                btstack_hsm_dispatch( &call->super, &sig_accept.data );

                // for all other call's of this bearer
                for( int i=0; i<CALL_POOL_SIZE; ++i ) {
                    if(( calls[i].id > 0 ) && (calls[i].id != call_id) && (calls[i].bearer == bearer)) {
                        tbs_call_state_t state = telephone_bearer_service_server_get_call_state(&calls[i].data);
                        switch( state ) {
                        case TBS_CALL_STATE_ACTIVE:
                            btstack_hsm_dispatch( &calls[i].super, &sig_local_hold.data );
                            break;
                        case TBS_CALL_STATE_REMOTELY_HELD:
                            btstack_hsm_dispatch( &calls[i].super, &sig_local_hold.data );
                            break;
                        default:
                            break;
                        }
                    }
                }
                break;
            }
            case TBS_CONTROL_POINT_OPCODE_JOIN: {
                printf("%s( ", opcode_to_string[opcode]);
                for(int i=0; i<data_size; ++i) {
                    printf("%d ", data[i]);
                }
                printf(")\n");
                if( !join_operation ) {
                    telephone_bearer_service_server_call_control_point_notification(bearer_id, 0, opcode, TBS_CONTROL_POINT_RESULT_OPERATION_NOT_POSSIBLE);
                    break;
                }
                tbs_private_public_event_t sig_retrive = { .data.sig=LOCAL_RETRIEVE_SIG, .private = true };
                tbs_private_public_event_t sig_local_hold = { .data.sig=LOCAL_HOLD_SIG, .private = true };

                if( data_size < 2 ) {
                    telephone_bearer_service_server_call_control_point_notification(bearer_id, 0, opcode, TBS_CONTROL_POINT_RESULT_OPERATION_NOT_POSSIBLE);
                    break;
                }
                // if any call in the list is incoming, or invalid reject operation
                for( int i=0; i<data_size; ++i ) {
                    call_id = data[i];
                    tbs_call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, call_id );
                    if( tbs_call == NULL ) {
                        telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_INVALID_CALL_INDEX);
                        return;
                    }
                    tbs_call_state_t state = telephone_bearer_service_server_get_call_state(tbs_call);
                    switch( state ) {
                    case TBS_CALL_STATE_INCOMMING:
                        telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_OPERATION_NOT_POSSIBLE);
                        return;
                    default:
                        break;
                    }
                }

                call_id = data[0];
                tbs_call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, call_id );
                tbs_call_state_t state = telephone_bearer_service_server_get_call_state(tbs_call);
                telephone_bearer_service_server_set_call_state( bearer_id, call_id, state );

                // retrieve locally held calls
                for( int i=0; i<data_size; ++i ) {
                    tbs_call = telephone_bearer_service_server_get_call_by_id( tbs_bearer, data[i] );
                    call = tbs_call_to_my_call( tbs_call );
                    state = telephone_bearer_service_server_get_call_state(tbs_call);
                    switch( state ) {
                    case TBS_CALL_STATE_LOCALLY_HELD:
                    case TBS_CALL_STATE_LOCALLY_AND_REMOTELY_HELD:
                        btstack_hsm_dispatch( &call->super, &sig_retrive.data );
                        break;
                    default:
                        break;
                    }
                }
                // for all other call's of this bearer
                for( int i=0; i<CALL_POOL_SIZE; ++i ) {
                    call = &calls[i];
                    // skip unused call
                    if( call->id == 0 ) {
                        continue;
                    }
                    // skip call's not belonging to us
                    if( call->bearer != bearer ) {
                        continue;
                    }
                    // skip calls in the join list
                    int j;
                    for( j=0; j<data_size; ++j ) {
                        if( data[i] == call->id ) {
                            break;
                        }
                    }
                    if(j < data_size ) {
                        continue;
                    }

                    tbs_call_state_t state = telephone_bearer_service_server_get_call_state(&calls[i].data);
                    switch( state ) {
                    case TBS_CALL_STATE_ACTIVE:
                        btstack_hsm_dispatch( &calls[i].super, &sig_local_hold.data );
                        break;
                    default:
                        break;
                    }
                }

                telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_SUCCESS);
                break;
            }
            case TBS_CONTROL_POINT_OPCODE_LOCAL_HOLD: {
                printf("%s( %d )\n", opcode_to_string[opcode], call_id);
                tbs_private_public_event_t sig_local_hold = { .data.sig=LOCAL_HOLD_SIG, .private = false };
                if( call == NULL ) {
                    telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_INVALID_CALL_INDEX);
                    break;
                }
                btstack_hsm_dispatch( &call->super, &sig_local_hold.data );
                break;
            }
            case TBS_CONTROL_POINT_OPCODE_LOCAL_RETRIEVE: {
                printf("%s( %d )\n", opcode_to_string[opcode], call_id);

                tbs_private_public_event_t sig_retrive = { .data.sig=LOCAL_RETRIEVE_SIG, .private = false };
                tbs_private_public_event_t sig_local_hold = { .data.sig=LOCAL_HOLD_SIG, .private = true };
                if( call == NULL ) {
                    telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_INVALID_CALL_INDEX);
                    break;
                }
                btstack_hsm_dispatch( &call->super, &sig_retrive.data );

                // for all other call's of this bearer
                for( int i=0; i<CALL_POOL_SIZE; ++i ) {
                    if(( calls[i].id > 0 ) && (calls[i].id != call_id) && (calls[i].bearer == bearer)) {
                        tbs_call_state_t state = telephone_bearer_service_server_get_call_state(&calls[i].data);
                        switch( state ) {
                        case TBS_CALL_STATE_ACTIVE:
                            btstack_hsm_dispatch( &calls[i].super, &sig_local_hold.data );
                            break;
                        case TBS_CALL_STATE_REMOTELY_HELD:
                            btstack_hsm_dispatch( &calls[i].super, &sig_local_hold.data );
                            break;
                        default:
                            break;
                        }
                    }
                }
                break;
            }
            case TBS_CONTROL_POINT_OPCODE_ORIGINATE: {
                printf("%s( %s )\n", opcode_to_string[opcode], data);
                my_call_data_t *call = find_unused_call();
                if( call == NULL ) {
                    telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_LACK_OF_RESOURCES);
                    break;
                }
                btstack_assert( bearer != NULL );

                tbs_state_constructor( call );
                call->bearer = bearer;
                tbs_call_event_t e = {
                  .data.sig = ORIGINATE_SIG,
                  .caller_id = "5551234",
                  .friendly_name = "all mighty",
                  .target_uri = (char *)data,
                };
                telephone_bearer_service_server_register_call( bearer_id, &call->data, &call->id );
                call_id = call->id;
                btstack_hsm_init( &call->super, &e.data);
                break;
            }
            case TBS_CONTROL_POINT_OPCODE_TERMINATE: {
                printf("%s( %d )\n", opcode_to_string[opcode], call_id);
                btstack_hsm_event_t e = { .sig=TERMINATE_SIG };
                if( call == NULL ) {
                    telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_INVALID_CALL_INDEX);
                    break;
                }
                btstack_hsm_dispatch( &call->super, &e );
                break;
            }
            default:
                printf("unknown opcode\n");
                telephone_bearer_service_server_call_control_point_notification(bearer_id, call_id, opcode, TBS_CONTROL_POINT_RESULT_OPCODE_NOT_SUPPORTED);
                break;
            }
            break;
        }
        case LEAUDIO_SUBEVENT_TBS_SERVER_CALL_DEREGISTER_DONE: {
//            hci_con_handle_t con_handle = little_endian_read_16(packet, 3);
//            uint16_t bearer_id = little_endian_read_16(packet, 5);
            uint8_t call_id = packet[7];

            my_call_data_t *call = find_call_by_id(call_id);
            btstack_assert( call != NULL );
            call->id = 0;
            printf("de-register call_id - %d\n", call_id);
            break;
        }
        default:
            break;
    }
}

static void show_usage(void){
    uint8_t iut_address_type;
    bd_addr_t      iut_address;
    gap_le_get_own_address(&iut_address_type, iut_address);

    printf("IUT: addr type %u, addr %s", iut_address_type, bd_addr_to_str(iut_address));

    printf("\n## Telephony Bearer Test Console\n");
   
}
static int bearer_index = 0;

static void stdin_process(char cmd){
    uint8_t status = ERROR_CODE_SUCCESS;
    my_bearer_t *tbs_bearer = &tbs_bearers[bearer_index];
    uint16_t bearer_id = tbs_bearer->id;

    switch (cmd){
        case 'b' : {
            bearer_index += 1;
            bearer_index &= 1;
            printf("bearer id %d selected.\n", bearer_index);
            break;
        }
        case 'c': {
            printf("initiate call...\n");
            my_call_data_t *call = find_unused_call();
            btstack_assert( call != NULL );
            tbs_state_constructor( call );
            call->bearer = tbs_bearer;
            tbs_call_event_t e = {
              .data.sig = INCOMING_SIG,
              .caller_id = "5551234",
              .friendly_name = "John Doe",
              .target_uri = "555000",
            };
            telephone_bearer_service_server_register_call( bearer_id, &call->data, &call->id );
            btstack_hsm_init( &call->super, &e.data);

            printf("call id: %d\n", call->id);
            break;
        }
        case 'n': {
            printf("name things...\n");
            telephone_bearer_service_server_set_provider_name(bearer_id, "Great Mobile");
            telephone_bearer_service_server_set_technology(bearer_id, TBS_TECHNOLOGY_WI_FI);
            telephone_bearer_service_server_set_status_flags(bearer_id, TBS_STATUS_FLAG_INBAND_RINGTONE | TBS_STATUS_FLAG_SILENT_MODE);
            telephone_bearer_service_server_set_supported_schemes_list(bearer_id, "tel,sip,skype");
            telephone_bearer_service_server_set_signal_strength(bearer_id, 66);
            break;
        }
        case 'u': {
            printf("update things...\n");
            telephone_bearer_service_server_set_provider_name(bearer_id, "Great Mobile");
            telephone_bearer_service_server_set_technology(bearer_id, TBS_TECHNOLOGY_5G);
            telephone_bearer_service_server_set_status_flags(bearer_id, 0);
            telephone_bearer_service_server_set_supported_schemes_list(bearer_id, "tel,sip");
            break;
        }
        case 'o': {
            printf("update long things...\n");
            my_call_data_t *call = find_active_call();
            if( call == NULL ) {
                printf("generate a call first!\n");
                break;
            }
            telephone_bearer_service_server_incoming_call_target_bearer_uri(bearer_id, call->id, "012345678901234567890123456789");
            telephone_bearer_service_server_call_uri(bearer_id, call->id, "012345678901234567890123456789");

            telephone_bearer_service_server_set_provider_name(bearer_id, "012345678901234567890123456789");
            telephone_bearer_service_server_call_friendly_name(bearer_id, call->id, "012345678901234567890123456789");
            telephone_bearer_service_server_set_supported_schemes_list(bearer_id, "012345678901234567890123456789");
            telephone_bearer_service_server_set_technology(bearer_id, TBS_TECHNOLOGY_5G);
            break;
        }
        case 'r': {
            printf("putting active call to remotely held.\n");
            tbs_private_public_event_t sig_remote_hold = { .data.sig=REMOTE_HOLD_SIG, .private = true };
            for( int i=0; i<CALL_POOL_SIZE; ++i ) {
                if( calls[i].id > 0 ) {
                    tbs_call_state_t state = telephone_bearer_service_server_get_call_state(&calls[i].data);
                    switch( state ) {
                    case TBS_CALL_STATE_ACTIVE:
                        btstack_hsm_dispatch( &calls[i].super, &sig_remote_hold.data );
                        return;
                    default:
                        break;
                    }
                }
            }
            break;
        }
        case 'h': {
            printf("putting active call to locally and remotely held.\n");
            tbs_private_public_event_t sig_remote_hold = { .data.sig=REMOTE_HOLD_SIG, .private = true };
            tbs_private_public_event_t sig_local_hold = { .data.sig=LOCAL_HOLD_SIG, .private = true };

            for( int i=0; i<CALL_POOL_SIZE; ++i ) {
                if( calls[i].id > 0 ) {
                    tbs_call_state_t state = telephone_bearer_service_server_get_call_state(&calls[i].data);
                    switch( state ) {
                    case TBS_CALL_STATE_ACTIVE:
                        btstack_hsm_dispatch( &calls[i].super, &sig_remote_hold.data );
                        btstack_hsm_dispatch( &calls[i].super, &sig_local_hold.data );
                        return;
                    default:
                        break;
                    }
                }
            }
            break;
        }
        case 'a': {
            printf("putting dialing call to alarming.\n");
            tbs_private_public_event_t sig_remote_allert = { .data.sig=REMOTE_ALERT_START_SIG, .private = true };

            for( int i=0; i<CALL_POOL_SIZE; ++i ) {
                if( calls[i].id > 0 ) {
                    tbs_call_state_t state = telephone_bearer_service_server_get_call_state(&calls[i].data);
                    switch( state ) {
                    case TBS_CALL_STATE_DIALING:
                        btstack_hsm_dispatch( &calls[i].super, &sig_remote_allert.data );
                        return;
                    default:
                        break;
                    }
                }
            }
            break;
        }
        case 'd': {
            join_operation = !join_operation;
            if( !join_operation ) {
                printf("deny join operation.\n");
            } else {
                printf("join operation allowed.\n");
            }
            break;
        }

        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf(" - Command '%c' could not be performed, status 0x%02x\n", cmd, status);
    }
}

int btstack_main(void);
int btstack_main(void)
{
    
    l2cap_init();

    // setup le device db
    le_device_db_init();

    // setup SM: Display only
    sm_init();
    // register for SM events
    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
    sm_allow_ltk_reconstruction_without_le_device_db_entry(0);
    // setup ATT server
    att_server_init(profile_data, att_read_callback, att_write_callback);    

    // setup TBS
    telephone_bearer_service_server_init();
    
    tbs_bearer_index = 0;
    (void) telephone_bearer_service_server_register_generic_bearer(
        &tbs_bearers[tbs_bearer_index].bearer,
        &tbs_server_packet_handler,
        TBS_HEARER_CONNECTIONS_MAX_NUM, tbs_bearers[tbs_bearer_index].connections,
        TBS_OPCODE_MASK_LOCAL_HOLD | TBS_OPCODE_MASK_JOIN,
        &tbs_bearers[tbs_bearer_index].id);

    tbs_bearer_index++;
    (void) telephone_bearer_service_server_register_individual_bearer(
            &tbs_bearers[tbs_bearer_index].bearer,
            &tbs_server_packet_handler,
            TBS_HEARER_CONNECTIONS_MAX_NUM, tbs_bearers[tbs_bearer_index].connections,
            TBS_OPCODE_MASK_LOCAL_HOLD | TBS_OPCODE_MASK_JOIN,
            &tbs_bearers[tbs_bearer_index].id);

    // register for HCI events
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    gap_set_max_number_peripheral_connections(2);
    // setup advertisements
    setup_advertising();
    gap_periodic_advertising_sync_transfer_set_default_parameters(2, 0, 0x2000, 0);

    tbs_bearers[0].scheme = "tel";
    tbs_bearers[1].scheme = "tel";
    btstack_stdin_setup(stdin_process);
    // turn on!
	hci_power_control(HCI_POWER_ON);
	    
    return 0;
}
