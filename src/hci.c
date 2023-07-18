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

#define BTSTACK_FILE__ "hci.c"

/*
 *  hci.c
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#include "btstack_config.h"


#ifdef ENABLE_CLASSIC
#ifdef HAVE_EMBEDDED_TICK
#include "btstack_run_loop_embedded.h"
#endif
#endif

#ifdef ENABLE_BLE
#include "gap.h"
#include "ble/le_device_db.h"
#endif

#include <stdarg.h>
#include <string.h>
#include <inttypes.h>

#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_linked_list.h"
#include "btstack_memory.h"
#include "bluetooth_company_id.h"
#include "bluetooth_data_types.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "ad_parser.h"

#ifdef ENABLE_CONTROLLER_DUMP_PACKETS
#include <stdio.h>  // sprintf
#endif

#ifdef ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
#ifndef HCI_HOST_ACL_PACKET_NUM
#error "ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL requires to define HCI_HOST_ACL_PACKET_NUM"
#endif
#ifndef HCI_HOST_ACL_PACKET_LEN
#error "ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL requires to define HCI_HOST_ACL_PACKET_LEN"
#endif
#ifndef HCI_HOST_SCO_PACKET_NUM
#error "ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL requires to define HCI_HOST_SCO_PACKET_NUM"
#endif
#ifndef HCI_HOST_SCO_PACKET_LEN
#error "ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL requires to define HCI_HOST_SCO_PACKET_LEN"
#endif
#endif

#ifndef MAX_NR_CONTROLLER_ACL_BUFFERS
#define MAX_NR_CONTROLLER_ACL_BUFFERS 255
#endif
#ifndef MAX_NR_CONTROLLER_SCO_PACKETS
#define MAX_NR_CONTROLLER_SCO_PACKETS 255
#endif

#ifndef HCI_ACL_CHUNK_SIZE_ALIGNMENT
#define HCI_ACL_CHUNK_SIZE_ALIGNMENT 1
#endif

#if defined(ENABLE_SCO_OVER_HCI) && defined(ENABLE_SCO_OVER_PCM)
#error "SCO data can either be routed over HCI or over PCM, but not over both. Please only enable ENABLE_SCO_OVER_HCI or ENABLE_SCO_OVER_PCM."
#endif

#if defined(ENABLE_SCO_OVER_HCI) && defined(HAVE_SCO_TRANSPORT)
#error "SCO data can either be routed over HCI or over PCM, but not over both. Please only enable ENABLE_SCO_OVER_HCI or HAVE_SCO_TRANSPORT."
#endif

#define HCI_CONNECTION_TIMEOUT_MS 10000

#ifndef HCI_RESET_RESEND_TIMEOUT_MS
#define HCI_RESET_RESEND_TIMEOUT_MS 200
#endif

// Names are arbitrarily shortened to 32 bytes if not requested otherwise
#ifndef GAP_INQUIRY_MAX_NAME_LEN
#define GAP_INQUIRY_MAX_NAME_LEN 32
#endif

// GAP inquiry state: 0 = off, 0x01 - 0x30 = requested duration, 0xfe = active, 0xff = stop requested
#define GAP_INQUIRY_DURATION_MIN       0x01
#define GAP_INQUIRY_DURATION_MAX       0x30
#define GAP_INQUIRY_MIN_PERIODIC_LEN_MIN 0x02
#define GAP_INQUIRY_MAX_PERIODIC_LEN_MIN 0x03
#define GAP_INQUIRY_STATE_IDLE         0x00
#define GAP_INQUIRY_STATE_W4_ACTIVE    0x80
#define GAP_INQUIRY_STATE_ACTIVE       0x81
#define GAP_INQUIRY_STATE_W2_CANCEL    0x82
#define GAP_INQUIRY_STATE_W4_CANCELLED 0x83
#define GAP_INQUIRY_STATE_PERIODIC     0x84
#define GAP_INQUIRY_STATE_W2_EXIT_PERIODIC 0x85

// GAP Remote Name Request
#define GAP_REMOTE_NAME_STATE_IDLE 0
#define GAP_REMOTE_NAME_STATE_W2_SEND 1
#define GAP_REMOTE_NAME_STATE_W4_COMPLETE 2

// GAP Pairing
#define GAP_PAIRING_STATE_IDLE                       0
#define GAP_PAIRING_STATE_SEND_PIN                   1
#define GAP_PAIRING_STATE_SEND_PIN_NEGATIVE          2
#define GAP_PAIRING_STATE_SEND_PASSKEY               3
#define GAP_PAIRING_STATE_SEND_PASSKEY_NEGATIVE      4
#define GAP_PAIRING_STATE_SEND_CONFIRMATION          5
#define GAP_PAIRING_STATE_SEND_CONFIRMATION_NEGATIVE 6
#define GAP_PAIRING_STATE_WAIT_FOR_COMMAND_COMPLETE  7

//
// compact storage of relevant supported HCI Commands.
// X-Macro below provides enumeration and mapping table into the supported
// commands bitmap (64 bytes) from HCI Read Local Supported Commands
//

// format: command name, byte offset, bit nr in 64-byte supported commands
// currently stored in 32-bit variable
#define SUPPORTED_HCI_COMMANDS \
    X( SUPPORTED_HCI_COMMAND_READ_REMOTE_EXTENDED_FEATURES         ,  2, 5) \
    X( SUPPORTED_HCI_COMMAND_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE , 10, 4) \
    X( SUPPORTED_HCI_COMMAND_READ_BUFFER_SIZE                      , 14, 7) \
    X( SUPPORTED_HCI_COMMAND_WRITE_DEFAULT_ERRONEOUS_DATA_REPORTING, 18, 3) \
    X( SUPPORTED_HCI_COMMAND_READ_ENCRYPTION_KEY_SIZE              , 20, 4) \
    X( SUPPORTED_HCI_COMMAND_SET_EVENT_MASK_PAGE_2                 , 22, 2) \
    X( SUPPORTED_HCI_COMMAND_WRITE_LE_HOST_SUPPORTED               , 24, 6) \
    X( SUPPORTED_HCI_COMMAND_REMOTE_OOB_EXTENDED_DATA_REQUEST_REPLY, 32, 1) \
    X( SUPPORTED_HCI_COMMAND_WRITE_SECURE_CONNECTIONS_HOST         , 32, 3) \
    X( SUPPORTED_HCI_COMMAND_READ_LOCAL_OOB_EXTENDED_DATA_COMMAND  , 32, 6) \
    X( SUPPORTED_HCI_COMMAND_LE_WRITE_SUGGESTED_DEFAULT_DATA_LENGTH, 34, 0) \
    X( SUPPORTED_HCI_COMMAND_LE_SET_ADDRESS_RESOLUTION_ENABLE      , 35, 1) \
    X( SUPPORTED_HCI_COMMAND_LE_READ_MAXIMUM_DATA_LENGTH           , 35, 3) \
    X( SUPPORTED_HCI_COMMAND_LE_SET_DEFAULT_PHY                    , 35, 5) \
    X( SUPPORTED_HCI_COMMAND_LE_SET_EXTENDED_ADVERTISING_ENABLE    , 36, 6) \
    X( SUPPORTED_HCI_COMMAND_LE_READ_BUFFER_SIZE_V2                , 41, 5) \
    X( SUPPORTED_HCI_COMMAND_SET_MIN_ENCRYPTION_KEY_SIZE           , 45, 7) \

// enumerate supported commands
#define X(name, offset, bit) name,
enum {
    SUPPORTED_HCI_COMMANDS
    SUPPORTED_HCI_COMMANDS_COUNT
};
#undef X

// prototypes
#ifdef ENABLE_CLASSIC
static void hci_update_scan_enable(void);
static void hci_emit_scan_mode_changed(uint8_t discoverable, uint8_t connectable);
static int  hci_local_ssp_activated(void);
static bool hci_remote_ssp_supported(hci_con_handle_t con_handle);
static bool hci_ssp_supported(hci_connection_t * connection);
static void hci_notify_if_sco_can_send_now(void);
static void hci_emit_connection_complete(bd_addr_t address, hci_con_handle_t con_handle, uint8_t status);
static gap_security_level_t gap_security_level_for_connection(hci_connection_t * connection);
static void hci_emit_security_level(hci_con_handle_t con_handle, gap_security_level_t level);
static void hci_connection_timeout_handler(btstack_timer_source_t *timer);
static void hci_connection_timestamp(hci_connection_t *connection);
static void hci_emit_l2cap_check_timeout(hci_connection_t *conn);
static void gap_inquiry_explode(uint8_t *packet, uint16_t size);
#endif

static int  hci_power_control_on(void);
static void hci_power_control_off(void);
static void hci_state_reset(void);
static void hci_halting_timeout_handler(btstack_timer_source_t * ds);
static void hci_emit_transport_packet_sent(void);
static void hci_emit_disconnection_complete(hci_con_handle_t con_handle, uint8_t reason);
static void hci_emit_nr_connections_changed(void);
static void hci_emit_hci_open_failed(void);
static void hci_emit_dedicated_bonding_result(bd_addr_t address, uint8_t status);
static void hci_emit_event(uint8_t * event, uint16_t size, int dump);
static void hci_emit_acl_packet(uint8_t * packet, uint16_t size);
static void hci_run(void);
static int  hci_is_le_connection(hci_connection_t * connection);

#ifdef ENABLE_CLASSIC
static int hci_have_usb_transport(void);
static void hci_trigger_remote_features_for_connection(hci_connection_t * connection);
#endif

#ifdef ENABLE_BLE
#ifdef ENABLE_LE_CENTRAL
// called from test/ble_client/advertising_data_parser.c
void le_handle_advertisement_report(uint8_t *packet, uint16_t size);
static uint8_t hci_whitelist_remove(bd_addr_type_t address_type, const bd_addr_t address);
static hci_connection_t * gap_get_outgoing_connection(void);
static void hci_le_scan_stop(void);
static bool hci_run_general_gap_le(void);
#endif
#ifdef ENABLE_LE_PERIPHERAL
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
static le_advertising_set_t * hci_advertising_set_for_handle(uint8_t advertising_handle);
#endif /* ENABLE_LE_EXTENDED_ADVERTISING */
#endif /* ENABLE_LE_PERIPHERAL */
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
static hci_iso_stream_t * hci_iso_stream_create(hci_iso_type_t iso_type, hci_iso_stream_state_t state, uint8_t group_id, uint8_t stream_id);
static void hci_iso_stream_finalize(hci_iso_stream_t * iso_stream);
static void hci_iso_stream_finalize_by_type_and_group_id(hci_iso_type_t iso_type, uint8_t group_id);
static hci_iso_stream_t * hci_iso_stream_for_con_handle(hci_con_handle_t con_handle);
static void hci_iso_stream_requested_finalize(uint8_t big_handle);
static void hci_iso_stream_requested_confirm(uint8_t big_handle);
static void hci_iso_packet_handler(uint8_t * packet, uint16_t size);
static le_audio_big_t * hci_big_for_handle(uint8_t big_handle);
static le_audio_cig_t * hci_cig_for_id(uint8_t cig_id);
static void hci_iso_notify_can_send_now(void);
static void hci_emit_big_created(const le_audio_big_t * big, uint8_t status);
static void hci_emit_big_terminated(const le_audio_big_t * big);
static void hci_emit_big_sync_created(const le_audio_big_sync_t * big_sync, uint8_t status);
static void hci_emit_big_sync_stopped(uint8_t big_handle);
static void hci_emit_cig_created(const le_audio_cig_t * cig, uint8_t status);
static void hci_cis_handle_created(hci_iso_stream_t * iso_stream, uint8_t status);
static le_audio_big_sync_t * hci_big_sync_for_handle(uint8_t big_handle);

#endif /* ENABLE_LE_ISOCHRONOUS_STREAMS */
#endif /* ENABLE_BLE */

// the STACK is here
#ifndef HAVE_MALLOC
static hci_stack_t   hci_stack_static;
#endif
static hci_stack_t * hci_stack = NULL;

#ifdef ENABLE_CLASSIC
// default name
static const char * default_classic_name = "BTstack 00:00:00:00:00:00";

// test helper
static uint8_t disable_l2cap_timeouts = 0;
#endif

// reset connection state on create and on reconnect
// don't overwrite addr, con handle, role
static void hci_connection_init(hci_connection_t * conn){
    conn->authentication_flags = AUTH_FLAG_NONE;
    conn->bonding_flags = 0;
    conn->requested_security_level = LEVEL_0;
#ifdef ENABLE_CLASSIC
    conn->request_role = HCI_ROLE_INVALID;
    conn->sniff_subrating_max_latency = 0xffff;
    conn->qos_service_type = HCI_SERVICE_TYPE_INVALID;
    conn->link_key_type = INVALID_LINK_KEY;
    btstack_run_loop_set_timer_handler(&conn->timeout, hci_connection_timeout_handler);
    btstack_run_loop_set_timer_context(&conn->timeout, conn);
    hci_connection_timestamp(conn);
#endif
    conn->acl_recombination_length = 0;
    conn->acl_recombination_pos = 0;
    conn->num_packets_sent = 0;

    conn->le_con_parameter_update_state = CON_PARAMETER_UPDATE_NONE;
#ifdef ENABLE_BLE
    conn->le_phy_update_all_phys = 0xff;
#endif
#ifdef ENABLE_LE_LIMIT_ACL_FRAGMENT_BY_MAX_OCTETS
    conn->le_max_tx_octets = 27;
#endif
#ifdef ENABLE_CLASSIC_PAIRING_OOB
    conn->classic_oob_c_192 = NULL;
    conn->classic_oob_r_192 = NULL;
    conn->classic_oob_c_256 = NULL;
    conn->classic_oob_r_256 = NULL;
#endif
}

/**
 * create connection for given address
 *
 * @return connection OR NULL, if no memory left
 */
static hci_connection_t *
create_connection_for_bd_addr_and_type(const bd_addr_t addr, bd_addr_type_t addr_type, hci_role_t role) {
    log_info("create_connection_for_addr %s, type %x", bd_addr_to_str(addr), addr_type);

    hci_connection_t * conn = btstack_memory_hci_connection_get();
    if (!conn) return NULL;
    hci_connection_init(conn);

    bd_addr_copy(conn->address, addr);
    conn->address_type = addr_type;
    conn->con_handle = HCI_CON_HANDLE_INVALID;
    conn->role = role;
#ifdef ENABLE_LE_PERIODIC_ADVERTISING
    conn->le_past_sync_handle = HCI_CON_HANDLE_INVALID;
#endif
    btstack_linked_list_add(&hci_stack->connections, (btstack_linked_item_t *) conn);

    return conn;
}


/**
 * get le connection parameter range
*
 * @return le connection parameter range struct
 */
void gap_get_connection_parameter_range(le_connection_parameter_range_t * range){
    *range = hci_stack->le_connection_parameter_range;
}

/**
 * set le connection parameter range
 *
 */

void gap_set_connection_parameter_range(le_connection_parameter_range_t *range){
    hci_stack->le_connection_parameter_range = *range;
}

/**
 * @brief Test if connection parameters are inside in existing rage
 * @param conn_interval_min (unit: 1.25ms)
 * @param conn_interval_max (unit: 1.25ms)
 * @param conn_latency
 * @param supervision_timeout (unit: 10ms)
 * @return 1 if included
 */
int gap_connection_parameter_range_included(le_connection_parameter_range_t * existing_range, uint16_t le_conn_interval_min, uint16_t le_conn_interval_max, uint16_t le_conn_latency, uint16_t le_supervision_timeout){
    if (le_conn_interval_min < existing_range->le_conn_interval_min) return 0;
    if (le_conn_interval_max > existing_range->le_conn_interval_max) return 0;

    if (le_conn_latency < existing_range->le_conn_latency_min) return 0;
    if (le_conn_latency > existing_range->le_conn_latency_max) return 0;

    if (le_supervision_timeout < existing_range->le_supervision_timeout_min) return 0;
    if (le_supervision_timeout > existing_range->le_supervision_timeout_max) return 0;

    return 1;
}

/**
 * @brief Set max number of connections in LE Peripheral role (if Bluetooth Controller supports it)
 * @note: default: 1
 * @param max_peripheral_connections
 */
#ifdef ENABLE_LE_PERIPHERAL
void gap_set_max_number_peripheral_connections(int max_peripheral_connections){
    hci_stack->le_max_number_peripheral_connections = max_peripheral_connections;
}
#endif

/**
 * get hci connections iterator
 *
 * @return hci connections iterator
 */

void hci_connections_get_iterator(btstack_linked_list_iterator_t *it){
    btstack_linked_list_iterator_init(it, &hci_stack->connections);
}

/**
 * get connection for a given handle
 *
 * @return connection OR NULL, if not found
 */
hci_connection_t * hci_connection_for_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hci_connection_t * item = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
        if ( item->con_handle == con_handle ) {
            return item;
        }
    } 
    return NULL;
}

/**
 * get connection for given address
 *
 * @return connection OR NULL, if not found
 */
hci_connection_t * hci_connection_for_bd_addr_and_type(const bd_addr_t  addr, bd_addr_type_t addr_type){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hci_connection_t * connection = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
        if (connection->address_type != addr_type)  continue;
        if (memcmp(addr, connection->address, 6) != 0) continue;
        return connection;   
    } 
    return NULL;
}

#ifdef ENABLE_CLASSIC

inline static void connectionClearAuthenticationFlags(hci_connection_t * conn, hci_authentication_flags_t flags){
    conn->authentication_flags = (hci_authentication_flags_t)(conn->authentication_flags & ~flags);
}

inline static void connectionSetAuthenticationFlags(hci_connection_t * conn, hci_authentication_flags_t flags){
    conn->authentication_flags = (hci_authentication_flags_t)(conn->authentication_flags | flags);
}

#ifdef ENABLE_SCO_OVER_HCI
static int hci_number_sco_connections(void){
    int connections = 0;
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hci_connection_t * connection = (hci_connection_t *) btstack_linked_list_iterator_next(&it);
        if (connection->address_type != BD_ADDR_TYPE_SCO) continue;
        connections++;
    } 
    return connections;
}
#endif

static void hci_connection_timeout_handler(btstack_timer_source_t *timer){
    hci_connection_t * connection = (hci_connection_t *) btstack_run_loop_get_timer_context(timer);
#ifdef HAVE_EMBEDDED_TICK
    if (btstack_run_loop_embedded_get_ticks() > connection->timestamp + btstack_run_loop_embedded_ticks_for_ms(HCI_CONNECTION_TIMEOUT_MS)){
        // connections might be timed out
        hci_emit_l2cap_check_timeout(connection);
    }
#else 
    if (btstack_run_loop_get_time_ms() > (connection->timestamp + HCI_CONNECTION_TIMEOUT_MS)){
        // connections might be timed out
        hci_emit_l2cap_check_timeout(connection);
    }
#endif
}

static void hci_connection_timestamp(hci_connection_t *connection){
#ifdef HAVE_EMBEDDED_TICK
    connection->timestamp = btstack_run_loop_embedded_get_ticks();
#else
    connection->timestamp = btstack_run_loop_get_time_ms();
#endif
}

/**
 * add authentication flags and reset timer
 * @note: assumes classic connection
 * @note: bd_addr is passed in as litle endian uint8_t * as it is called from parsing packets
 */
static void hci_add_connection_flags_for_flipped_bd_addr(uint8_t *bd_addr, hci_authentication_flags_t flags){
    bd_addr_t addr;
    reverse_bd_addr(bd_addr, addr);
    hci_connection_t * conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
    if (conn) {
        connectionSetAuthenticationFlags(conn, flags);
        hci_connection_timestamp(conn);
    }
}

static bool hci_pairing_active(hci_connection_t * hci_connection){
    return (hci_connection->authentication_flags & AUTH_FLAG_PAIRING_ACTIVE_MASK) != 0;
}

static void hci_pairing_started(hci_connection_t * hci_connection, bool ssp){
    if (hci_pairing_active(hci_connection)) return;
    if (ssp){
        hci_connection->authentication_flags |= AUTH_FLAG_SSP_PAIRING_ACTIVE;
    } else {
        hci_connection->authentication_flags |= AUTH_FLAG_LEGACY_PAIRING_ACTIVE;
    }
    // if we are initiator, we have sent an HCI Authenticate Request
    bool initiator = (hci_connection->bonding_flags & BONDING_SENT_AUTHENTICATE_REQUEST) != 0;

    // if we are responder, use minimal service security level as required level
    if (!initiator){
        hci_connection->requested_security_level = (gap_security_level_t) btstack_max((uint32_t) hci_connection->requested_security_level, (uint32_t) hci_stack->gap_minimal_service_security_level);
    }

    log_info("pairing started, ssp %u, initiator %u, requested level %u", (int) ssp, (int) initiator, hci_connection->requested_security_level);

    uint8_t event[12];
    event[0] = GAP_EVENT_PAIRING_STARTED;
    event[1] = 10;
    little_endian_store_16(event, 2, (uint16_t) hci_connection->con_handle);
    reverse_bd_addr(hci_connection->address, &event[4]);
    event[10] = (uint8_t) ssp;
    event[11] = (uint8_t) initiator;
    hci_emit_event(event, sizeof(event), 1);
}

static void hci_pairing_complete(hci_connection_t * hci_connection, uint8_t status){
    hci_connection->requested_security_level = LEVEL_0;
    if (!hci_pairing_active(hci_connection)) return;
    hci_connection->authentication_flags &= ~AUTH_FLAG_PAIRING_ACTIVE_MASK;
#ifdef ENABLE_CLASSIC_PAIRING_OOB
    hci_connection->classic_oob_c_192 = NULL;
    hci_connection->classic_oob_r_192 = NULL;
    hci_connection->classic_oob_c_256 = NULL;
    hci_connection->classic_oob_r_256 = NULL;
#endif
    log_info("pairing complete, status %02x", status);

    uint8_t event[11];
    event[0] = GAP_EVENT_PAIRING_COMPLETE;
    event[1] = 9;
    little_endian_store_16(event, 2, (uint16_t) hci_connection->con_handle);
    reverse_bd_addr(hci_connection->address, &event[4]);
    event[10] = status;
    hci_emit_event(event, sizeof(event), 1);

    // emit dedicated bonding done on failure, otherwise verify that connection can be encrypted
    if ((status != ERROR_CODE_SUCCESS) && ((hci_connection->bonding_flags & BONDING_DEDICATED) != 0)){
        hci_connection->bonding_flags &= ~BONDING_DEDICATED;
        hci_connection->bonding_flags |= BONDING_DISCONNECT_DEDICATED_DONE;
        hci_connection->bonding_status = status;
    }
}

bool hci_authentication_active_for_handle(hci_con_handle_t handle){
    hci_connection_t * conn = hci_connection_for_handle(handle);
    if (!conn) return false;
    return hci_pairing_active(conn);
}

void gap_drop_link_key_for_bd_addr(bd_addr_t addr){
    if (!hci_stack->link_key_db) return;
    log_info("gap_drop_link_key_for_bd_addr: %s", bd_addr_to_str(addr));
    hci_stack->link_key_db->delete_link_key(addr);
}

void gap_store_link_key_for_bd_addr(bd_addr_t addr, link_key_t link_key, link_key_type_t type){
    if (!hci_stack->link_key_db) return;
    log_info("gap_store_link_key_for_bd_addr: %s, type %u", bd_addr_to_str(addr), type);
    hci_stack->link_key_db->put_link_key(addr, link_key, type);
}

bool gap_get_link_key_for_bd_addr(bd_addr_t addr, link_key_t link_key, link_key_type_t * type){
	if (!hci_stack->link_key_db) return false;
	int result = hci_stack->link_key_db->get_link_key(addr, link_key, type) != 0;
	log_info("link key for %s available %u, type %u", bd_addr_to_str(addr), result, (int) *type);
	return result;
}

void gap_delete_all_link_keys(void){
    bd_addr_t  addr;
    link_key_t link_key;
    link_key_type_t type;
    btstack_link_key_iterator_t it;
    int ok = gap_link_key_iterator_init(&it);
    if (!ok) {
        log_error("could not initialize iterator");
        return;
    }
    while (gap_link_key_iterator_get_next(&it, addr, link_key, &type)){
        gap_drop_link_key_for_bd_addr(addr);
    }
    gap_link_key_iterator_done(&it);        
}

int gap_link_key_iterator_init(btstack_link_key_iterator_t * it){
    if (!hci_stack->link_key_db) return 0;
    if (!hci_stack->link_key_db->iterator_init) return 0;
    return hci_stack->link_key_db->iterator_init(it);
}
int gap_link_key_iterator_get_next(btstack_link_key_iterator_t * it, bd_addr_t bd_addr, link_key_t link_key, link_key_type_t * type){
    if (!hci_stack->link_key_db) return 0;
    return hci_stack->link_key_db->iterator_get_next(it, bd_addr, link_key, type);
}
void gap_link_key_iterator_done(btstack_link_key_iterator_t * it){
    if (!hci_stack->link_key_db) return;
    hci_stack->link_key_db->iterator_done(it);
}
#endif

static bool hci_is_le_connection_type(bd_addr_type_t address_type){
    switch (address_type){
        case BD_ADDR_TYPE_LE_PUBLIC:
        case BD_ADDR_TYPE_LE_RANDOM:
        case BD_ADDR_TYPE_LE_PUBLIC_IDENTITY:
        case BD_ADDR_TYPE_LE_RANDOM_IDENTITY:
            return true;
        default:
            return false;
    }
}

static int hci_is_le_connection(hci_connection_t * connection){
    return hci_is_le_connection_type(connection->address_type);
}

/**
 * count connections
 */
static int nr_hci_connections(void){
    int count = 0;
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) hci_stack->connections; it != NULL ; it = it->next){
        count++;
    }
    return count;
}

uint16_t hci_number_free_acl_slots_for_connection_type(bd_addr_type_t address_type){
    
    unsigned int num_packets_sent_classic = 0;
    unsigned int num_packets_sent_le = 0;

    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) hci_stack->connections; it != NULL; it = it->next){
        hci_connection_t * connection = (hci_connection_t *) it;
        if (hci_is_le_connection(connection)){
            num_packets_sent_le += connection->num_packets_sent;
        }
        if (connection->address_type == BD_ADDR_TYPE_ACL){
            num_packets_sent_classic += connection->num_packets_sent;
        }
    }
    log_debug("ACL classic buffers: %u used of %u", num_packets_sent_classic, hci_stack->acl_packets_total_num);
    int free_slots_classic = hci_stack->acl_packets_total_num - num_packets_sent_classic;
    int free_slots_le = 0;

    if (free_slots_classic < 0){
        log_error("hci_number_free_acl_slots: outgoing classic packets (%u) > total classic packets (%u)", num_packets_sent_classic, hci_stack->acl_packets_total_num);
        return 0;
    }

    if (hci_stack->le_acl_packets_total_num){
        // if we have LE slots, they are used
        free_slots_le = hci_stack->le_acl_packets_total_num - num_packets_sent_le;
        if (free_slots_le < 0){
            log_error("hci_number_free_acl_slots: outgoing le packets (%u) > total le packets (%u)", num_packets_sent_le, hci_stack->le_acl_packets_total_num);
            return 0;
        }
    } else {
        // otherwise, classic slots are used for LE, too
        free_slots_classic -= num_packets_sent_le;
        if (free_slots_classic < 0){
            log_error("hci_number_free_acl_slots: outgoing classic + le packets (%u + %u) > total packets (%u)", num_packets_sent_classic, num_packets_sent_le, hci_stack->acl_packets_total_num);
            return 0;
        }
    }

    switch (address_type){
        case BD_ADDR_TYPE_UNKNOWN:
            log_error("hci_number_free_acl_slots: unknown address type");
            return 0;

        case BD_ADDR_TYPE_ACL:
            return (uint16_t) free_slots_classic;

        default:
           if (hci_stack->le_acl_packets_total_num > 0){
               return (uint16_t) free_slots_le;
           }
           return (uint16_t) free_slots_classic;
    }
}

int hci_number_free_acl_slots_for_handle(hci_con_handle_t con_handle){
    // get connection type
    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection){
        log_error("hci_number_free_acl_slots: handle 0x%04x not in connection list", con_handle);
        return 0;
    }
    return hci_number_free_acl_slots_for_connection_type(connection->address_type);
}

#ifdef ENABLE_CLASSIC
static int hci_number_free_sco_slots(void){
    unsigned int num_sco_packets_sent  = 0;
    btstack_linked_item_t *it;
    if (hci_stack->synchronous_flow_control_enabled){
        // explicit flow control
        for (it = (btstack_linked_item_t *) hci_stack->connections; it ; it = it->next){
            hci_connection_t * connection = (hci_connection_t *) it;
            if (connection->address_type != BD_ADDR_TYPE_SCO) continue;
            num_sco_packets_sent += connection->num_packets_sent;
        }
        if (num_sco_packets_sent > hci_stack->sco_packets_total_num){
            log_info("hci_number_free_sco_slots:packets (%u) > total packets (%u)", num_sco_packets_sent, hci_stack->sco_packets_total_num);
            return 0;
        }
        return hci_stack->sco_packets_total_num - num_sco_packets_sent;
    } else {
        // implicit flow control -- TODO
        int num_ready = 0;
        for (it = (btstack_linked_item_t *) hci_stack->connections; it ; it = it->next){
            hci_connection_t * connection = (hci_connection_t *) it;
            if (connection->address_type != BD_ADDR_TYPE_SCO) continue;
            if (connection->sco_tx_ready == 0) continue;
            num_ready++;
        }
        return num_ready;
    }
}
#endif

// only used to send HCI Host Number Completed Packets
static int hci_can_send_comand_packet_transport(void){
    if (hci_stack->hci_packet_buffer_reserved) return 0;

    // check for async hci transport implementations
    if (hci_stack->hci_transport->can_send_packet_now){
        if (!hci_stack->hci_transport->can_send_packet_now(HCI_COMMAND_DATA_PACKET)){
            return 0;
        }
    }
    return 1;
}

// new functions replacing hci_can_send_packet_now[_using_packet_buffer]
bool hci_can_send_command_packet_now(void){
    if (hci_can_send_comand_packet_transport() == 0) return false;
    return hci_stack->num_cmd_packets > 0u;
}

static int hci_transport_can_send_prepared_packet_now(uint8_t packet_type){
    // check for async hci transport implementations
    if (!hci_stack->hci_transport->can_send_packet_now) return true;
    return hci_stack->hci_transport->can_send_packet_now(packet_type);
}

static bool hci_can_send_prepared_acl_packet_for_address_type(bd_addr_type_t address_type){
    if (!hci_transport_can_send_prepared_packet_now(HCI_ACL_DATA_PACKET)) return false;
    return hci_number_free_acl_slots_for_connection_type(address_type) > 0;
}

bool hci_can_send_acl_le_packet_now(void){
    if (hci_stack->hci_packet_buffer_reserved) return false;
    return hci_can_send_prepared_acl_packet_for_address_type(BD_ADDR_TYPE_LE_PUBLIC);
}

bool hci_can_send_prepared_acl_packet_now(hci_con_handle_t con_handle) {
    if (!hci_transport_can_send_prepared_packet_now(HCI_ACL_DATA_PACKET)) return false;
    return hci_number_free_acl_slots_for_handle(con_handle) > 0;
}

bool hci_can_send_acl_packet_now(hci_con_handle_t con_handle){
    if (hci_stack->hci_packet_buffer_reserved) return false;
    return hci_can_send_prepared_acl_packet_now(con_handle);
}

#ifdef ENABLE_CLASSIC
bool hci_can_send_acl_classic_packet_now(void){
    if (hci_stack->hci_packet_buffer_reserved) return false;
    return hci_can_send_prepared_acl_packet_for_address_type(BD_ADDR_TYPE_ACL);
}

bool hci_can_send_prepared_sco_packet_now(void){
    if (!hci_transport_can_send_prepared_packet_now(HCI_SCO_DATA_PACKET)) return false;
    if (hci_have_usb_transport()){
        return hci_stack->sco_can_send_now;
    } else {
        return hci_number_free_sco_slots() > 0;    
    }
}

bool hci_can_send_sco_packet_now(void){
    if (hci_stack->hci_packet_buffer_reserved) return false;
    return hci_can_send_prepared_sco_packet_now();
}

void hci_request_sco_can_send_now_event(void){
    hci_stack->sco_waiting_for_can_send_now = 1;
    hci_notify_if_sco_can_send_now();
}
#endif

// used for internal checks in l2cap.c
bool hci_is_packet_buffer_reserved(void){
    return hci_stack->hci_packet_buffer_reserved;
}

// reserves outgoing packet buffer. 
// @return 1 if successful
bool hci_reserve_packet_buffer(void){
    if (hci_stack->hci_packet_buffer_reserved) {
        log_error("hci_reserve_packet_buffer called but buffer already reserved");
        return false;
    }
    hci_stack->hci_packet_buffer_reserved = true;
    return true;
}

void hci_release_packet_buffer(void){
    hci_stack->hci_packet_buffer_reserved = false;
}

// assumption: synchronous implementations don't provide can_send_packet_now as they don't keep the buffer after the call
static int hci_transport_synchronous(void){
    return hci_stack->hci_transport->can_send_packet_now == NULL;
}

// used for debugging
#ifdef ENABLE_CONTROLLER_DUMP_PACKETS
static void hci_controller_dump_packets(void){
    // format: "{handle:04x}:{count:02d} "
    char summaries[3][7 * 8 + 1];
    uint16_t totals[3];
    uint8_t index;
    for (index = 0 ; index < 3 ; index++){
        summaries[index][0] = 0;
        totals[index] = 0;
    }
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) hci_stack->connections; it != NULL; it = it->next){
        hci_connection_t * connection = (hci_connection_t *) it;
        switch (connection->address_type){
            case BD_ADDR_TYPE_ACL:
                index = 0;
                break;
            case BD_ADDR_TYPE_SCO:
                index = 2;
                break;
            default:
                index = 1;
                break;
        }
        totals[index] += connection->num_packets_sent;
        char item_text[10];
        sprintf(item_text, "%04x:%02d ", connection->con_handle,connection->num_packets_sent);
        btstack_strcat(summaries[index], sizeof(summaries[0]), item_text);
    }
    for (index = 0 ; index < 3 ; index++){
        if (summaries[index][0] == 0){
            summaries[index][0] = '-';
            summaries[index][1] = 0;
        }
    }
    log_info("Controller ACL BR/EDR: %s total %u / LE: %s total %u / SCO: %s total %u", summaries[0], totals[0], summaries[1], totals[1], summaries[2], totals[2]);
}
#endif

static uint8_t hci_send_acl_packet_fragments(hci_connection_t *connection){

    // log_info("hci_send_acl_packet_fragments  %u/%u (con 0x%04x)", hci_stack->acl_fragmentation_pos, hci_stack->acl_fragmentation_total_size, connection->con_handle);

    // max ACL data packet length depends on connection type (LE vs. Classic) and available buffers
    uint16_t max_acl_data_packet_length = hci_stack->acl_data_packet_length;
    if (hci_is_le_connection(connection) && (hci_stack->le_data_packets_length > 0u)){
        max_acl_data_packet_length = hci_stack->le_data_packets_length;
    }

#ifdef ENABLE_LE_LIMIT_ACL_FRAGMENT_BY_MAX_OCTETS
    if (hci_is_le_connection(connection) && (connection->le_max_tx_octets < max_acl_data_packet_length)){
        max_acl_data_packet_length = connection->le_max_tx_octets;
    }
#endif

    log_debug("hci_send_acl_packet_fragments entered");

    uint8_t status = ERROR_CODE_SUCCESS;
    // multiple packets could be send on a synchronous HCI transport
    while (true){

        log_debug("hci_send_acl_packet_fragments loop entered");

        // get current data
        const uint16_t acl_header_pos = hci_stack->acl_fragmentation_pos - 4u;
        int current_acl_data_packet_length = hci_stack->acl_fragmentation_total_size - hci_stack->acl_fragmentation_pos;
        bool more_fragments = false;

        // if ACL packet is larger than Bluetooth packet buffer, only send max_acl_data_packet_length
        if (current_acl_data_packet_length > max_acl_data_packet_length){
            more_fragments = true;
            current_acl_data_packet_length = max_acl_data_packet_length & (~(HCI_ACL_CHUNK_SIZE_ALIGNMENT-1));
        }

        // copy handle_and_flags if not first fragment and update packet boundary flags to be 01 (continuing fragmnent)
        if (acl_header_pos > 0u){
            uint16_t handle_and_flags = little_endian_read_16(hci_stack->hci_packet_buffer, 0);
            handle_and_flags = (handle_and_flags & 0xcfffu) | (1u << 12u);
            little_endian_store_16(hci_stack->hci_packet_buffer, acl_header_pos, handle_and_flags);
        }

        // update header len
        little_endian_store_16(hci_stack->hci_packet_buffer, acl_header_pos + 2u, current_acl_data_packet_length);
        
        // count packet
        connection->num_packets_sent++;
        log_debug("hci_send_acl_packet_fragments loop before send (more fragments %d)", (int) more_fragments);

        // update state for next fragment (if any) as "transport done" might be sent during send_packet already
        if (more_fragments){
            // update start of next fragment to send
            hci_stack->acl_fragmentation_pos += current_acl_data_packet_length;
        } else {
            // done
            hci_stack->acl_fragmentation_pos = 0;
            hci_stack->acl_fragmentation_total_size = 0;
        }

        // send packet
        uint8_t * packet = &hci_stack->hci_packet_buffer[acl_header_pos];
        const int size = current_acl_data_packet_length + 4;
        hci_dump_packet(HCI_ACL_DATA_PACKET, 0, packet, size);
        hci_stack->acl_fragmentation_tx_active = 1;
        int err = hci_stack->hci_transport->send_packet(HCI_ACL_DATA_PACKET, packet, size);
        if (err != 0){
            // no error from HCI Transport expected
            status = ERROR_CODE_HARDWARE_FAILURE;
        }

#ifdef ENABLE_CONTROLLER_DUMP_PACKETS
        hci_controller_dump_packets();
#endif

        log_debug("hci_send_acl_packet_fragments loop after send (more fragments %d)", (int) more_fragments);

        // done yet?
        if (!more_fragments) break;

        // can send more?
        if (!hci_can_send_prepared_acl_packet_now(connection->con_handle)) return status;
    }

    log_debug("hci_send_acl_packet_fragments loop over");

    // release buffer now for synchronous transport
    if (hci_transport_synchronous()){
        hci_stack->acl_fragmentation_tx_active = 0;
        hci_release_packet_buffer();
        hci_emit_transport_packet_sent();
    }

    return status;
}

// pre: caller has reserved the packet buffer
uint8_t hci_send_acl_packet_buffer(int size){
    btstack_assert(hci_stack->hci_packet_buffer_reserved);

    uint8_t * packet = hci_stack->hci_packet_buffer;
    hci_con_handle_t con_handle = READ_ACL_CONNECTION_HANDLE(packet);

    hci_connection_t *connection = hci_connection_for_handle( con_handle);
    if (!connection) {
        log_error("hci_send_acl_packet_buffer called but no connection for handle 0x%04x", con_handle);
        hci_release_packet_buffer();
        hci_emit_transport_packet_sent();
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    // check for free places on Bluetooth module
    if (!hci_can_send_prepared_acl_packet_now(con_handle)) {
        log_error("hci_send_acl_packet_buffer called but no free ACL buffers on controller");
        hci_release_packet_buffer();
        hci_emit_transport_packet_sent();
        return BTSTACK_ACL_BUFFERS_FULL;
    }

#ifdef ENABLE_CLASSIC
    hci_connection_timestamp(connection);
#endif

    // hci_dump_packet( HCI_ACL_DATA_PACKET, 0, packet, size);

    // setup data
    hci_stack->acl_fragmentation_total_size = size;
    hci_stack->acl_fragmentation_pos = 4;   // start of L2CAP packet

    return hci_send_acl_packet_fragments(connection);
}

#ifdef ENABLE_CLASSIC
// pre: caller has reserved the packet buffer
uint8_t hci_send_sco_packet_buffer(int size){
    btstack_assert(hci_stack->hci_packet_buffer_reserved);

    uint8_t * packet = hci_stack->hci_packet_buffer;

    // skip checks in loopback mode
    if (!hci_stack->loopback_mode){
        hci_con_handle_t con_handle = READ_ACL_CONNECTION_HANDLE(packet);   // same for ACL and SCO

        // check for free places on Bluetooth module
        if (!hci_can_send_prepared_sco_packet_now()) {
            log_error("hci_send_sco_packet_buffer called but no free SCO buffers on controller");
            hci_release_packet_buffer();
            hci_emit_transport_packet_sent();
            return BTSTACK_ACL_BUFFERS_FULL;
        }

        // track send packet in connection struct
        hci_connection_t *connection = hci_connection_for_handle( con_handle);
        if (!connection) {
            log_error("hci_send_sco_packet_buffer called but no connection for handle 0x%04x", con_handle);
            hci_release_packet_buffer();
            hci_emit_transport_packet_sent();
            return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
        }

        if (hci_have_usb_transport()){
            // token used
            hci_stack->sco_can_send_now = false;
        } else {
            if (hci_stack->synchronous_flow_control_enabled){
                connection->num_packets_sent++;
            } else {
                connection->sco_tx_ready--;
            }
        }
    }

    hci_dump_packet( HCI_SCO_DATA_PACKET, 0, packet, size);

#ifdef HAVE_SCO_TRANSPORT
    hci_stack->sco_transport->send_packet(packet, size);
    hci_release_packet_buffer();
    hci_emit_transport_packet_sent();

    return 0;
#else
    int err = hci_stack->hci_transport->send_packet(HCI_SCO_DATA_PACKET, packet, size);
    if (hci_transport_synchronous()){
        hci_release_packet_buffer();
        hci_emit_transport_packet_sent();
    }

    if (err != 0){
        return ERROR_CODE_HARDWARE_FAILURE;
    }
    return ERROR_CODE_SUCCESS;
#endif
}
#endif

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
static uint8_t hci_send_iso_packet_fragments(void){

    uint16_t max_iso_data_packet_length = hci_stack->le_iso_packets_length;
    uint8_t status = ERROR_CODE_SUCCESS;
    // multiple packets could be send on a synchronous HCI transport
    while (true){

        // get current data
        const uint16_t iso_header_pos = hci_stack->iso_fragmentation_pos - 4u;
        int current_iso_data_packet_length = hci_stack->iso_fragmentation_total_size - hci_stack->iso_fragmentation_pos;
        bool more_fragments = false;

        // if ISO packet is larger than Bluetooth packet buffer, only send max_acl_data_packet_length
        if (current_iso_data_packet_length > max_iso_data_packet_length){
            more_fragments = true;
            current_iso_data_packet_length = max_iso_data_packet_length;
        }

        // copy handle_and_flags if not first fragment and update packet boundary flags to be 01 (continuing fragmnent)
        uint16_t handle_and_flags = little_endian_read_16(hci_stack->hci_packet_buffer, 0);
        uint8_t pb_flags;
        if (iso_header_pos == 0u){
            // first fragment, keep TS field
            pb_flags = more_fragments ? 0x00 : 0x02;
            handle_and_flags = (handle_and_flags & 0x4fffu) | (pb_flags << 12u);
        } else {
            // later fragment, drop TS field
            pb_flags = more_fragments ? 0x01 : 0x03;
            handle_and_flags = (handle_and_flags & 0x0fffu) | (pb_flags << 12u);
        }
        little_endian_store_16(hci_stack->hci_packet_buffer, iso_header_pos, handle_and_flags);

        // update header len
        little_endian_store_16(hci_stack->hci_packet_buffer, iso_header_pos + 2u, current_iso_data_packet_length);

        // update state for next fragment (if any) as "transport done" might be sent during send_packet already
        if (more_fragments){
            // update start of next fragment to send
            hci_stack->iso_fragmentation_pos += current_iso_data_packet_length;
        } else {
            // done
            hci_stack->iso_fragmentation_pos = 0;
            hci_stack->iso_fragmentation_total_size = 0;
        }

        // send packet
        uint8_t * packet = &hci_stack->hci_packet_buffer[iso_header_pos];
        const int size = current_iso_data_packet_length + 4;
        hci_dump_packet(HCI_ISO_DATA_PACKET, 0, packet, size);
        hci_stack->iso_fragmentation_tx_active = true;
        int err = hci_stack->hci_transport->send_packet(HCI_ISO_DATA_PACKET, packet, size);
        if (err != 0){
            // no error from HCI Transport expected
            status = ERROR_CODE_HARDWARE_FAILURE;
        }

        // done yet?
        if (!more_fragments) break;

        // can send more?
        if (!hci_transport_can_send_prepared_packet_now(HCI_ISO_DATA_PACKET)) return false;
    }

    // release buffer now for synchronous transport
    if (hci_transport_synchronous()){
        hci_stack->iso_fragmentation_tx_active = false;
        hci_release_packet_buffer();
        hci_emit_transport_packet_sent();
    }

    return status;
}

uint8_t hci_send_iso_packet_buffer(uint16_t size){
    btstack_assert(hci_stack->hci_packet_buffer_reserved);

    hci_con_handle_t con_handle = (hci_con_handle_t) little_endian_read_16(hci_stack->hci_packet_buffer, 0) & 0xfff;
    hci_iso_stream_t * iso_stream = hci_iso_stream_for_con_handle(con_handle);
    if (iso_stream == NULL){
        hci_release_packet_buffer();
        hci_iso_notify_can_send_now();
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    // TODO: check for space on controller

    // skip iso packets if needed
    if (iso_stream->num_packets_to_skip > 0){
        iso_stream->num_packets_to_skip--;
        // pretend it was processed and trigger next one
        hci_release_packet_buffer();
        hci_iso_notify_can_send_now();
        return ERROR_CODE_SUCCESS;
    }

    // track outgoing packet sent
    log_info("Outgoing ISO packet for con handle 0x%04x", con_handle);
    iso_stream->num_packets_sent++;

    // setup data
    hci_stack->iso_fragmentation_total_size = size;
    hci_stack->iso_fragmentation_pos = 4;   // start of L2CAP packet

    return hci_send_iso_packet_fragments();
}
#endif

static void acl_handler(uint8_t *packet, uint16_t size){

    // get info
    hci_con_handle_t con_handle = READ_ACL_CONNECTION_HANDLE(packet);
    hci_connection_t *conn      = hci_connection_for_handle(con_handle);
    uint8_t  acl_flags          = READ_ACL_FLAGS(packet);
    uint16_t acl_length         = READ_ACL_LENGTH(packet);

    // ignore non-registered handle
    if (!conn){
        log_error("acl_handler called with non-registered handle %u!" , con_handle);
        return;
    }

    // assert packet is complete    
    if ((acl_length + 4u) != size){
        log_error("acl_handler called with ACL packet of wrong size %d, expected %u => dropping packet", size, acl_length + 4);
        return;
    }

#ifdef ENABLE_CLASSIC
    // update idle timestamp
    hci_connection_timestamp(conn);
#endif

#ifdef ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
    hci_stack->host_completed_packets = 1;
    conn->num_packets_completed++;
#endif

    // handle different packet types
    switch (acl_flags & 0x03u) {
            
        case 0x01: // continuation fragment
            
            // sanity checks
            if (conn->acl_recombination_pos == 0u) {
                log_error( "ACL Cont Fragment but no first fragment for handle 0x%02x", con_handle);
                return;
            }
            if ((conn->acl_recombination_pos + acl_length) > (4u + HCI_ACL_BUFFER_SIZE)){
                log_error( "ACL Cont Fragment to large: combined packet %u > buffer size %u for handle 0x%02x",
                    conn->acl_recombination_pos + acl_length, 4 + HCI_ACL_BUFFER_SIZE, con_handle);
                conn->acl_recombination_pos = 0;
                return;
            }

            // append fragment payload (header already stored)
            (void)memcpy(&conn->acl_recombination_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + conn->acl_recombination_pos],
                         &packet[4], acl_length);
            conn->acl_recombination_pos += acl_length;

            // forward complete L2CAP packet if complete. 
            if (conn->acl_recombination_pos >= (conn->acl_recombination_length + 4u + 4u)){ // pos already incl. ACL header
                hci_emit_acl_packet(&conn->acl_recombination_buffer[HCI_INCOMING_PRE_BUFFER_SIZE], conn->acl_recombination_pos);
                // reset recombination buffer
                conn->acl_recombination_length = 0;
                conn->acl_recombination_pos = 0;
            }
            break;
            
        case 0x02: { // first fragment
            
            // sanity check
            if (conn->acl_recombination_pos) {
                // we just received the first fragment, but still have data. Only warn if the packet wasn't a flushable packet
                if ((conn->acl_recombination_buffer[HCI_INCOMING_PRE_BUFFER_SIZE+1] >> 4) != 0x02){
                    log_error( "ACL First Fragment but %u bytes in buffer for handle 0x%02x, dropping stale fragments", conn->acl_recombination_pos, con_handle);
                }
                conn->acl_recombination_pos = 0;
            }

            // peek into L2CAP packet!
            uint16_t l2cap_length = READ_L2CAP_LENGTH( packet );

            // compare fragment size to L2CAP packet size
            if (acl_length >= (l2cap_length + 4u)){
                // forward fragment as L2CAP packet
                hci_emit_acl_packet(packet, acl_length + 4u);
            } else {

                if (acl_length > HCI_ACL_BUFFER_SIZE){
                    log_error( "ACL First Fragment to large: fragment %u > buffer size %u for handle 0x%02x",
                        4 + acl_length, 4 + HCI_ACL_BUFFER_SIZE, con_handle);
                    return;
                }

                // store first fragment and tweak acl length for complete package
                (void)memcpy(&conn->acl_recombination_buffer[HCI_INCOMING_PRE_BUFFER_SIZE],
                             packet, acl_length + 4u);
                conn->acl_recombination_pos    = acl_length + 4u;
                conn->acl_recombination_length = l2cap_length;
                little_endian_store_16(conn->acl_recombination_buffer, HCI_INCOMING_PRE_BUFFER_SIZE + 2u, l2cap_length +4u);
            }
            break;
            
        } 
        default:
            log_error( "acl_handler called with invalid packet boundary flags %u", acl_flags & 0x03);
            return;
    }
    
    // execute main loop
    hci_run();
}

static void hci_connection_stop_timer(hci_connection_t * conn){
    btstack_run_loop_remove_timer(&conn->timeout);
#ifdef ENABLE_CLASSIC
    btstack_run_loop_remove_timer(&conn->timeout_sco);
#endif
}

static void hci_shutdown_connection(hci_connection_t *conn){
    log_info("Connection closed: handle 0x%x, %s", conn->con_handle, bd_addr_to_str(conn->address));

#ifdef ENABLE_CLASSIC
#if defined(ENABLE_SCO_OVER_HCI) || defined(HAVE_SCO_TRANSPORT)
    bd_addr_type_t addr_type = conn->address_type;
#endif
#ifdef HAVE_SCO_TRANSPORT
    hci_con_handle_t con_handle = conn->con_handle;
#endif
#endif

    hci_connection_stop_timer(conn);

    btstack_linked_list_remove(&hci_stack->connections, (btstack_linked_item_t *) conn);
    btstack_memory_hci_connection_free( conn );
    
    // now it's gone
    hci_emit_nr_connections_changed();

#ifdef ENABLE_CLASSIC
#ifdef ENABLE_SCO_OVER_HCI
    // update SCO
    if ((addr_type == BD_ADDR_TYPE_SCO) && (hci_stack->hci_transport != NULL) && (hci_stack->hci_transport->set_sco_config != NULL)){
        hci_stack->hci_transport->set_sco_config(hci_stack->sco_voice_setting_active, hci_number_sco_connections());
    }
#endif
#ifdef HAVE_SCO_TRANSPORT
    if ((addr_type == BD_ADDR_TYPE_SCO) && (hci_stack->sco_transport != NULL)){
        hci_stack->sco_transport->close(con_handle);
    }
#endif
#endif
}

#ifdef ENABLE_CLASSIC

static const uint16_t hci_acl_packet_type_sizes[] = {
    0, HCI_ACL_2DH1_SIZE, HCI_ACL_3DH1_SIZE, HCI_ACL_DM1_SIZE,
    HCI_ACL_DH1_SIZE, 0, 0, 0,
    HCI_ACL_2DH3_SIZE, HCI_ACL_3DH3_SIZE, HCI_ACL_DM3_SIZE, HCI_ACL_DH3_SIZE,
    HCI_ACL_2DH5_SIZE, HCI_ACL_3DH5_SIZE, HCI_ACL_DM5_SIZE, HCI_ACL_DH5_SIZE
};
static const uint8_t hci_acl_packet_type_feature_requirement_bit[] = {
     0, // 3 slot packets
     1, // 5 slot packets
    25, // EDR 2 mpbs
    26, // EDR 3 mbps
    39, // 3 slot EDR packts
    40, // 5 slot EDR packet
};
static const uint16_t hci_acl_packet_type_feature_packet_mask[] = {
    0x0f00, // 3 slot packets
    0xf000, // 5 slot packets
    0x1102, // EDR 2 mpbs
    0x2204, // EDR 3 mbps
    0x0300, // 3 slot EDR packts
    0x3000, // 5 slot EDR packet
};

static uint16_t hci_acl_packet_types_for_buffer_size_and_local_features(uint16_t buffer_size, uint8_t * local_supported_features){
    // enable packet types based on size
    uint16_t packet_types = 0;
    unsigned int i;
    for (i=0;i<16;i++){
        if (hci_acl_packet_type_sizes[i] == 0) continue;
        if (hci_acl_packet_type_sizes[i] <= buffer_size){
            packet_types |= 1 << i;
        }
    }
    // disable packet types due to missing local supported features
    for (i=0;i<sizeof(hci_acl_packet_type_feature_requirement_bit); i++){
        unsigned int bit_idx = hci_acl_packet_type_feature_requirement_bit[i];
        int feature_set = (local_supported_features[bit_idx >> 3] & (1<<(bit_idx & 7))) != 0;
        if (feature_set) continue;
        log_info("Features bit %02u is not set, removing packet types 0x%04x", bit_idx, hci_acl_packet_type_feature_packet_mask[i]);
        packet_types &= ~hci_acl_packet_type_feature_packet_mask[i];
    }
    return packet_types;
}

uint16_t hci_usable_acl_packet_types(void){
    // flip bits for "may not be used"
    return hci_stack->usable_packet_types_acl ^ 0x3306;
}

static const struct {
    uint8_t feature_index;
    uint16_t feature_packet_mask;
} hci_sco_packet_type_feature_requirements[] = {
        { 12, SCO_PACKET_TYPES_HV2 },                           // HV2 packets
        { 13, SCO_PACKET_TYPES_HV3 },                           // HV3 packets
        { 31, SCO_PACKET_TYPES_ESCO },                          // eSCO links (EV3 packets)
        { 32, SCO_PACKET_TYPES_EV4 },                           // EV4 packets
        { 45, SCO_PACKET_TYPES_2EV3 | SCO_PACKET_TYPES_2EV5 },  // EDR eSCO 2 Mb/s
        { 46, SCO_PACKET_TYPES_3EV3 | SCO_PACKET_TYPES_3EV5 },  // EDR eSCO 3 Mb/s
        { 47, SCO_PACKET_TYPES_2EV5 | SCO_PACKET_TYPES_3EV5 },  // 3-slot EDR eSCO packets, 2-EV3/3-EV3 use single slot
};

static uint16_t hci_sco_packet_types_for_features(const uint8_t * local_supported_features){
    uint16_t packet_types = SCO_PACKET_TYPES_ALL;
    unsigned int i;
    // disable packet types due to missing local supported features
    for (i=0;i<(sizeof(hci_sco_packet_type_feature_requirements)/sizeof(hci_sco_packet_type_feature_requirements[0])); i++){
        unsigned int bit_idx = hci_sco_packet_type_feature_requirements[i].feature_index;
        bool feature_set = (local_supported_features[bit_idx >> 3] & (1<<(bit_idx & 7))) != 0;
        if (feature_set) continue;
        log_info("Features bit %02u is not set, removing packet types 0x%04x", bit_idx, hci_sco_packet_type_feature_requirements[i].feature_packet_mask);
        packet_types &= ~hci_sco_packet_type_feature_requirements[i].feature_packet_mask;
    }
    return packet_types;
}

uint16_t hci_usable_sco_packet_types(void){
    return hci_stack->usable_packet_types_sco;
}

#endif

uint8_t* hci_get_outgoing_packet_buffer(void){
    // hci packet buffer is >= acl data packet length
    return hci_stack->hci_packet_buffer;
}

uint16_t hci_max_acl_data_packet_length(void){
    return hci_stack->acl_data_packet_length;
}

#ifdef ENABLE_CLASSIC
bool hci_extended_sco_link_supported(void){
    // No. 31, byte 3, bit 7
    return (hci_stack->local_supported_features[3] & (1 << 7)) != 0;
}
#endif

bool hci_non_flushable_packet_boundary_flag_supported(void){
    // No. 54, byte 6, bit 6
    return (hci_stack->local_supported_features[6u] & (1u << 6u)) != 0u;
}

#ifdef ENABLE_CLASSIC
static int gap_ssp_supported(void){
    // No. 51, byte 6, bit 3
    return (hci_stack->local_supported_features[6u] & (1u << 3u)) != 0u;
}
#endif

static int hci_classic_supported(void){
#ifdef ENABLE_CLASSIC    
    // No. 37, byte 4, bit 5, = No BR/EDR Support
    return (hci_stack->local_supported_features[4] & (1 << 5)) == 0;
#else
    return 0;
#endif
}

static int hci_le_supported(void){
#ifdef ENABLE_BLE
    // No. 37, byte 4, bit 6 = LE Supported (Controller)
    return (hci_stack->local_supported_features[4u] & (1u << 6u)) != 0u;
#else
    return 0;
#endif    
}

static bool hci_command_supported(uint8_t command_index){
    return (hci_stack->local_supported_commands & (1LU << command_index)) != 0;
}

#ifdef ENABLE_BLE

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
static bool hci_extended_advertising_supported(void){
    return hci_command_supported(SUPPORTED_HCI_COMMAND_LE_SET_EXTENDED_ADVERTISING_ENABLE);
}
#endif

static void hci_get_own_address_for_addr_type(uint8_t own_addr_type, bd_addr_t own_addr){
    if (own_addr_type == BD_ADDR_TYPE_LE_PUBLIC){
        (void)memcpy(own_addr, hci_stack->local_bd_addr, 6);
    } else {
        (void)memcpy(own_addr, hci_stack->le_random_address, 6);
    }
}

void gap_le_get_own_address(uint8_t * addr_type, bd_addr_t addr){
    *addr_type = hci_stack->le_own_addr_type;
    hci_get_own_address_for_addr_type(hci_stack->le_own_addr_type, addr);
}

#ifdef ENABLE_LE_PERIPHERAL
void gap_le_get_own_advertisements_address(uint8_t * addr_type, bd_addr_t addr){
    *addr_type = hci_stack->le_advertisements_own_addr_type;
    hci_get_own_address_for_addr_type(hci_stack->le_advertisements_own_addr_type, addr);
};
#endif

#ifdef ENABLE_LE_CENTRAL

/**
 * @brief Get own addr type and address used for LE connections (Central)
 */
void gap_le_get_own_connection_address(uint8_t * addr_type, bd_addr_t addr){
    *addr_type = hci_stack->le_connection_own_addr_type;
    hci_get_own_address_for_addr_type(hci_stack->le_connection_own_addr_type, addr);
}

void le_handle_advertisement_report(uint8_t *packet, uint16_t size){

    uint16_t offset = 3;
    uint8_t num_reports = packet[offset];
    offset += 1;

    uint16_t i;
    uint8_t event[12 + LE_ADVERTISING_DATA_SIZE]; // use upper bound to avoid var size automatic var
    for (i=0; (i<num_reports) && (offset < size);i++){
        // sanity checks on data_length:
        uint8_t data_length = packet[offset + 8];
        if (data_length > LE_ADVERTISING_DATA_SIZE) return;
        if ((offset + 9u + data_length + 1u) > size)    return;
        // setup event
        uint8_t event_size = 10u + data_length;
        uint16_t pos = 0;
        event[pos++] = GAP_EVENT_ADVERTISING_REPORT;
        event[pos++] = event_size;
        (void)memcpy(&event[pos], &packet[offset], 1 + 1 + 6); // event type + address type + address
        offset += 8;
        pos += 8;
        event[pos++] = packet[offset + 1 + data_length]; // rssi
        event[pos++] = data_length;
        offset++;
        (void)memcpy(&event[pos], &packet[offset], data_length);
        pos +=    data_length;
        offset += data_length + 1u; // rssi
        hci_emit_event(event, pos, 1);
    }
}

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
void le_handle_extended_advertisement_report(uint8_t *packet, uint16_t size) {
    uint16_t offset = 3;
    uint8_t num_reports = packet[offset++];
    uint8_t event[2 + 255]; // use upper bound to avoid var size automatic var
    uint8_t i;
    for (i=0; (i<num_reports) && (offset < size);i++){
        // sanity checks on data_length:
        uint16_t data_length = packet[offset + 23];
        if (data_length > LE_EXTENDED_ADVERTISING_DATA_SIZE) return;
        if ((offset + 24u + data_length) > size)    return;
        uint16_t event_type = little_endian_read_16(packet, offset);
        offset += 2;
        if ((event_type & 0x10) != 0) {
           // setup legacy event
            uint8_t legacy_event_type;
            switch (event_type){
                case 0b0010011:
                    // ADV_IND
                    legacy_event_type = 0;
                    break;
                case 0b0010101:
                    // ADV_DIRECT_IND
                    legacy_event_type = 1;
                    break;
                case 0b0010010:
                    // ADV_SCAN_IND
                    legacy_event_type = 2;
                    break;
                case 0b0010000:
                    // ADV_NONCONN_IND
                    legacy_event_type = 3;
                    break;
                case 0b0011011:
                case 0b0011010:
                    // SCAN_RSP
                    legacy_event_type = 4;
                    break;
                default:
                    legacy_event_type = 0;
                    break;
            }
            uint16_t pos = 0;
            event[pos++] = GAP_EVENT_ADVERTISING_REPORT;
            event[pos++] = 10u + data_length;
            event[pos++] = legacy_event_type;
            // copy address type + address
            (void) memcpy(&event[pos], &packet[offset], 1 + 6);
            offset += 7;
            pos += 7;
            // skip primary_phy, secondary_phy, advertising_sid, tx_power
            offset += 4;
            // copy rssi
            event[pos++] = packet[offset++];
            // skip periodic advertising interval and direct address
            offset += 9;
            // copy data len + data;
            (void) memcpy(&event[pos], &packet[offset], 1 + data_length);
            pos    += 1 +data_length;
            offset += 1+ data_length;
            hci_emit_event(event, pos, 1);
        } else {
            event[0] = GAP_EVENT_EXTENDED_ADVERTISING_REPORT;
            uint8_t report_len = 24 + data_length;
            event[1] = report_len;
            little_endian_store_16(event, 2, event_type);
            memcpy(&event[4], &packet[offset], report_len);
            offset += report_len;
            hci_emit_event(event, 2 + report_len, 1);
        }
    }
}
#endif

#endif
#endif

#ifdef ENABLE_BLE
#ifdef ENABLE_LE_PERIPHERAL
static void hci_update_advertisements_enabled_for_current_roles(void){
    if ((hci_stack->le_advertisements_state & LE_ADVERTISEMENT_STATE_ENABLED) != 0){
        // get number of active le slave connections
        int num_slave_connections = 0;
        btstack_linked_list_iterator_t it;
        btstack_linked_list_iterator_init(&it, &hci_stack->connections);
        while (btstack_linked_list_iterator_has_next(&it)){
            hci_connection_t * con = (hci_connection_t*) btstack_linked_list_iterator_next(&it);
            log_info("state %u, role %u, le_con %u", con->state, con->role, hci_is_le_connection(con));
            if (con->state != OPEN) continue;
            if (con->role  != HCI_ROLE_SLAVE) continue;
            if (!hci_is_le_connection(con)) continue;
            num_slave_connections++;
        }
        log_info("Num LE Peripheral roles: %u of %u", num_slave_connections, hci_stack->le_max_number_peripheral_connections);
        hci_stack->le_advertisements_enabled_for_current_roles = num_slave_connections < hci_stack->le_max_number_peripheral_connections;
    } else {
        hci_stack->le_advertisements_enabled_for_current_roles = false;
    }
}
#endif
#endif

#ifdef ENABLE_CLASSIC
static void gap_run_set_local_name(void){
    hci_reserve_packet_buffer();
    uint8_t * packet = hci_stack->hci_packet_buffer;
    // construct HCI Command and send
    uint16_t opcode = hci_write_local_name.opcode;
    hci_stack->last_cmd_opcode = opcode;
    packet[0] = opcode & 0xff;
    packet[1] = opcode >> 8;
    packet[2] = DEVICE_NAME_LEN;
    memset(&packet[3], 0, DEVICE_NAME_LEN);
    uint16_t name_len = (uint16_t) strlen(hci_stack->local_name);
    uint16_t bytes_to_copy = btstack_min(name_len, DEVICE_NAME_LEN);
    // if shorter than DEVICE_NAME_LEN, it's implicitly NULL-terminated by memset call
    (void)memcpy(&packet[3], hci_stack->local_name, bytes_to_copy);
    // expand '00:00:00:00:00:00' in name with bd_addr
    btstack_replace_bd_addr_placeholder(&packet[3], bytes_to_copy, hci_stack->local_bd_addr);
    hci_send_cmd_packet(packet, HCI_CMD_HEADER_SIZE + DEVICE_NAME_LEN);
}

static void gap_run_set_eir_data(void){
    hci_reserve_packet_buffer();
    uint8_t * packet = hci_stack->hci_packet_buffer;
    // construct HCI Command in-place and send
    uint16_t opcode = hci_write_extended_inquiry_response.opcode;
    hci_stack->last_cmd_opcode = opcode;
    uint16_t offset = 0;
    packet[offset++] = opcode & 0xff;
    packet[offset++] = opcode >> 8;
    packet[offset++] = 1 + EXTENDED_INQUIRY_RESPONSE_DATA_LEN;
    packet[offset++] = 0;  // FEC not required
    memset(&packet[offset], 0, EXTENDED_INQUIRY_RESPONSE_DATA_LEN);
    if (hci_stack->eir_data){
        // copy items and expand '00:00:00:00:00:00' in name with bd_addr
        ad_context_t context;
        for (ad_iterator_init(&context, EXTENDED_INQUIRY_RESPONSE_DATA_LEN, hci_stack->eir_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)) {
            uint8_t data_type   = ad_iterator_get_data_type(&context);
            uint8_t size        = ad_iterator_get_data_len(&context);
            const uint8_t *data = ad_iterator_get_data(&context);
            // copy item
            packet[offset++] = size + 1;
            packet[offset++] = data_type;
            memcpy(&packet[offset], data, size);
            // update name item
            if ((data_type == BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME) || (data_type == BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME)){
                btstack_replace_bd_addr_placeholder(&packet[offset], size, hci_stack->local_bd_addr);
            }
            offset += size;
        }
    } else {
        uint16_t name_len = (uint16_t) strlen(hci_stack->local_name);
        uint16_t bytes_to_copy = btstack_min(name_len, EXTENDED_INQUIRY_RESPONSE_DATA_LEN - 2);
        packet[offset++] = bytes_to_copy + 1;
        packet[offset++] = BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME;
        (void)memcpy(&packet[6], hci_stack->local_name, bytes_to_copy);
        // expand '00:00:00:00:00:00' in name with bd_addr
        btstack_replace_bd_addr_placeholder(&packet[offset], bytes_to_copy, hci_stack->local_bd_addr);
    }
    hci_send_cmd_packet(packet, HCI_CMD_HEADER_SIZE + 1 + EXTENDED_INQUIRY_RESPONSE_DATA_LEN);
}

static void hci_run_gap_tasks_classic(void){
    if ((hci_stack->gap_tasks_classic & GAP_TASK_SET_CLASS_OF_DEVICE) != 0) {
        hci_stack->gap_tasks_classic &= ~GAP_TASK_SET_CLASS_OF_DEVICE;
        hci_send_cmd(&hci_write_class_of_device, hci_stack->class_of_device);
        return;
    }
    if ((hci_stack->gap_tasks_classic & GAP_TASK_SET_LOCAL_NAME) != 0) {
        hci_stack->gap_tasks_classic &= ~GAP_TASK_SET_LOCAL_NAME;
        gap_run_set_local_name();
        return;
    }
    if ((hci_stack->gap_tasks_classic & GAP_TASK_SET_EIR_DATA) != 0) {
        hci_stack->gap_tasks_classic &= ~GAP_TASK_SET_EIR_DATA;
        gap_run_set_eir_data();
        return;
    }
    if ((hci_stack->gap_tasks_classic & GAP_TASK_SET_DEFAULT_LINK_POLICY) != 0) {
        hci_stack->gap_tasks_classic &= ~GAP_TASK_SET_DEFAULT_LINK_POLICY;
        hci_send_cmd(&hci_write_default_link_policy_setting, hci_stack->default_link_policy_settings);
        return;
    }
    // write page scan activity
    if ((hci_stack->gap_tasks_classic & GAP_TASK_WRITE_PAGE_SCAN_ACTIVITY) != 0) {
        hci_stack->gap_tasks_classic &= ~GAP_TASK_WRITE_PAGE_SCAN_ACTIVITY;
        hci_send_cmd(&hci_write_page_scan_activity, hci_stack->new_page_scan_interval, hci_stack->new_page_scan_window);
        return;
    }
    // write page scan type
    if ((hci_stack->gap_tasks_classic & GAP_TASK_WRITE_PAGE_SCAN_TYPE) != 0) {
        hci_stack->gap_tasks_classic &= ~GAP_TASK_WRITE_PAGE_SCAN_TYPE;
        hci_send_cmd(&hci_write_page_scan_type, hci_stack->new_page_scan_type);
        return;
    }
    // write page timeout
    if ((hci_stack->gap_tasks_classic & GAP_TASK_WRITE_PAGE_TIMEOUT) != 0) {
        hci_stack->gap_tasks_classic &= ~GAP_TASK_WRITE_PAGE_TIMEOUT;
        hci_send_cmd(&hci_write_page_timeout, hci_stack->page_timeout);
        return;
    }
    // send scan enable
    if ((hci_stack->gap_tasks_classic & GAP_TASK_WRITE_SCAN_ENABLE) != 0) {
        hci_stack->gap_tasks_classic &= ~GAP_TASK_WRITE_SCAN_ENABLE;
        hci_send_cmd(&hci_write_scan_enable, hci_stack->new_scan_enable_value);
        return;
    }
    // send write scan activity
    if ((hci_stack->gap_tasks_classic & GAP_TASK_WRITE_INQUIRY_SCAN_ACTIVITY) != 0) {
        hci_stack->gap_tasks_classic &= ~GAP_TASK_WRITE_INQUIRY_SCAN_ACTIVITY;
        hci_send_cmd(&hci_write_inquiry_scan_activity, hci_stack->inquiry_scan_interval, hci_stack->inquiry_scan_window);
        return;
    }
}
#endif

#ifndef HAVE_HOST_CONTROLLER_API

static uint32_t hci_transport_uart_get_main_baud_rate(void){
    if (!hci_stack->config) return 0;
    uint32_t baud_rate = ((hci_transport_config_uart_t *)hci_stack->config)->baudrate_main;
    // Limit baud rate for Broadcom chipsets to 3 mbps
    if ((hci_stack->manufacturer == BLUETOOTH_COMPANY_ID_BROADCOM_CORPORATION) && (baud_rate > 3000000)){
        baud_rate = 3000000;
    }
    return baud_rate;
}

static void hci_initialization_timeout_handler(btstack_timer_source_t * ds){
    UNUSED(ds);

    switch (hci_stack->substate){
        case HCI_INIT_W4_SEND_RESET:
            log_info("Resend HCI Reset");
            hci_stack->substate = HCI_INIT_SEND_RESET;
            hci_stack->num_cmd_packets = 1;
            hci_run();
            break;
        case HCI_INIT_W4_CUSTOM_INIT_CSR_WARM_BOOT_LINK_RESET:
            log_info("Resend HCI Reset - CSR Warm Boot with Link Reset");
            if (hci_stack->hci_transport->reset_link){
                hci_stack->hci_transport->reset_link();
            }

            /* fall through */

        case HCI_INIT_W4_CUSTOM_INIT_CSR_WARM_BOOT:
            log_info("Resend HCI Reset - CSR Warm Boot");
            hci_stack->substate = HCI_INIT_SEND_RESET_CSR_WARM_BOOT;
            hci_stack->num_cmd_packets = 1;
            hci_run();
            break;
        case HCI_INIT_W4_SEND_BAUD_CHANGE:
            if (hci_stack->hci_transport->set_baudrate){
                uint32_t baud_rate = hci_transport_uart_get_main_baud_rate();
                log_info("Local baud rate change to %" PRIu32 "(timeout handler)", baud_rate);
                hci_stack->hci_transport->set_baudrate(baud_rate);
            }
            // For CSR, HCI Reset is sent on new baud rate. Don't forget to reset link for H5/BCSP
            if (hci_stack->manufacturer == BLUETOOTH_COMPANY_ID_CAMBRIDGE_SILICON_RADIO){
                if (hci_stack->hci_transport->reset_link){
                    log_info("Link Reset");
                    hci_stack->hci_transport->reset_link();
                }
                hci_stack->substate = HCI_INIT_SEND_RESET_CSR_WARM_BOOT;
                hci_run();
            }
            break;
        case HCI_INIT_W4_CUSTOM_INIT_BCM_DELAY:
            // otherwise continue
            hci_stack->substate = HCI_INIT_W4_READ_LOCAL_SUPPORTED_COMMANDS;
            hci_send_cmd(&hci_read_local_supported_commands);
            break;
        default:
            break;
    }
}
#endif

static void hci_initializing_next_state(void){
    hci_stack->substate = (hci_substate_t )( ((int) hci_stack->substate) + 1);
}

static void hci_init_done(void){
    // done. tell the app
    log_info("hci_init_done -> HCI_STATE_WORKING");
    hci_stack->state = HCI_STATE_WORKING;
    hci_emit_state();
}

// assumption: hci_can_send_command_packet_now() == true
static void hci_initializing_run(void){
    log_debug("hci_initializing_run: substate %u, can send %u", hci_stack->substate, hci_can_send_command_packet_now());

    if (!hci_can_send_command_packet_now()) return;

#ifndef HAVE_HOST_CONTROLLER_API
    bool need_baud_change = hci_stack->config
            && hci_stack->chipset
            && hci_stack->chipset->set_baudrate_command
            && hci_stack->hci_transport->set_baudrate
            && ((hci_transport_config_uart_t *)hci_stack->config)->baudrate_main;
#endif

    switch (hci_stack->substate){
        case HCI_INIT_SEND_RESET:
            hci_state_reset();

#ifndef HAVE_HOST_CONTROLLER_API
            // prepare reset if command complete not received in 100ms
            btstack_run_loop_set_timer(&hci_stack->timeout, HCI_RESET_RESEND_TIMEOUT_MS);
            btstack_run_loop_set_timer_handler(&hci_stack->timeout, hci_initialization_timeout_handler);
            btstack_run_loop_add_timer(&hci_stack->timeout);
#endif
            // send command
            hci_stack->substate = HCI_INIT_W4_SEND_RESET;
            hci_send_cmd(&hci_reset);
            break;
        case HCI_INIT_SEND_READ_LOCAL_VERSION_INFORMATION:
            hci_send_cmd(&hci_read_local_version_information);
            hci_stack->substate = HCI_INIT_W4_SEND_READ_LOCAL_VERSION_INFORMATION;
            break;

#ifndef HAVE_HOST_CONTROLLER_API
        case HCI_INIT_SEND_RESET_CSR_WARM_BOOT:
            hci_state_reset();
            // prepare reset if command complete not received in 100ms
            btstack_run_loop_set_timer(&hci_stack->timeout, HCI_RESET_RESEND_TIMEOUT_MS);
            btstack_run_loop_set_timer_handler(&hci_stack->timeout, hci_initialization_timeout_handler);
            btstack_run_loop_add_timer(&hci_stack->timeout);
            // send command
            hci_stack->substate = HCI_INIT_W4_CUSTOM_INIT_CSR_WARM_BOOT;
            hci_send_cmd(&hci_reset);
            break;
        case HCI_INIT_SEND_RESET_ST_WARM_BOOT:
            hci_state_reset();
            hci_stack->substate = HCI_INIT_W4_SEND_RESET_ST_WARM_BOOT;
            hci_send_cmd(&hci_reset);
            break;
        case HCI_INIT_SEND_BAUD_CHANGE_BCM: {
            uint32_t baud_rate = hci_transport_uart_get_main_baud_rate();
            hci_stack->chipset->set_baudrate_command(baud_rate, hci_stack->hci_packet_buffer);
            hci_stack->last_cmd_opcode = little_endian_read_16(hci_stack->hci_packet_buffer, 0);
            hci_stack->substate = HCI_INIT_W4_SEND_BAUD_CHANGE_BCM;
            hci_send_cmd_packet(hci_stack->hci_packet_buffer, 3u + hci_stack->hci_packet_buffer[2u]);
            break;
        }
        case HCI_INIT_SET_BD_ADDR:
            log_info("Set Public BD ADDR to %s", bd_addr_to_str(hci_stack->custom_bd_addr));
            hci_stack->chipset->set_bd_addr_command(hci_stack->custom_bd_addr, hci_stack->hci_packet_buffer);
            hci_stack->last_cmd_opcode = little_endian_read_16(hci_stack->hci_packet_buffer, 0);
            hci_stack->substate = HCI_INIT_W4_SET_BD_ADDR;
            hci_send_cmd_packet(hci_stack->hci_packet_buffer, 3u + hci_stack->hci_packet_buffer[2u]);
            break;
        case HCI_INIT_SEND_READ_LOCAL_NAME:
#ifdef ENABLE_CLASSIC
            hci_send_cmd(&hci_read_local_name);
            hci_stack->substate = HCI_INIT_W4_SEND_READ_LOCAL_NAME;
            break;
#endif
            /* fall through */

        case HCI_INIT_SEND_BAUD_CHANGE:
            if (need_baud_change) {
                uint32_t baud_rate = hci_transport_uart_get_main_baud_rate();
                hci_stack->chipset->set_baudrate_command(baud_rate, hci_stack->hci_packet_buffer);
                hci_stack->last_cmd_opcode = little_endian_read_16(hci_stack->hci_packet_buffer, 0);
                hci_stack->substate = HCI_INIT_W4_SEND_BAUD_CHANGE;
                hci_send_cmd_packet(hci_stack->hci_packet_buffer, 3u + hci_stack->hci_packet_buffer[2u]);
                // STLC25000D: baudrate change happens within 0.5 s after command was send,
                // use timer to update baud rate after 100 ms (knowing exactly, when command was sent is non-trivial)
                if (hci_stack->manufacturer == BLUETOOTH_COMPANY_ID_ST_MICROELECTRONICS){
                    btstack_run_loop_set_timer(&hci_stack->timeout, HCI_RESET_RESEND_TIMEOUT_MS);
                    btstack_run_loop_add_timer(&hci_stack->timeout);
               }
               break;
            }
            hci_stack->substate = HCI_INIT_CUSTOM_INIT;

            /* fall through */

        case HCI_INIT_CUSTOM_INIT:
        case HCI_INIT_CUSTOM_PRE_INIT:
            // Custom initialization
            if (hci_stack->chipset && hci_stack->chipset->next_command){
                hci_stack->chipset_result = (*hci_stack->chipset->next_command)(hci_stack->hci_packet_buffer);
                bool send_cmd = false;
                switch (hci_stack->chipset_result){
                    case BTSTACK_CHIPSET_VALID_COMMAND:
                        send_cmd = true;
                        switch (hci_stack->substate){
                            case HCI_INIT_CUSTOM_INIT:
                                hci_stack->substate = HCI_INIT_W4_CUSTOM_INIT;
                                break;
                            case HCI_INIT_CUSTOM_PRE_INIT:
                                hci_stack->substate = HCI_INIT_W4_CUSTOM_PRE_INIT;
                                break;
                            default:
                                btstack_assert(false);
                                break;
                        }
                        break;
                    case BTSTACK_CHIPSET_WARMSTART_REQUIRED:
                        send_cmd = true;
                        // CSR Warm Boot: Wait a bit, then send HCI Reset until HCI Command Complete
                        log_info("CSR Warm Boot");
                        btstack_run_loop_set_timer(&hci_stack->timeout, HCI_RESET_RESEND_TIMEOUT_MS);
                        btstack_run_loop_set_timer_handler(&hci_stack->timeout, hci_initialization_timeout_handler);
                        btstack_run_loop_add_timer(&hci_stack->timeout);
                        if ((hci_stack->manufacturer == BLUETOOTH_COMPANY_ID_CAMBRIDGE_SILICON_RADIO)
                            && hci_stack->config
                            && hci_stack->chipset
                            // && hci_stack->chipset->set_baudrate_command -- there's no such command
                            && hci_stack->hci_transport->set_baudrate
                            && hci_transport_uart_get_main_baud_rate()){
                            hci_stack->substate = HCI_INIT_W4_SEND_BAUD_CHANGE;
                        } else {
                           hci_stack->substate = HCI_INIT_W4_CUSTOM_INIT_CSR_WARM_BOOT_LINK_RESET;
                        }
                        break;
                    default:
                        break;
                }

                if (send_cmd){
                    int size = 3u + hci_stack->hci_packet_buffer[2u];
                    hci_stack->last_cmd_opcode = little_endian_read_16(hci_stack->hci_packet_buffer, 0);
                    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, hci_stack->hci_packet_buffer, size);
                    hci_stack->hci_transport->send_packet(HCI_COMMAND_DATA_PACKET, hci_stack->hci_packet_buffer, size);
                    break;
                }
                log_info("Init script done");

                // Custom Pre-Init complete, start regular init with HCI Reset
                if (hci_stack->substate == HCI_INIT_CUSTOM_PRE_INIT){
                    hci_stack->substate = HCI_INIT_W4_SEND_RESET;
                    hci_send_cmd(&hci_reset);
                    break;
                }

                // Init script download on Broadcom chipsets causes:
                if ( (hci_stack->chipset_result != BTSTACK_CHIPSET_NO_INIT_SCRIPT) &&
                   (  (hci_stack->manufacturer == BLUETOOTH_COMPANY_ID_BROADCOM_CORPORATION) 
                ||    (hci_stack->manufacturer == BLUETOOTH_COMPANY_ID_EM_MICROELECTRONIC_MARIN_SA)) ){

                    // - baud rate to reset, restore UART baud rate if needed
                    if (need_baud_change) {
                        uint32_t baud_rate = ((hci_transport_config_uart_t *)hci_stack->config)->baudrate_init;
                        log_info("Local baud rate change to %" PRIu32 " after init script (bcm)", baud_rate);
                        hci_stack->hci_transport->set_baudrate(baud_rate);
                    }

                    uint16_t bcm_delay_ms = 300;
                    // - UART may or may not be disabled during update and Controller RTS may or may not be high during this time
                    //   -> Work around: wait here.
                    log_info("BCM delay (%u ms) after init script", bcm_delay_ms);
                    hci_stack->substate = HCI_INIT_W4_CUSTOM_INIT_BCM_DELAY;
                    btstack_run_loop_set_timer(&hci_stack->timeout, bcm_delay_ms);
                    btstack_run_loop_set_timer_handler(&hci_stack->timeout, hci_initialization_timeout_handler);
                    btstack_run_loop_add_timer(&hci_stack->timeout);
                    break;
                }
            }
#endif
            /* fall through */

        case HCI_INIT_READ_LOCAL_SUPPORTED_COMMANDS:
            hci_stack->substate = HCI_INIT_W4_READ_LOCAL_SUPPORTED_COMMANDS;
            hci_send_cmd(&hci_read_local_supported_commands);
            break;       
        case HCI_INIT_READ_BD_ADDR:
            hci_stack->substate = HCI_INIT_W4_READ_BD_ADDR;
            hci_send_cmd(&hci_read_bd_addr);
            break;
        case HCI_INIT_READ_BUFFER_SIZE:
            // only read buffer size if supported
            if (hci_command_supported(SUPPORTED_HCI_COMMAND_READ_BUFFER_SIZE)){
                hci_stack->substate = HCI_INIT_W4_READ_BUFFER_SIZE;
                hci_send_cmd(&hci_read_buffer_size);
                break;
            }

            /* fall through */

        case HCI_INIT_READ_LOCAL_SUPPORTED_FEATURES:
            hci_stack->substate = HCI_INIT_W4_READ_LOCAL_SUPPORTED_FEATURES;
            hci_send_cmd(&hci_read_local_supported_features);
            break;                

#ifdef ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
        case HCI_INIT_SET_CONTROLLER_TO_HOST_FLOW_CONTROL:
            hci_stack->substate = HCI_INIT_W4_SET_CONTROLLER_TO_HOST_FLOW_CONTROL;
            hci_send_cmd(&hci_set_controller_to_host_flow_control, 3);  // ACL + SCO Flow Control
            break;
        case HCI_INIT_HOST_BUFFER_SIZE:
            hci_stack->substate = HCI_INIT_W4_HOST_BUFFER_SIZE;
            hci_send_cmd(&hci_host_buffer_size, HCI_HOST_ACL_PACKET_LEN, HCI_HOST_SCO_PACKET_LEN, 
                                                HCI_HOST_ACL_PACKET_NUM, HCI_HOST_SCO_PACKET_NUM);
            break;            
#endif

        case HCI_INIT_SET_EVENT_MASK:
            hci_stack->substate = HCI_INIT_W4_SET_EVENT_MASK;
            if (hci_le_supported()){
                hci_send_cmd(&hci_set_event_mask,0xFFFFFFFFU, 0x3FFFFFFFU);
            } else {
                // Kensington Bluetooth 2.1 USB Dongle (CSR Chipset) returns an error for 0xffff... 
                hci_send_cmd(&hci_set_event_mask,0xFFFFFFFFU, 0x1FFFFFFFU);
            }
            break;

        case HCI_INIT_SET_EVENT_MASK_2:
            // On Bluettooth PTS dongle (BL 654) with PacketCraft HCI Firmware (LMP subversion) 0x5244,
            // setting Event Mask 2 causes Controller to drop Encryption Change events.
            if (hci_command_supported(SUPPORTED_HCI_COMMAND_SET_EVENT_MASK_PAGE_2)
            && (hci_stack->manufacturer != BLUETOOTH_COMPANY_ID_PACKETCRAFT_INC)){
                hci_stack->substate = HCI_INIT_W4_SET_EVENT_MASK_2;
                // Encryption Change Event v2 - bit 25
                hci_send_cmd(&hci_set_event_mask_2,0x02000000U, 0x0);
                break;
            }

#ifdef ENABLE_CLASSIC
            /* fall through */

        case HCI_INIT_WRITE_SIMPLE_PAIRING_MODE:
            if (hci_classic_supported() && gap_ssp_supported()){
                hci_stack->substate = HCI_INIT_W4_WRITE_SIMPLE_PAIRING_MODE;
                hci_send_cmd(&hci_write_simple_pairing_mode, hci_stack->ssp_enable);
                break;
            }

            /* fall through */

        case HCI_INIT_WRITE_INQUIRY_MODE:
            if (hci_classic_supported()){
                hci_stack->substate = HCI_INIT_W4_WRITE_INQUIRY_MODE;
                hci_send_cmd(&hci_write_inquiry_mode, (int) hci_stack->inquiry_mode);
                break;
            }

            /* fall through */

        case HCI_INIT_WRITE_SECURE_CONNECTIONS_HOST_ENABLE:
            // skip write secure connections host support if not supported or disabled
            if (hci_classic_supported() && hci_stack->secure_connections_enable
            && hci_command_supported(SUPPORTED_HCI_COMMAND_WRITE_SECURE_CONNECTIONS_HOST)) {
                hci_stack->secure_connections_active = true;
                hci_stack->substate = HCI_INIT_W4_WRITE_SECURE_CONNECTIONS_HOST_ENABLE;
                hci_send_cmd(&hci_write_secure_connections_host_support, 1);
                break;
            }

            /* fall through */

        case HCI_INIT_SET_MIN_ENCRYPTION_KEY_SIZE:
            // skip set min encryption key size
            if (hci_classic_supported() && hci_command_supported(SUPPORTED_HCI_COMMAND_SET_MIN_ENCRYPTION_KEY_SIZE)) {
                hci_stack->substate = HCI_INIT_W4_SET_MIN_ENCRYPTION_KEY_SIZE;
                hci_send_cmd(&hci_set_min_encryption_key_size, hci_stack->gap_required_encyrption_key_size);
                break;
            }

#ifdef ENABLE_SCO_OVER_HCI
            /* fall through */

        // only sent if ENABLE_SCO_OVER_HCI is defined
        case HCI_INIT_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE:
            // skip write synchronous flow control if not supported
            if (hci_classic_supported()
            && hci_command_supported(SUPPORTED_HCI_COMMAND_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE)) {
                hci_stack->substate = HCI_INIT_W4_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE;
                hci_send_cmd(&hci_write_synchronous_flow_control_enable, 1); // SCO tracking enabled
                break;
            }
            /* fall through */

        case HCI_INIT_WRITE_DEFAULT_ERRONEOUS_DATA_REPORTING:
            // skip write default erroneous data reporting if not supported
            if (hci_classic_supported()
            && hci_command_supported(SUPPORTED_HCI_COMMAND_WRITE_DEFAULT_ERRONEOUS_DATA_REPORTING)) {
                hci_stack->substate = HCI_INIT_W4_WRITE_DEFAULT_ERRONEOUS_DATA_REPORTING;
                hci_send_cmd(&hci_write_default_erroneous_data_reporting, 1);
                break;
            }
#endif

#if defined(ENABLE_SCO_OVER_HCI) || defined(ENABLE_SCO_OVER_PCM)
            /* fall through */

        // only sent if manufacturer is Broadcom and ENABLE_SCO_OVER_HCI or ENABLE_SCO_OVER_PCM is defined
        case HCI_INIT_BCM_WRITE_SCO_PCM_INT:
            if (hci_classic_supported() && (hci_stack->manufacturer == BLUETOOTH_COMPANY_ID_BROADCOM_CORPORATION)){
                hci_stack->substate = HCI_INIT_W4_BCM_WRITE_SCO_PCM_INT;
#ifdef ENABLE_SCO_OVER_HCI
                log_info("BCM: Route SCO data via HCI transport");
                hci_send_cmd(&hci_bcm_write_sco_pcm_int, 1, 0, 0, 0, 0);
#endif
#ifdef ENABLE_SCO_OVER_PCM
                log_info("BCM: Route SCO data via PCM interface");
#ifdef ENABLE_BCM_PCM_WBS
                // 512 kHz bit clock for 2 channels x 16 bit x 16 kHz
                hci_send_cmd(&hci_bcm_write_sco_pcm_int, 0, 2, 0, 1, 1);
#else
                // 256 kHz bit clock for 2 channels x 16 bit x 8 kHz
                hci_send_cmd(&hci_bcm_write_sco_pcm_int, 0, 1, 0, 1, 1);
#endif
#endif
                break;
            }
#endif

#ifdef ENABLE_SCO_OVER_PCM
            /* fall through */

        case HCI_INIT_BCM_WRITE_I2SPCM_INTERFACE_PARAM:
            if (hci_classic_supported() && (hci_stack->manufacturer == BLUETOOTH_COMPANY_ID_BROADCOM_CORPORATION)){
                hci_stack->substate = HCI_INIT_W4_BCM_WRITE_I2SPCM_INTERFACE_PARAM;
                log_info("BCM: Config PCM interface for I2S");
#ifdef ENABLE_BCM_PCM_WBS
                // 512 kHz bit clock for 2 channels x 16 bit x 8 kHz
                hci_send_cmd(&hci_bcm_write_i2spcm_interface_param, 1, 1, 0, 2);
#else
                // 256 kHz bit clock for 2 channels x 16 bit x 8 kHz
                hci_send_cmd(&hci_bcm_write_i2spcm_interface_param, 1, 1, 0, 1);
#endif
                break;
            }
#endif
#endif

#ifdef ENABLE_BLE
            /* fall through */

        // LE INIT
        case HCI_INIT_LE_READ_BUFFER_SIZE:
            if (hci_le_supported()){
                hci_stack->substate = HCI_INIT_W4_LE_READ_BUFFER_SIZE;
                if (hci_command_supported(SUPPORTED_HCI_COMMAND_LE_READ_BUFFER_SIZE_V2)){
                    hci_send_cmd(&hci_le_read_buffer_size_v2);
                } else {
                    hci_send_cmd(&hci_le_read_buffer_size);
                }
                break;
            }

            /* fall through */

        case HCI_INIT_WRITE_LE_HOST_SUPPORTED:
            // skip write le host if not supported (e.g. on LE only EM9301)
            if (hci_le_supported()
            && hci_command_supported(SUPPORTED_HCI_COMMAND_WRITE_LE_HOST_SUPPORTED)) {
                // LE Supported Host = 1, Simultaneous Host = 0
                hci_stack->substate = HCI_INIT_W4_WRITE_LE_HOST_SUPPORTED;
                hci_send_cmd(&hci_write_le_host_supported, 1, 0);
                break;
            }

            /* fall through */

        case HCI_INIT_LE_SET_EVENT_MASK:
            if (hci_le_supported()){
                hci_stack->substate = HCI_INIT_W4_LE_SET_EVENT_MASK;
                hci_send_cmd(&hci_le_set_event_mask, 0xfffffdff, 0x07); // all events from core v5.3 without LE Enhanced Connection Complete
                break;
            }
#endif

#ifdef ENABLE_LE_DATA_LENGTH_EXTENSION
            /* fall through */

        case HCI_INIT_LE_READ_MAX_DATA_LENGTH:
            if (hci_le_supported()
            && hci_command_supported(SUPPORTED_HCI_COMMAND_LE_READ_MAXIMUM_DATA_LENGTH)) {
                hci_stack->substate = HCI_INIT_W4_LE_READ_MAX_DATA_LENGTH;
                hci_send_cmd(&hci_le_read_maximum_data_length);
                break;
            }

            /* fall through */

        case HCI_INIT_LE_WRITE_SUGGESTED_DATA_LENGTH:
            if (hci_le_supported()
            && hci_command_supported(SUPPORTED_HCI_COMMAND_LE_WRITE_SUGGESTED_DEFAULT_DATA_LENGTH)) {
                hci_stack->substate = HCI_INIT_W4_LE_WRITE_SUGGESTED_DATA_LENGTH;
                hci_send_cmd(&hci_le_write_suggested_default_data_length, hci_stack->le_supported_max_tx_octets, hci_stack->le_supported_max_tx_time);
                break;
            }
#endif

#ifdef ENABLE_LE_CENTRAL
            /* fall through */

        case HCI_INIT_READ_WHITE_LIST_SIZE:
            if (hci_le_supported()){
                hci_stack->substate = HCI_INIT_W4_READ_WHITE_LIST_SIZE;
                hci_send_cmd(&hci_le_read_white_list_size);
                break;
            }
            
#endif

#ifdef ENABLE_LE_PERIPHERAL
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
            /* fall through */

        case HCI_INIT_LE_READ_MAX_ADV_DATA_LEN:
            if (hci_extended_advertising_supported()){
                hci_stack->substate = HCI_INIT_W4_LE_READ_MAX_ADV_DATA_LEN;
                hci_send_cmd(&hci_le_read_maximum_advertising_data_length);
                break;
            }
#endif
#endif
            /* fall through */

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
    case HCI_INIT_LE_SET_HOST_FEATURE_CONNECTED_ISO_STREAMS:
            if (hci_le_supported()) {
                hci_stack->substate = HCI_INIT_W4_LE_SET_HOST_FEATURE_CONNECTED_ISO_STREAMS;
                hci_send_cmd(&hci_le_set_host_feature, 32, 1);
                break;
            }
#endif

            /* fall through */

        case HCI_INIT_DONE:
            hci_stack->substate = HCI_INIT_DONE;
            // main init sequence complete
#ifdef ENABLE_CLASSIC
            // check if initial Classic GAP Tasks are completed
            if (hci_classic_supported() && (hci_stack->gap_tasks_classic != 0)) {
                hci_run_gap_tasks_classic();
                break;
            }
#endif
#ifdef ENABLE_BLE
#ifdef ENABLE_LE_CENTRAL
            // check if initial LE GAP Tasks are completed
            if (hci_le_supported() && hci_stack->le_scanning_param_update) {
                hci_run_general_gap_le();
                break;
            }
#endif
#endif
            hci_init_done();
            break;

        default:
            return;
    }
}

static bool hci_initializing_event_handler_command_completed(const uint8_t * packet){
    bool command_completed = false;
    if (hci_event_packet_get_type(packet) == HCI_EVENT_COMMAND_COMPLETE){
        uint16_t opcode = little_endian_read_16(packet,3);
        if (opcode == hci_stack->last_cmd_opcode){
            command_completed = true;
            log_debug("Command complete for expected opcode %04x at substate %u", opcode, hci_stack->substate);
        } else {
            log_info("Command complete for different opcode %04x, expected %04x, at substate %u", opcode, hci_stack->last_cmd_opcode, hci_stack->substate);
        }
    }

    if (hci_event_packet_get_type(packet) == HCI_EVENT_COMMAND_STATUS){
        uint8_t  status = packet[2];
        uint16_t opcode = little_endian_read_16(packet,4);
        if (opcode == hci_stack->last_cmd_opcode){
            if (status){
                command_completed = true;
                log_debug("Command status error 0x%02x for expected opcode %04x at substate %u", status, opcode, hci_stack->substate);
            } else {
                log_info("Command status OK for expected opcode %04x, waiting for command complete", opcode);
            }
        } else {
            log_debug("Command status for opcode %04x, expected %04x", opcode, hci_stack->last_cmd_opcode);
        }
    }
#ifndef HAVE_HOST_CONTROLLER_API
    // Vendor == CSR
    if ((hci_stack->substate == HCI_INIT_W4_CUSTOM_INIT) && (hci_event_packet_get_type(packet) == HCI_EVENT_VENDOR_SPECIFIC)){
        // TODO: track actual command
        command_completed = true;
    }

    // Vendor == Toshiba
    if ((hci_stack->substate == HCI_INIT_W4_SEND_BAUD_CHANGE) && (hci_event_packet_get_type(packet) == HCI_EVENT_VENDOR_SPECIFIC)){
        // TODO: track actual command
        command_completed = true;
        // Fix: no HCI Command Complete received, so num_cmd_packets not reset
        hci_stack->num_cmd_packets = 1;
    }
#endif

    return command_completed;
}

static void hci_initializing_event_handler(const uint8_t * packet, uint16_t size){

    UNUSED(size);   // ok: less than 6 bytes are read from our buffer
    
    bool command_completed =  hci_initializing_event_handler_command_completed(packet);

#ifndef HAVE_HOST_CONTROLLER_API

    // Late response (> 100 ms) for HCI Reset e.g. on Toshiba TC35661:
    // Command complete for HCI Reset arrives after we've resent the HCI Reset command
    //
    // HCI Reset
    // Timeout 100 ms
    // HCI Reset
    // Command Complete Reset
    // HCI Read Local Version Information
    // Command Complete Reset - but we expected Command Complete Read Local Version Information
    // hang...
    //
    // Fix: Command Complete for HCI Reset in HCI_INIT_W4_SEND_READ_LOCAL_VERSION_INFORMATION trigger resend
    if (!command_completed
            && (hci_event_packet_get_type(packet) == HCI_EVENT_COMMAND_COMPLETE)
            && (hci_stack->substate == HCI_INIT_W4_SEND_READ_LOCAL_VERSION_INFORMATION)){

        uint16_t opcode = little_endian_read_16(packet,3);
        if (opcode == hci_reset.opcode){
            hci_stack->substate = HCI_INIT_SEND_READ_LOCAL_VERSION_INFORMATION;
            return;
        }
    }

    // CSR & H5
    // Fix: Command Complete for HCI Reset in HCI_INIT_W4_SEND_READ_LOCAL_VERSION_INFORMATION trigger resend
    if (!command_completed
            && (hci_event_packet_get_type(packet) == HCI_EVENT_COMMAND_COMPLETE)
            && (hci_stack->substate == HCI_INIT_W4_READ_LOCAL_SUPPORTED_COMMANDS)){

        uint16_t opcode = little_endian_read_16(packet,3);
        if (opcode == hci_reset.opcode){
            hci_stack->substate = HCI_INIT_READ_LOCAL_SUPPORTED_COMMANDS;
            return;
        }
    }

    // on CSR with BCSP/H5, the reset resend timeout leads to substate == HCI_INIT_SEND_RESET or HCI_INIT_SEND_RESET_CSR_WARM_BOOT
    // fix: Correct substate and behave as command below
    if (command_completed){
        switch (hci_stack->substate){
            case HCI_INIT_SEND_RESET:
                hci_stack->substate = HCI_INIT_W4_SEND_RESET;
                break;
            case HCI_INIT_SEND_RESET_CSR_WARM_BOOT:
                hci_stack->substate = HCI_INIT_W4_CUSTOM_INIT_CSR_WARM_BOOT;
                break;
            default:
                break;
        }
    }

#endif

    if (!command_completed) return;

    bool need_baud_change = false;
    bool need_addr_change = false;

#ifndef HAVE_HOST_CONTROLLER_API
    need_baud_change = hci_stack->config
                        && hci_stack->chipset
                        && hci_stack->chipset->set_baudrate_command
                        && hci_stack->hci_transport->set_baudrate
                        && ((hci_transport_config_uart_t *)hci_stack->config)->baudrate_main;

    need_addr_change = hci_stack->custom_bd_addr_set
                        && hci_stack->chipset
                        && hci_stack->chipset->set_bd_addr_command;
#endif

    switch(hci_stack->substate){

#ifndef HAVE_HOST_CONTROLLER_API
        case HCI_INIT_SEND_RESET:
            // on CSR with BCSP/H5, resend triggers resend of HCI Reset and leads to substate == HCI_INIT_SEND_RESET
            // fix: just correct substate and behave as command below

            /* fall through */
#endif

        case HCI_INIT_W4_SEND_RESET:
            btstack_run_loop_remove_timer(&hci_stack->timeout);
            hci_stack->substate = HCI_INIT_SEND_READ_LOCAL_VERSION_INFORMATION;
            return;

#ifndef HAVE_HOST_CONTROLLER_API
        case HCI_INIT_W4_SEND_BAUD_CHANGE:
            // for STLC2500D, baud rate change already happened.
            // for others, baud rate gets changed now
            if ((hci_stack->manufacturer != BLUETOOTH_COMPANY_ID_ST_MICROELECTRONICS) && need_baud_change){
                uint32_t baud_rate = hci_transport_uart_get_main_baud_rate();
                log_info("Local baud rate change to %" PRIu32 "(w4_send_baud_change)", baud_rate);
                hci_stack->hci_transport->set_baudrate(baud_rate);
            }
            hci_stack->substate = HCI_INIT_CUSTOM_INIT;
            return;
        case HCI_INIT_W4_CUSTOM_INIT_CSR_WARM_BOOT:
            btstack_run_loop_remove_timer(&hci_stack->timeout);
            hci_stack->substate = HCI_INIT_CUSTOM_INIT;
            return;
        case HCI_INIT_W4_CUSTOM_INIT:
            // repeat custom init
            hci_stack->substate = HCI_INIT_CUSTOM_INIT;
            return;
        case HCI_INIT_W4_CUSTOM_PRE_INIT:
            // repeat custom init
            hci_stack->substate = HCI_INIT_CUSTOM_PRE_INIT;
            return;
#endif

        case HCI_INIT_W4_READ_LOCAL_SUPPORTED_COMMANDS:
            if (need_baud_change && (hci_stack->chipset_result != BTSTACK_CHIPSET_NO_INIT_SCRIPT) &&
              ((hci_stack->manufacturer == BLUETOOTH_COMPANY_ID_BROADCOM_CORPORATION) || 
               (hci_stack->manufacturer == BLUETOOTH_COMPANY_ID_EM_MICROELECTRONIC_MARIN_SA))) {
                hci_stack->substate = HCI_INIT_SEND_BAUD_CHANGE_BCM;
                return;
            }
            if (need_addr_change){
                hci_stack->substate = HCI_INIT_SET_BD_ADDR;
                return;
            }
            hci_stack->substate = HCI_INIT_READ_BD_ADDR;
            return;
#ifndef HAVE_HOST_CONTROLLER_API
        case HCI_INIT_W4_SEND_BAUD_CHANGE_BCM:
            if (need_baud_change){
                uint32_t baud_rate = hci_transport_uart_get_main_baud_rate();
                log_info("Local baud rate change to %" PRIu32 "(w4_send_baud_change_bcm))", baud_rate);
                hci_stack->hci_transport->set_baudrate(baud_rate);
            }
            if (need_addr_change){
                hci_stack->substate = HCI_INIT_SET_BD_ADDR;
                return;
            }
            hci_stack->substate = HCI_INIT_READ_BD_ADDR;
            return;            
        case HCI_INIT_W4_SET_BD_ADDR:
            // for STLC2500D + ATWILC3000, bd addr change only gets active after sending reset command
            if ((hci_stack->manufacturer == BLUETOOTH_COMPANY_ID_ST_MICROELECTRONICS)
            ||  (hci_stack->manufacturer == BLUETOOTH_COMPANY_ID_ATMEL_CORPORATION)){
                hci_stack->substate = HCI_INIT_SEND_RESET_ST_WARM_BOOT;
                return;
            }
            // skipping st warm boot
            hci_stack->substate = HCI_INIT_READ_BD_ADDR;
            return;
        case HCI_INIT_W4_SEND_RESET_ST_WARM_BOOT:
            hci_stack->substate = HCI_INIT_READ_BD_ADDR;
            return;
#endif

        case HCI_INIT_DONE:
            // set state if we came here by fall through
            hci_stack->substate = HCI_INIT_DONE;
            return;

        default:
            break;
    }
    hci_initializing_next_state();
}

static void hci_handle_connection_failed(hci_connection_t * conn, uint8_t status){
    // CC2564C might emit Connection Complete for rejected incoming SCO connection
    // To prevent accidentally free'ing the HCI connection for the ACL connection,
    // check if we have been aware of the HCI connection
    switch (conn->state){
        case SENT_CREATE_CONNECTION:
        case RECEIVED_CONNECTION_REQUEST:
        case ACCEPTED_CONNECTION_REQUEST:
            break;
        default:
            return;
    }

    log_info("Outgoing connection to %s failed", bd_addr_to_str(conn->address));
    bd_addr_t bd_address;
    (void)memcpy(&bd_address, conn->address, 6);

#ifdef ENABLE_CLASSIC
    // cache needed data
    int notify_dedicated_bonding_failed = conn->bonding_flags & BONDING_DEDICATED;
#endif
    
    // connection failed, remove entry
    btstack_linked_list_remove(&hci_stack->connections, (btstack_linked_item_t *) conn);
    btstack_memory_hci_connection_free( conn );

#ifdef ENABLE_CLASSIC
    // notify client if dedicated bonding
    if (notify_dedicated_bonding_failed){
        log_info("hci notify_dedicated_bonding_failed");
        hci_emit_dedicated_bonding_result(bd_address, status);
    }

    // if authentication error, also delete link key
    if (status == ERROR_CODE_AUTHENTICATION_FAILURE) {
        gap_drop_link_key_for_bd_addr(bd_address);
    }
#else
    UNUSED(status);
#endif
}

#ifdef ENABLE_CLASSIC
static void hci_handle_remote_features_page_0(hci_connection_t * conn, const uint8_t * features){
    // SSP Controller
    if (features[6] & (1 << 3)){
        conn->bonding_flags |= BONDING_REMOTE_SUPPORTS_SSP_CONTROLLER;
    }
    // eSCO
    if (features[3] & (1<<7)){
        conn->remote_supported_features[0] |= 1;
    }
    // Extended features
    if (features[7] & (1<<7)){
        conn->remote_supported_features[0] |= 2;
    }
    // SCO packet types
    conn->remote_supported_sco_packets = hci_sco_packet_types_for_features(features);
}

static void hci_handle_remote_features_page_1(hci_connection_t * conn, const uint8_t * features){
    // SSP Host
    if (features[0] & (1 << 0)){
        conn->bonding_flags |= BONDING_REMOTE_SUPPORTS_SSP_HOST;
    }
    // SC Host
    if (features[0] & (1 << 3)){
        conn->bonding_flags |= BONDING_REMOTE_SUPPORTS_SC_HOST;
    }
}

static void hci_handle_remote_features_page_2(hci_connection_t * conn, const uint8_t * features){
    // SC Controller
    if (features[1] & (1 << 0)){
        conn->bonding_flags |= BONDING_REMOTE_SUPPORTS_SC_CONTROLLER;
    }
}

static void hci_handle_remote_features_received(hci_connection_t * conn){
    conn->bonding_flags &= ~BONDING_REMOTE_FEATURES_QUERY_ACTIVE;
    conn->bonding_flags |= BONDING_RECEIVED_REMOTE_FEATURES;
    log_info("Remote features %02x, bonding flags %" PRIx32, conn->remote_supported_features[0], conn->bonding_flags);
    if (conn->bonding_flags & BONDING_DEDICATED){
        conn->bonding_flags |= BONDING_SEND_AUTHENTICATE_REQUEST;
    }
}
static bool hci_remote_sc_enabled(hci_connection_t * connection){
    const uint16_t sc_enabled_mask = BONDING_REMOTE_SUPPORTS_SC_HOST | BONDING_REMOTE_SUPPORTS_SC_CONTROLLER;
    return (connection->bonding_flags & sc_enabled_mask) == sc_enabled_mask;
}

#endif

static void handle_event_for_current_stack_state(const uint8_t * packet, uint16_t size) {
    // handle BT initialization
    if (hci_stack->state == HCI_STATE_INITIALIZING) {
        hci_initializing_event_handler(packet, size);
    }

    // help with BT sleep
    if ((hci_stack->state == HCI_STATE_FALLING_ASLEEP)
        && (hci_stack->substate == HCI_FALLING_ASLEEP_W4_WRITE_SCAN_ENABLE)
        && (hci_event_packet_get_type(packet) == HCI_EVENT_COMMAND_COMPLETE)
        && (hci_event_command_complete_get_command_opcode(packet) == HCI_OPCODE_HCI_WRITE_SCAN_ENABLE)){
        hci_initializing_next_state();
    }
}

#ifdef ENABLE_CLASSIC
static void hci_handle_mutual_authentication_completed(hci_connection_t * conn){
    // bonding complete if connection is authenticated (either initiated or BR/EDR SC)
    conn->requested_security_level = LEVEL_0;
    gap_security_level_t security_level = gap_security_level_for_connection(conn);
    hci_emit_security_level(conn->con_handle, security_level);

    // dedicated bonding
    if ((conn->bonding_flags & BONDING_DEDICATED) != 0){
        conn->bonding_flags &= ~BONDING_DEDICATED;
        conn->bonding_status = security_level == 0 ? ERROR_CODE_INSUFFICIENT_SECURITY : ERROR_CODE_SUCCESS;
#ifdef ENABLE_EXPLICIT_DEDICATED_BONDING_DISCONNECT
        // emit dedicated bonding complete, don't disconnect
        hci_emit_dedicated_bonding_result(conn->address, conn->bonding_status);
#else
        // request disconnect, event is emitted after disconnect
        conn->bonding_flags |= BONDING_DISCONNECT_DEDICATED_DONE;
#endif
    }
}

static void hci_handle_read_encryption_key_size_complete(hci_connection_t * conn, uint8_t encryption_key_size) {
    conn->authentication_flags |= AUTH_FLAG_CONNECTION_ENCRYPTED;
    conn->encryption_key_size = encryption_key_size;

    // mutual authentication complete if authenticated and we have retrieved the encryption key size
    if ((conn->authentication_flags & AUTH_FLAG_CONNECTION_AUTHENTICATED) != 0) {
        hci_handle_mutual_authentication_completed(conn);
    } else {
        // otherwise trigger remote feature request and send authentication request
        hci_trigger_remote_features_for_connection(conn);
        if ((conn->bonding_flags & BONDING_SENT_AUTHENTICATE_REQUEST) == 0) {
            conn->bonding_flags |= BONDING_SEND_AUTHENTICATE_REQUEST;
        }
    }
}
#endif

static void hci_store_local_supported_commands(const uint8_t * packet){
    // create mapping table
#define X(name, offset, bit) { offset, bit },
    static struct {
        uint8_t byte_offset;
        uint8_t bit_position;
    } supported_hci_commands_map [] = {
        SUPPORTED_HCI_COMMANDS
    };
#undef X

    // create names for debug purposes
#ifdef ENABLE_LOG_DEBUG
#define X(name, offset, bit) #name,
    static const char * command_names[] = {
        SUPPORTED_HCI_COMMANDS
    };
#undef X
#endif

    hci_stack->local_supported_commands = 0;
    const uint8_t * commands_map = &packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE+1];
    uint16_t i;
    for (i = 0 ; i < SUPPORTED_HCI_COMMANDS_COUNT ; i++){
        if ((commands_map[supported_hci_commands_map[i].byte_offset] & (1 << supported_hci_commands_map[i].bit_position)) != 0){
#ifdef ENABLE_LOG_DEBUG
            log_info("Command %s (%u) supported %u/%u", command_names[i], i, supported_hci_commands_map[i].byte_offset, supported_hci_commands_map[i].bit_position);
#else
            log_info("Command 0x%02x supported %u/%u", i, supported_hci_commands_map[i].byte_offset, supported_hci_commands_map[i].bit_position);
#endif
            hci_stack->local_supported_commands |= (1LU << i);
        }
    }
    log_info("Local supported commands summary %08" PRIx32, hci_stack->local_supported_commands);
}

static void handle_command_complete_event(uint8_t * packet, uint16_t size){
    UNUSED(size);

    uint16_t manufacturer;
#ifdef ENABLE_CLASSIC
    hci_con_handle_t handle;
    hci_connection_t * conn;
#endif
#if defined(ENABLE_CLASSIC) || (defined(ENABLE_BLE) && defined(ENABLE_LE_ISOCHRONOUS_STREAMS))
    uint8_t status;
#endif
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
    le_audio_cig_t * cig;
#endif

    // get num cmd packets - limit to 1 to reduce complexity
    hci_stack->num_cmd_packets = packet[2] ? 1 : 0;

    uint16_t opcode = hci_event_command_complete_get_command_opcode(packet);
    switch (opcode){
        case HCI_OPCODE_HCI_READ_LOCAL_NAME:
            if (packet[5]) break;
            // terminate, name 248 chars
            packet[6+248] = 0;
            log_info("local name: %s", &packet[6]);
            break;
        case HCI_OPCODE_HCI_READ_BUFFER_SIZE:
            // "The HC_ACL_Data_Packet_Length return parameter will be used to determine the size of the L2CAP segments contained in ACL Data Packets"
            if (hci_stack->state == HCI_STATE_INITIALIZING) {
                uint16_t acl_len = little_endian_read_16(packet, 6);
                uint16_t sco_len = packet[8];

                // determine usable ACL/SCO payload size
                hci_stack->acl_data_packet_length = btstack_min(acl_len, HCI_ACL_PAYLOAD_SIZE);
                hci_stack->sco_data_packet_length = btstack_min(sco_len, HCI_ACL_PAYLOAD_SIZE);

                hci_stack->acl_packets_total_num = (uint8_t) btstack_min(little_endian_read_16(packet,  9), MAX_NR_CONTROLLER_ACL_BUFFERS);
                hci_stack->sco_packets_total_num = (uint8_t) btstack_min(little_endian_read_16(packet, 11), MAX_NR_CONTROLLER_SCO_PACKETS);

                log_info("hci_read_buffer_size: ACL size module %u -> used %u, count %u / SCO size %u, count %u",
                         acl_len, hci_stack->acl_data_packet_length, hci_stack->acl_packets_total_num,
                         hci_stack->sco_data_packet_length, hci_stack->sco_packets_total_num);
            }
            break;
        case HCI_OPCODE_HCI_READ_RSSI:
            if (packet[5] == ERROR_CODE_SUCCESS){
                uint8_t event[5];
                event[0] = GAP_EVENT_RSSI_MEASUREMENT;
                event[1] = 3;
                (void)memcpy(&event[2], &packet[6], 3);
                hci_emit_event(event, sizeof(event), 1);
            }
            break;
#ifdef ENABLE_BLE
        case HCI_OPCODE_HCI_LE_READ_BUFFER_SIZE_V2:
            hci_stack->le_iso_packets_length = little_endian_read_16(packet, 9);
            hci_stack->le_iso_packets_total_num = packet[11];
            log_info("hci_le_read_buffer_size_v2: iso size %u, iso count %u",
                     hci_stack->le_iso_packets_length, hci_stack->le_iso_packets_total_num);

            /* fall through */

        case HCI_OPCODE_HCI_LE_READ_BUFFER_SIZE:
            hci_stack->le_data_packets_length = little_endian_read_16(packet, 6);
            hci_stack->le_acl_packets_total_num = packet[8];
            // determine usable ACL payload size
            if (HCI_ACL_PAYLOAD_SIZE < hci_stack->le_data_packets_length){
                hci_stack->le_data_packets_length = HCI_ACL_PAYLOAD_SIZE;
            }
            log_info("hci_le_read_buffer_size: acl size %u, acl count %u", hci_stack->le_data_packets_length, hci_stack->le_acl_packets_total_num);
            break;
#endif
#ifdef ENABLE_LE_DATA_LENGTH_EXTENSION
        case HCI_OPCODE_HCI_LE_READ_MAXIMUM_DATA_LENGTH:
            hci_stack->le_supported_max_tx_octets = little_endian_read_16(packet, 6);
            hci_stack->le_supported_max_tx_time = little_endian_read_16(packet, 8);
            log_info("hci_le_read_maximum_data_length: tx octets %u, tx time %u us", hci_stack->le_supported_max_tx_octets, hci_stack->le_supported_max_tx_time);
            break;
#endif
#ifdef ENABLE_LE_CENTRAL
        case HCI_OPCODE_HCI_LE_READ_WHITE_LIST_SIZE:
            hci_stack->le_whitelist_capacity = packet[6];
            log_info("hci_le_read_white_list_size: size %u", hci_stack->le_whitelist_capacity);
            break;
#endif
#ifdef ENABLE_LE_PERIPHERAL
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
        case HCI_OPCODE_HCI_LE_READ_MAXIMUM_ADVERTISING_DATA_LENGTH:
            hci_stack->le_maximum_advertising_data_length = little_endian_read_16(packet, 6);
            break;
        case HCI_OPCODE_HCI_LE_SET_EXTENDED_ADVERTISING_PARAMETERS:
            if (hci_stack->le_advertising_set_in_current_command != 0) {
                le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(hci_stack->le_advertising_set_in_current_command);
                hci_stack->le_advertising_set_in_current_command = 0;
                if (advertising_set == NULL) break;
                uint8_t adv_status = packet[6];
                uint8_t tx_power   = packet[7];
                uint8_t event[] = { HCI_EVENT_META_GAP, 4, GAP_SUBEVENT_ADVERTISING_SET_INSTALLED, hci_stack->le_advertising_set_in_current_command, adv_status, tx_power };
                if (adv_status == 0){
                    advertising_set->state |= LE_ADVERTISEMENT_STATE_PARAMS_SET;
                }
                hci_emit_event(event, sizeof(event), 1);
            }
            break;
        case HCI_OPCODE_HCI_LE_REMOVE_ADVERTISING_SET:
            if (hci_stack->le_advertising_set_in_current_command != 0) {
                le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(hci_stack->le_advertising_set_in_current_command);
                hci_stack->le_advertising_set_in_current_command = 0;
                if (advertising_set == NULL) break;
                uint8_t adv_status = packet[5];
                uint8_t event[] = { HCI_EVENT_META_GAP, 3, GAP_SUBEVENT_ADVERTISING_SET_REMOVED, hci_stack->le_advertising_set_in_current_command, adv_status };
                if (adv_status == 0){
                    btstack_linked_list_remove(&hci_stack->le_advertising_sets, (btstack_linked_item_t *) advertising_set);
                }
                hci_emit_event(event, sizeof(event), 1);
            }
            break;
#endif
#endif
        case HCI_OPCODE_HCI_READ_BD_ADDR:
            reverse_bd_addr(&packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE + 1], hci_stack->local_bd_addr);
            log_info("Local Address, Status: 0x%02x: Addr: %s", packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE], bd_addr_to_str(hci_stack->local_bd_addr));
#ifdef ENABLE_CLASSIC
            if (hci_stack->link_key_db){
                hci_stack->link_key_db->set_local_bd_addr(hci_stack->local_bd_addr);
            }
#endif
            break;
#ifdef ENABLE_CLASSIC
        case HCI_OPCODE_HCI_WRITE_SCAN_ENABLE:
            hci_emit_scan_mode_changed(hci_stack->discoverable, hci_stack->connectable);
            break;
        case HCI_OPCODE_HCI_PERIODIC_INQUIRY_MODE:
            status = hci_event_command_complete_get_return_parameters(packet)[0];
            if (status == ERROR_CODE_SUCCESS) {
                hci_stack->inquiry_state = GAP_INQUIRY_STATE_PERIODIC;
            } else {
                hci_stack->inquiry_state = GAP_INQUIRY_STATE_IDLE;
            }
            break;
        case HCI_OPCODE_HCI_INQUIRY_CANCEL:
        case HCI_OPCODE_HCI_EXIT_PERIODIC_INQUIRY_MODE:
            if (hci_stack->inquiry_state == GAP_INQUIRY_STATE_W4_CANCELLED){
                hci_stack->inquiry_state = GAP_INQUIRY_STATE_IDLE;
                uint8_t event[] = { GAP_EVENT_INQUIRY_COMPLETE, 1, 0};
                hci_emit_event(event, sizeof(event), 1);
            }
            break;
#endif
        case HCI_OPCODE_HCI_READ_LOCAL_SUPPORTED_FEATURES:
            (void)memcpy(hci_stack->local_supported_features, &packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE + 1], 8);

#ifdef ENABLE_CLASSIC
            // determine usable ACL packet types based on host buffer size and supported features
            hci_stack->usable_packet_types_acl = hci_acl_packet_types_for_buffer_size_and_local_features(HCI_ACL_PAYLOAD_SIZE, &hci_stack->local_supported_features[0]);
            log_info("ACL Packet types %04x", hci_stack->usable_packet_types_acl);
            // determine usable SCO packet types based on supported features
            hci_stack->usable_packet_types_sco = hci_sco_packet_types_for_features(
                    &hci_stack->local_supported_features[0]);
            log_info("SCO Packet types %04x - eSCO %u", hci_stack->usable_packet_types_sco, hci_extended_sco_link_supported());
#endif
            // Classic/LE
            log_info("BR/EDR support %u, LE support %u", hci_classic_supported(), hci_le_supported());
            break;
        case HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION:
            manufacturer = little_endian_read_16(packet, 10);
            // map Cypress & Infineon to Broadcom
            switch (manufacturer){
                case BLUETOOTH_COMPANY_ID_CYPRESS_SEMICONDUCTOR:
                case BLUETOOTH_COMPANY_ID_INFINEON_TECHNOLOGIES_AG:
                    log_info("Treat Cypress/Infineon as Broadcom");
                    manufacturer = BLUETOOTH_COMPANY_ID_BROADCOM_CORPORATION;
                    little_endian_store_16(packet, 10, manufacturer);
                    break;
                default:
                    break;
            }
            hci_stack->manufacturer = manufacturer;
            log_info("Manufacturer: 0x%04x", hci_stack->manufacturer);
            break;
        case HCI_OPCODE_HCI_READ_LOCAL_SUPPORTED_COMMANDS:
            hci_store_local_supported_commands(packet);
            break;
#ifdef ENABLE_CLASSIC
        case HCI_OPCODE_HCI_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE:
            if (packet[5]) return;
            hci_stack->synchronous_flow_control_enabled = 1;
            break;
        case HCI_OPCODE_HCI_READ_ENCRYPTION_KEY_SIZE:
            status = packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE];
            handle = little_endian_read_16(packet, OFFSET_OF_DATA_IN_COMMAND_COMPLETE+1);
            conn   = hci_connection_for_handle(handle);
            if (conn != NULL) {
                uint8_t key_size = 0;
                if (status == 0){
                    key_size = packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE+3];
                    log_info("Handle %04x key Size: %u", handle, key_size);
                } else {
                    key_size = 1;
                    log_info("Read Encryption Key Size failed 0x%02x-> assuming insecure connection with key size of 1", status);
                }
                hci_handle_read_encryption_key_size_complete(conn, key_size);
            }
            break;
        // assert pairing complete event is emitted.
        // note: for SSP, Simple Pairing Complete Event is sufficient, but we want to be more robust
        case HCI_OPCODE_HCI_PIN_CODE_REQUEST_NEGATIVE_REPLY:
        case HCI_OPCODE_HCI_USER_PASSKEY_REQUEST_NEGATIVE_REPLY:
        case HCI_OPCODE_HCI_USER_CONFIRMATION_REQUEST_NEGATIVE_REPLY:
            hci_stack->gap_pairing_state = GAP_PAIRING_STATE_IDLE;
            // lookup connection by gap pairing addr
            conn = hci_connection_for_bd_addr_and_type(hci_stack->gap_pairing_addr, BD_ADDR_TYPE_ACL);
            if (conn == NULL) break;
            hci_pairing_complete(conn, ERROR_CODE_AUTHENTICATION_FAILURE);
            break;

#ifdef ENABLE_CLASSIC_PAIRING_OOB
        case HCI_OPCODE_HCI_READ_LOCAL_OOB_DATA:
        case HCI_OPCODE_HCI_READ_LOCAL_EXTENDED_OOB_DATA:{
            uint8_t event[67];
            event[0] = GAP_EVENT_LOCAL_OOB_DATA;
            event[1] = 65;
            (void)memset(&event[2], 0, 65);
            if (packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE] == ERROR_CODE_SUCCESS){
                (void)memcpy(&event[3], &packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE+1], 32);
                if (opcode == HCI_OPCODE_HCI_READ_LOCAL_EXTENDED_OOB_DATA){
                    event[2] = 3;
                    (void)memcpy(&event[35], &packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE+33], 32);
                } else {
                    event[2] = 1;
                }
            }
            hci_emit_event(event, sizeof(event), 0);
            break;
        }

        // note: only needed if user does not provide OOB data
        case HCI_OPCODE_HCI_REMOTE_OOB_DATA_REQUEST_NEGATIVE_REPLY:
            conn = hci_connection_for_handle(hci_stack->classic_oob_con_handle);
            hci_stack->classic_oob_con_handle = HCI_CON_HANDLE_INVALID;
            if (conn == NULL) break;
            hci_pairing_complete(conn, ERROR_CODE_AUTHENTICATION_FAILURE);
            break;
#endif
#endif
#ifdef ENABLE_BLE
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
        case HCI_OPCODE_HCI_LE_SET_CIG_PARAMETERS:
            // lookup CIG
            cig = hci_cig_for_id(hci_stack->iso_active_operation_group_id);
            if (cig != NULL){
                status = packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE];
                uint8_t i = 0;
                if (status == ERROR_CODE_SUCCESS){
                    // assign CIS handles to pre-allocated CIS
                    btstack_linked_list_iterator_t it;
                    btstack_linked_list_iterator_init(&it, &hci_stack->iso_streams);
                    while (btstack_linked_list_iterator_has_next(&it) && (i < cig->num_cis)) {
                        hci_iso_stream_t *iso_stream = (hci_iso_stream_t *) btstack_linked_list_iterator_next(&it);
                        if ((iso_stream->group_id == hci_stack->iso_active_operation_group_id) &&
                            (iso_stream->iso_type == HCI_ISO_TYPE_CIS)){
                            hci_con_handle_t cis_handle = little_endian_read_16(packet, OFFSET_OF_DATA_IN_COMMAND_COMPLETE+3+(2*i));
                            iso_stream->cis_handle  = cis_handle;
                            cig->cis_con_handles[i] = cis_handle;
                            i++;
                        }
                    }
                    cig->state = LE_AUDIO_CIG_STATE_W4_CIS_REQUEST;
                    hci_emit_cig_created(cig, status);
                } else {
                    hci_emit_cig_created(cig, status);
                    btstack_linked_list_remove(&hci_stack->le_audio_cigs, (btstack_linked_item_t *) cig);
                }
            }
            hci_stack->iso_active_operation_type = HCI_ISO_TYPE_INVALID;
            break;
        case HCI_OPCODE_HCI_LE_CREATE_CIS:
            status = packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE];
            if (status != ERROR_CODE_SUCCESS){
                hci_iso_stream_requested_finalize(HCI_ISO_GROUP_ID_INVALID);
            }
            break;
        case HCI_OPCODE_HCI_LE_ACCEPT_CIS_REQUEST:
            status = packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE];
            if (status != ERROR_CODE_SUCCESS){
                hci_iso_stream_requested_finalize(HCI_ISO_GROUP_ID_INVALID);
            }
            break;
        case HCI_OPCODE_HCI_LE_SETUP_ISO_DATA_PATH: {
            // lookup BIG by state
            btstack_linked_list_iterator_t it;
            btstack_linked_list_iterator_init(&it, &hci_stack->le_audio_bigs);
            while (btstack_linked_list_iterator_has_next(&it)) {
                le_audio_big_t *big = (le_audio_big_t *) btstack_linked_list_iterator_next(&it);
                if (big->state == LE_AUDIO_BIG_STATE_W4_SETUP_ISO_PATH){
                    status = packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE];
                    if (status == ERROR_CODE_SUCCESS){
                        big->state_vars.next_bis++;
                        if (big->state_vars.next_bis == big->num_bis){
                            big->state = LE_AUDIO_BIG_STATE_ACTIVE;
                            hci_emit_big_created(big, ERROR_CODE_SUCCESS);
                        } else {
                            big->state = LE_AUDIO_BIG_STATE_SETUP_ISO_PATH;
                        }
                    } else {
                        big->state = LE_AUDIO_BIG_STATE_SETUP_ISO_PATHS_FAILED;
                        big->state_vars.status = status;
                    }
                    return;
                }
            }
            btstack_linked_list_iterator_init(&it, &hci_stack->le_audio_big_syncs);
            while (btstack_linked_list_iterator_has_next(&it)) {
                le_audio_big_sync_t *big_sync = (le_audio_big_sync_t *) btstack_linked_list_iterator_next(&it);
                if (big_sync->state == LE_AUDIO_BIG_STATE_W4_SETUP_ISO_PATH){
                    status = packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE];
                    if (status == ERROR_CODE_SUCCESS){
                        big_sync->state_vars.next_bis++;
                        if (big_sync->state_vars.next_bis == big_sync->num_bis){
                            big_sync->state = LE_AUDIO_BIG_STATE_ACTIVE;
                            hci_emit_big_sync_created(big_sync, ERROR_CODE_SUCCESS);
                        } else {
                            big_sync->state = LE_AUDIO_BIG_STATE_SETUP_ISO_PATH;
                        }
                    } else {
                        big_sync->state = LE_AUDIO_BIG_STATE_SETUP_ISO_PATHS_FAILED;
                        big_sync->state_vars.status = status;
                    }
                    return;
                }
            }
            // Lookup CIS via active group operation
            if (hci_stack->iso_active_operation_type == HCI_ISO_TYPE_CIS){
                if (hci_stack->iso_active_operation_group_id == HCI_ISO_GROUP_ID_SINGLE_CIS){
                    hci_stack->iso_active_operation_type = HCI_ISO_TYPE_INVALID;

                    // lookup CIS by state
                    btstack_linked_list_iterator_t it;
                    btstack_linked_list_iterator_init(&it, &hci_stack->iso_streams);
                    status = packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE];
                    while (btstack_linked_list_iterator_has_next(&it)){
                        hci_iso_stream_t * iso_stream = (hci_iso_stream_t *) btstack_linked_list_iterator_next(&it);
                        handle = iso_stream->cis_handle;
                        bool emit_cis_created = false;
                        switch (iso_stream->state){
                            case HCI_ISO_STREAM_STATE_W4_ISO_SETUP_INPUT:
                                if (status != ERROR_CODE_SUCCESS){
                                    emit_cis_created = true;
                                    break;
                                }
                                if (iso_stream->max_sdu_c_to_p > 0){
                                    iso_stream->state = HCI_ISO_STREAM_STATE_W2_SETUP_ISO_INPUT;
                                } else {
                                    emit_cis_created = true;
                                }
                                break;
                            case HCI_ISO_STREAM_STATE_W4_ISO_SETUP_OUTPUT:
                                emit_cis_created = true;
                                break;
                            default:
                                break;
                        }
                        if (emit_cis_created){
                            hci_cis_handle_created(iso_stream, status);
                        }
                    }
                } else {
                    cig = hci_cig_for_id(hci_stack->iso_active_operation_group_id);
                    hci_stack->iso_active_operation_type = HCI_ISO_TYPE_INVALID;
                    if (cig != NULL) {
                        // emit cis created if all ISO Paths have been created
                        // assume we are central
                        uint8_t cis_index     = cig->state_vars.next_cis >> 1;
                        uint8_t cis_direction = cig->state_vars.next_cis & 1;
                        bool outgoing_needed  = cig->params->cis_params[cis_index].max_sdu_p_to_c > 0;
                        // if outgoing has been setup, or incoming was setup but outgoing not required
                        if ((cis_direction == 1) || (outgoing_needed == false)){
                            // lookup iso stream by cig/cis
                            while (btstack_linked_list_iterator_has_next(&it)) {
                                hci_iso_stream_t *iso_stream = (hci_iso_stream_t *) btstack_linked_list_iterator_next(&it);
                                if ((iso_stream->group_id == cig->cig_id) && (iso_stream->stream_id == cis_index)){
                                    hci_cis_handle_created(iso_stream, status);
                                }
                            }
                        }
                        // next state
                        cig->state_vars.next_cis++;
                        cig->state = LE_AUDIO_CIG_STATE_SETUP_ISO_PATH;
                    }
                }
            }
            break;
        }
        case HCI_OPCODE_HCI_LE_BIG_TERMINATE_SYNC: {
            // lookup BIG by state
            btstack_linked_list_iterator_t it;
            btstack_linked_list_iterator_init(&it, &hci_stack->le_audio_big_syncs);
            while (btstack_linked_list_iterator_has_next(&it)) {
                le_audio_big_sync_t *big_sync = (le_audio_big_sync_t *) btstack_linked_list_iterator_next(&it);
                uint8_t big_handle = big_sync->big_handle;
                switch (big_sync->state){
                    case LE_AUDIO_BIG_STATE_W4_TERMINATED_AFTER_SETUP_FAILED:
                        btstack_linked_list_iterator_remove(&it);
                        hci_emit_big_sync_created(big_sync, big_sync->state_vars.status);
                        return;
                    default:
                        btstack_linked_list_iterator_remove(&it);
                        hci_emit_big_sync_stopped(big_handle);
                        return;
                }
            }
            break;
        }
#endif
#endif
        default:
            break;
    }
}

static void handle_command_status_event(uint8_t * packet, uint16_t size) {
    UNUSED(size);

    // get num cmd packets - limit to 1 to reduce complexity
    hci_stack->num_cmd_packets = packet[3] ? 1 : 0;

    // get opcode and command status
    uint16_t opcode = hci_event_command_status_get_command_opcode(packet);

#if defined(ENABLE_CLASSIC) || defined(ENABLE_LE_CENTRAL) || defined(ENABLE_LE_ISOCHRONOUS_STREAMS)
    uint8_t status = hci_event_command_status_get_status(packet);
#endif

#if defined(ENABLE_CLASSIC) || defined(ENABLE_LE_CENTRAL)
    bd_addr_type_t addr_type;
    bd_addr_t addr;
#endif

    switch (opcode){
#ifdef ENABLE_CLASSIC
        case HCI_OPCODE_HCI_CREATE_CONNECTION:
        case HCI_OPCODE_HCI_SETUP_SYNCHRONOUS_CONNECTION:
        case HCI_OPCODE_HCI_ACCEPT_SYNCHRONOUS_CONNECTION:
#endif
#ifdef ENABLE_LE_CENTRAL
        case HCI_OPCODE_HCI_LE_CREATE_CONNECTION:
#endif
#if defined(ENABLE_CLASSIC) || defined(ENABLE_LE_CENTRAL)
            addr_type = hci_stack->outgoing_addr_type;
            memcpy(addr, hci_stack->outgoing_addr, 6);

            // reset outgoing address info
            memset(hci_stack->outgoing_addr, 0, 6);
            hci_stack->outgoing_addr_type = BD_ADDR_TYPE_UNKNOWN;

            // on error
            if (status != ERROR_CODE_SUCCESS){
#ifdef ENABLE_LE_CENTRAL
                if (hci_is_le_connection_type(addr_type)){
                    hci_stack->le_connecting_state = LE_CONNECTING_IDLE;
                    hci_stack->le_connecting_request = LE_CONNECTING_IDLE;
                }
#endif
                // error => outgoing connection failed
                hci_connection_t * conn = hci_connection_for_bd_addr_and_type(addr, addr_type);
                if (conn != NULL){
                    hci_handle_connection_failed(conn, status);
                }
            }
            break;
#endif
#ifdef ENABLE_CLASSIC
        case HCI_OPCODE_HCI_INQUIRY:
            if (status == ERROR_CODE_SUCCESS) {
                hci_stack->inquiry_state = GAP_INQUIRY_STATE_ACTIVE;
            } else {
                hci_stack->inquiry_state = GAP_INQUIRY_STATE_IDLE;
            }
            break;
#endif
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
        case HCI_OPCODE_HCI_LE_CREATE_CIS:
        case HCI_OPCODE_HCI_LE_ACCEPT_CIS_REQUEST:
            if (status == ERROR_CODE_SUCCESS){
                hci_iso_stream_requested_confirm(HCI_ISO_GROUP_ID_INVALID);
            } else {
                hci_iso_stream_requested_finalize(HCI_ISO_GROUP_ID_INVALID);
            }
            break;
#endif /* ENABLE_LE_ISOCHRONOUS_STREAMS */
        default:
            break;
    }
}

#ifdef ENABLE_BLE
static void event_handle_le_connection_complete(const uint8_t * packet){
	bd_addr_t addr;
	bd_addr_type_t addr_type;
	hci_connection_t * conn;

    // read fields
    uint8_t status = packet[3];
    hci_role_t role = (hci_role_t) packet[6];

    // support different connection complete events
    uint16_t conn_interval;
    switch (packet[2]){
        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
            conn_interval = hci_subevent_le_connection_complete_get_conn_interval(packet);
            break;
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
        case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE_V1:
            conn_interval = hci_subevent_le_enhanced_connection_complete_v1_get_conn_interval(packet);
            break;
        case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE_V2:
            conn_interval = hci_subevent_le_enhanced_connection_complete_v2_get_conn_interval(packet);
            break;
#endif
        default:
            btstack_unreachable();
            return;
    }

	// Connection management
	reverse_bd_addr(&packet[8], addr);
	addr_type = (bd_addr_type_t)packet[7];
    log_info("LE Connection_complete (status=%u) type %u, %s", status, addr_type, bd_addr_to_str(addr));
	conn = hci_connection_for_bd_addr_and_type(addr, addr_type);

#ifdef ENABLE_LE_CENTRAL
	// handle error: error is reported only to the initiator -> outgoing connection
	if (status){

		// handle cancelled outgoing connection
		// "If the cancellation was successful then, after the Command Complete event for the LE_Create_Connection_Cancel command,
		//  either an LE Connection Complete or an LE Enhanced Connection Complete event shall be generated.
		//  In either case, the event shall be sent with the error code Unknown Connection Identifier (0x02)."
		if (status == ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER){
		    // reset state
            hci_stack->le_connecting_state   = LE_CONNECTING_IDLE;
            hci_stack->le_connecting_request = LE_CONNECTING_IDLE;
			// get outgoing connection conn struct for direct connect
			conn = gap_get_outgoing_connection();
		}

		// outgoing le connection establishment is done
		if (conn){
			// remove entry
			btstack_linked_list_remove(&hci_stack->connections, (btstack_linked_item_t *) conn);
			btstack_memory_hci_connection_free( conn );
		}
		return;
	}
#endif

	// on success, both hosts receive connection complete event
    if (role == HCI_ROLE_MASTER){
#ifdef ENABLE_LE_CENTRAL
		// if we're master on an le connection, it was an outgoing connection and we're done with it
		// note: no hci_connection_t object exists yet for connect with whitelist
		if (hci_is_le_connection_type(addr_type)){
			hci_stack->le_connecting_state   = LE_CONNECTING_IDLE;
			hci_stack->le_connecting_request = LE_CONNECTING_IDLE;
		}
#endif
	} else {
#ifdef ENABLE_LE_PERIPHERAL
		// if we're slave, it was an incoming connection, advertisements have stopped
        hci_stack->le_advertisements_state &= ~LE_ADVERTISEMENT_STATE_ACTIVE;
#endif
	}

	// LE connections are auto-accepted, so just create a connection if there isn't one already
	if (!conn){
		conn = create_connection_for_bd_addr_and_type(addr, addr_type, role);
	}

	// no memory, sorry.
	if (!conn){
		return;
	}

	conn->state = OPEN;
	conn->con_handle             = hci_subevent_le_connection_complete_get_connection_handle(packet);
    conn->le_connection_interval = conn_interval;

    // workaround: PAST doesn't work without LE Read Remote Features on PacketCraft Controller with LMP 568B
    conn->gap_connection_tasks = GAP_CONNECTION_TASK_LE_READ_REMOTE_FEATURES;

#ifdef ENABLE_LE_PERIPHERAL
	if (role == HCI_ROLE_SLAVE){
		hci_update_advertisements_enabled_for_current_roles();
	}
#endif

    // init unenhanced att bearer mtu
    conn->att_connection.mtu = ATT_DEFAULT_MTU;
    conn->att_connection.mtu_exchanged = false;

    // TODO: store - role, peer address type, conn_interval, conn_latency, supervision timeout, master clock

	// restart timer
	// btstack_run_loop_set_timer(&conn->timeout, HCI_CONNECTION_TIMEOUT_MS);
	// btstack_run_loop_add_timer(&conn->timeout);

	log_info("New connection: handle %u, %s", conn->con_handle, bd_addr_to_str(conn->address));

	hci_emit_nr_connections_changed();
}
#endif

#ifdef ENABLE_CLASSIC
static bool hci_ssp_security_level_possible_for_io_cap(gap_security_level_t level, uint8_t io_cap_local, uint8_t io_cap_remote){
    if (io_cap_local == SSP_IO_CAPABILITY_UNKNOWN) return false;
    // LEVEL_4 is tested by l2cap
    // LEVEL 3 requires MITM protection -> check io capabilities if Authenticated is possible
    // @see: Core Spec v5.3, Vol 3, Part C, Table 5.7
    if (level >= LEVEL_3){
        // MITM not possible without keyboard or display
        if (io_cap_remote >= SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT) return false;
        if (io_cap_local  >= SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT) return false;

        // MITM possible if one side has keyboard and the other has keyboard or display
        if (io_cap_remote == SSP_IO_CAPABILITY_KEYBOARD_ONLY)      return true;
        if (io_cap_local  == SSP_IO_CAPABILITY_KEYBOARD_ONLY)      return true;

        // MITM not possible if one side has only display and other side has no keyboard
        if (io_cap_remote == SSP_IO_CAPABILITY_DISPLAY_ONLY)       return false;
        if (io_cap_local  == SSP_IO_CAPABILITY_DISPLAY_ONLY)       return false;
    }
    // LEVEL 2 requires SSP, which is a given
    return true;
}

static void hci_ssp_assess_security_on_io_cap_request(hci_connection_t * conn){
    // get requested security level
    gap_security_level_t requested_security_level = conn->requested_security_level;
    if (hci_stack->gap_secure_connections_only_mode){
        requested_security_level = LEVEL_4;
    }

    // assess security: LEVEL 4 requires SC
    // skip this preliminary test if remote features are not available yet to work around potential issue in ESP32 controller
    if ((requested_security_level == LEVEL_4) &&
        ((conn->bonding_flags & BONDING_RECEIVED_REMOTE_FEATURES) != 0) &&
        !hci_remote_sc_enabled(conn)){
        log_info("Level 4 required, but SC not supported -> abort");
        hci_pairing_complete(conn, ERROR_CODE_INSUFFICIENT_SECURITY);
        connectionSetAuthenticationFlags(conn, AUTH_FLAG_SEND_IO_CAPABILITIES_NEGATIVE_REPLY);
        return;
    }

    // assess security based on io capabilities
    if (conn->authentication_flags & AUTH_FLAG_RECV_IO_CAPABILITIES_RESPONSE){
        // responder: fully validate io caps of both sides as well as OOB data
        bool security_possible = false;
        security_possible = hci_ssp_security_level_possible_for_io_cap(requested_security_level, hci_stack->ssp_io_capability, conn->io_cap_response_io);

#ifdef ENABLE_CLASSIC_PAIRING_OOB
        // We assume that both Controller can reach LEVEL 4, if one side has received P-192 and the other has received P-256,
        // so we merge the OOB data availability
        uint8_t have_oob_data = conn->io_cap_response_oob_data;
        if (conn->classic_oob_c_192 != NULL){
            have_oob_data |= 1;
        }
        if (conn->classic_oob_c_256 != NULL){
            have_oob_data |= 2;
        }
        // for up to Level 3, either P-192 as well as P-256 will do
        // if we don't support SC, then a) conn->classic_oob_c_256 will be NULL and b) remote should not report P-256 available
        // if remote does not SC, we should not receive P-256 data either
        if ((requested_security_level <= LEVEL_3) && (have_oob_data != 0)){
            security_possible = true;
        }
        // for Level 4, P-256 is needed
        if ((requested_security_level == LEVEL_4 && ((have_oob_data & 2) != 0))){
            security_possible = true;
        }
#endif

        if (security_possible == false){
            log_info("IOCap/OOB insufficient for level %u -> abort", requested_security_level);
            hci_pairing_complete(conn, ERROR_CODE_INSUFFICIENT_SECURITY);
            connectionSetAuthenticationFlags(conn, AUTH_FLAG_SEND_IO_CAPABILITIES_NEGATIVE_REPLY);
            return;
        }
    } else {
        // initiator: remote io cap not yet, only check if we have ability for MITM protection if requested and OOB is not supported
#ifndef ENABLE_CLASSIC_PAIRING_OOB
#ifndef ENABLE_EXPLICIT_IO_CAPABILITIES_REPLY
        if ((conn->requested_security_level >= LEVEL_3) && (hci_stack->ssp_io_capability >= SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT)){
            log_info("Level 3+ required, but no input/output -> abort");
            hci_pairing_complete(conn, ERROR_CODE_INSUFFICIENT_SECURITY);
            connectionSetAuthenticationFlags(conn, AUTH_FLAG_SEND_IO_CAPABILITIES_NEGATIVE_REPLY);
            return;
        }
#endif
#endif
    }

#ifndef ENABLE_EXPLICIT_IO_CAPABILITIES_REPLY
    if (hci_stack->ssp_io_capability != SSP_IO_CAPABILITY_UNKNOWN){
        connectionSetAuthenticationFlags(conn, AUTH_FLAG_SEND_IO_CAPABILITIES_REPLY);
    } else {
        connectionSetAuthenticationFlags(conn, AUTH_FLAG_SEND_IO_CAPABILITIES_NEGATIVE_REPLY);
    }
#endif
}

#endif

static void event_handler(uint8_t *packet, uint16_t size){

    uint16_t event_length = packet[1];

    // assert packet is complete
    if (size != (event_length + 2u)){
        log_error("event_handler called with packet of wrong size %d, expected %u => dropping packet", size, event_length + 2);
        return;
    }

    hci_con_handle_t handle;
    hci_connection_t * conn;
    int i;

#ifdef ENABLE_CLASSIC
    hci_link_type_t link_type;
    bd_addr_t addr;
    bd_addr_type_t addr_type;
#endif
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
    hci_iso_stream_t * iso_stream;
    le_audio_big_t   * big;
    le_audio_big_sync_t * big_sync;
#endif

    // log_info("HCI:EVENT:%02x", hci_event_packet_get_type(packet));
    
    switch (hci_event_packet_get_type(packet)) {
                        
        case HCI_EVENT_COMMAND_COMPLETE:
            handle_command_complete_event(packet, size);
            break;
            
        case HCI_EVENT_COMMAND_STATUS:
            handle_command_status_event(packet, size);
            break;

        case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS:{
            if (size < 3) return;
            uint16_t num_handles = packet[2];
            if (size != (3u + num_handles * 4u)) return;
#ifdef ENABLE_CLASSIC
            bool notify_sco = false;
#endif
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
            bool notify_iso = false;
#endif
            uint16_t offset = 3;
            for (i=0; i<num_handles;i++){
                handle = little_endian_read_16(packet, offset) & 0x0fffu;
                offset += 2u;
                uint16_t num_packets = little_endian_read_16(packet, offset);
                offset += 2u;
                
                conn = hci_connection_for_handle(handle);
                if (conn != NULL) {

                    if (conn->num_packets_sent >= num_packets) {
                        conn->num_packets_sent -= num_packets;
                    } else {
                        log_error("hci_number_completed_packets, more packet slots freed then sent.");
                        conn->num_packets_sent = 0;
                    }
                    // log_info("hci_number_completed_packet %u processed for handle %u, outstanding %u", num_packets, handle, conn->num_packets_sent);
#ifdef ENABLE_CLASSIC
                    if (conn->address_type == BD_ADDR_TYPE_SCO){
                        notify_sco = true;
                    }
#endif
                }

#ifdef ENABLE_CONTROLLER_DUMP_PACKETS
                hci_controller_dump_packets();
#endif

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
                if (conn == NULL){
                    hci_iso_stream_t * iso_stream = hci_iso_stream_for_con_handle(handle);
                    if (iso_stream != NULL){
                        if (iso_stream->num_packets_sent >= num_packets) {
                            iso_stream->num_packets_sent -= num_packets;
                        } else {
                            log_error("hci_number_completed_packets, more packet slots freed then sent.");
                            iso_stream->num_packets_sent = 0;
                        }
                        if (iso_stream->iso_type == HCI_ISO_TYPE_BIS){
                            le_audio_big_t * big = hci_big_for_handle(iso_stream->group_id);
                            if (big != NULL){
                                big->num_completed_timestamp_current_valid = true;
                                big->num_completed_timestamp_current_ms = btstack_run_loop_get_time_ms();
                            }
                        }
                        log_info("hci_number_completed_packet %u processed for handle %u, outstanding %u",
                                 num_packets, handle, iso_stream->num_packets_sent);
                        notify_iso = true;
                    }
                }
#endif
            }

#ifdef ENABLE_CLASSIC
            if (notify_sco){
                hci_notify_if_sco_can_send_now();
            }
#endif
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
            if (notify_iso){
                hci_iso_notify_can_send_now();
            }
#endif
            break;
        }

#ifdef ENABLE_CLASSIC
        case HCI_EVENT_FLUSH_OCCURRED:
            // flush occurs only if automatic flush has been enabled by gap_enable_link_watchdog()
            handle = hci_event_flush_occurred_get_handle(packet);
            conn = hci_connection_for_handle(handle);
            if (conn) {
                log_info("Flush occurred, disconnect 0x%04x", handle);
                conn->state = SEND_DISCONNECT;
            }
            break;

        case HCI_EVENT_INQUIRY_COMPLETE:
            if (hci_stack->inquiry_state == GAP_INQUIRY_STATE_ACTIVE){
                hci_stack->inquiry_state = GAP_INQUIRY_STATE_IDLE;
                uint8_t event[] = { GAP_EVENT_INQUIRY_COMPLETE, 1, 0};
                hci_emit_event(event, sizeof(event), 1);
            }
            break;
        case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
            if (hci_stack->remote_name_state == GAP_REMOTE_NAME_STATE_W4_COMPLETE){
                hci_stack->remote_name_state = GAP_REMOTE_NAME_STATE_IDLE;
            }
            break;
        case HCI_EVENT_CONNECTION_REQUEST:
            reverse_bd_addr(&packet[2], addr);
            link_type = (hci_link_type_t) packet[11];

            // CVE-2020-26555: reject incoming connection from device with same BD ADDR
            if (memcmp(hci_stack->local_bd_addr, addr, 6) == 0){
                hci_stack->decline_reason = ERROR_CODE_CONNECTION_REJECTED_DUE_TO_UNACCEPTABLE_BD_ADDR;
                bd_addr_copy(hci_stack->decline_addr, addr);
                break;
            }

            if (hci_stack->gap_classic_accept_callback != NULL){
                if ((*hci_stack->gap_classic_accept_callback)(addr, link_type) == 0){
                    hci_stack->decline_reason = ERROR_CODE_CONNECTION_REJECTED_DUE_TO_SECURITY_REASONS;
                    bd_addr_copy(hci_stack->decline_addr, addr);
                    break;
                }
            } 

            // TODO: eval COD 8-10
            log_info("Connection_incoming: %s, type %u", bd_addr_to_str(addr), (unsigned int) link_type);
            addr_type = (link_type == HCI_LINK_TYPE_ACL) ? BD_ADDR_TYPE_ACL : BD_ADDR_TYPE_SCO;
            conn = hci_connection_for_bd_addr_and_type(addr, addr_type);
            if (!conn) {
                conn = create_connection_for_bd_addr_and_type(addr, addr_type, HCI_ROLE_SLAVE);
            }
            if (!conn) {
                // CONNECTION REJECTED DUE TO LIMITED RESOURCES (0X0D)
                hci_stack->decline_reason = ERROR_CODE_CONNECTION_REJECTED_DUE_TO_LIMITED_RESOURCES;
                bd_addr_copy(hci_stack->decline_addr, addr);
                hci_run();
                // avoid event to higher layer
                return;
            }
            conn->state = RECEIVED_CONNECTION_REQUEST;
            // store info about eSCO
            if (link_type == HCI_LINK_TYPE_ESCO){
                conn->remote_supported_features[0] |= 1;
            }
            hci_run();
            break;
            
        case HCI_EVENT_CONNECTION_COMPLETE:
            // Connection management
            reverse_bd_addr(&packet[5], addr);
            log_info("Connection_complete (status=%u) %s", packet[2], bd_addr_to_str(addr));
            addr_type = BD_ADDR_TYPE_ACL;
            conn = hci_connection_for_bd_addr_and_type(addr, addr_type);
            if (conn) {
                switch (conn->state){
                    // expected states
                    case ACCEPTED_CONNECTION_REQUEST:
                    case SENT_CREATE_CONNECTION:
                        break;
                    // unexpected state -> ignore
                    default:
                        // don't forward event to app
                        return;
                }
                if (!packet[2]){
                    conn->state = OPEN;
                    conn->con_handle = little_endian_read_16(packet, 3);

                    // trigger write supervision timeout if we're master
                    if ((hci_stack->link_supervision_timeout != HCI_LINK_SUPERVISION_TIMEOUT_DEFAULT) && (conn->role == HCI_ROLE_MASTER)){
                        conn->gap_connection_tasks |= GAP_CONNECTION_TASK_WRITE_SUPERVISION_TIMEOUT;
                    }

                    // trigger write automatic flush timeout
                    if (hci_stack->automatic_flush_timeout != 0){
                        conn->gap_connection_tasks |= GAP_CONNECTION_TASK_WRITE_AUTOMATIC_FLUSH_TIMEOUT;
                    }

                    // restart timer
                    btstack_run_loop_set_timer(&conn->timeout, HCI_CONNECTION_TIMEOUT_MS);
                    btstack_run_loop_add_timer(&conn->timeout);

                    // trigger remote features for dedicated bonding
                    if ((conn->bonding_flags & BONDING_DEDICATED) != 0){
                        hci_trigger_remote_features_for_connection(conn);
                    }

                    log_info("New connection: handle %u, %s", conn->con_handle, bd_addr_to_str(conn->address));
                    
                    hci_emit_nr_connections_changed();
                } else {
                    // connection failed
                    hci_handle_connection_failed(conn, packet[2]);
                }
            }
            break;

        case HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE:
            reverse_bd_addr(&packet[5], addr);
            conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_SCO);
            log_info("Synchronous Connection Complete for %p (status=%u) %s", conn, packet[2], bd_addr_to_str(addr));
            btstack_assert(conn != NULL);

            if (packet[2] != ERROR_CODE_SUCCESS){
                // connection failed, remove entry
                hci_handle_connection_failed(conn, packet[2]);
                break;
            }

            conn->state = OPEN;
            conn->con_handle = little_endian_read_16(packet, 3);            

#ifdef ENABLE_SCO_OVER_HCI
            // update SCO
            if (conn->address_type == BD_ADDR_TYPE_SCO && hci_stack->hci_transport && hci_stack->hci_transport->set_sco_config){
                hci_stack->hci_transport->set_sco_config(hci_stack->sco_voice_setting_active, hci_number_sco_connections());
            }
            // trigger can send now
            if (hci_have_usb_transport()){
                hci_stack->sco_can_send_now = true;
            }
#endif
#ifdef HAVE_SCO_TRANSPORT
            // configure sco transport
            if (hci_stack->sco_transport != NULL){
                sco_format_t sco_format = ((hci_stack->sco_voice_setting_active & 0x03) == 0x03) ? SCO_FORMAT_8_BIT : SCO_FORMAT_16_BIT;
                hci_stack->sco_transport->open(conn->con_handle, sco_format);
            }
#endif
            break;

        case HCI_EVENT_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE:
            handle = little_endian_read_16(packet, 3);
            conn = hci_connection_for_handle(handle);
            if (!conn) break;
            if (!packet[2]){
                const uint8_t * features = &packet[5];
                hci_handle_remote_features_page_0(conn, features);

                // read extended features if possible
                if (hci_command_supported(SUPPORTED_HCI_COMMAND_READ_REMOTE_EXTENDED_FEATURES)
                && ((conn->remote_supported_features[0] & 2) != 0)) {
                    conn->bonding_flags |= BONDING_REQUEST_REMOTE_FEATURES_PAGE_1;
                    break;
                }
            }
            hci_handle_remote_features_received(conn);
            break;

        case HCI_EVENT_READ_REMOTE_EXTENDED_FEATURES_COMPLETE:
            handle = little_endian_read_16(packet, 3);
            conn = hci_connection_for_handle(handle);
            if (!conn) break;
            // status = ok, page = 1
            if (!packet[2]) {
                uint8_t page_number = packet[5];
                uint8_t maximum_page_number = packet[6];
                const uint8_t * features = &packet[7];
                bool done = false;
                switch (page_number){
                    case 1:
                        hci_handle_remote_features_page_1(conn, features);
                        if (maximum_page_number >= 2){
                            // get Secure Connections (Controller) from Page 2 if available
                            conn->bonding_flags |= BONDING_REQUEST_REMOTE_FEATURES_PAGE_2;
                        } else {
                            // otherwise, assume SC (Controller) == SC (Host)
                            if ((conn->bonding_flags & BONDING_REMOTE_SUPPORTS_SC_HOST) != 0){
                                conn->bonding_flags |= BONDING_REMOTE_SUPPORTS_SC_CONTROLLER;
                            }
                            done = true;
                        }
                        break;
                    case 2:
                        hci_handle_remote_features_page_2(conn, features);
                        done = true;
                        break;
                    default:
                        break;
                }
                if (!done) break;
            }
            hci_handle_remote_features_received(conn);
            break;

        case HCI_EVENT_LINK_KEY_REQUEST:
#ifndef ENABLE_EXPLICIT_LINK_KEY_REPLY
            hci_event_link_key_request_get_bd_addr(packet, addr);
            conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
            if (!conn) break;

            // lookup link key in db if not cached
            if ((conn->link_key_type == INVALID_LINK_KEY) && (hci_stack->link_key_db != NULL)){
                hci_stack->link_key_db->get_link_key(conn->address, conn->link_key, &conn->link_key_type);
            }

            // response sent by hci_run()
            conn->authentication_flags |= AUTH_FLAG_HANDLE_LINK_KEY_REQUEST;
#endif
            break;
            
        case HCI_EVENT_LINK_KEY_NOTIFICATION: {
            hci_event_link_key_request_get_bd_addr(packet, addr);
            conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
            if (!conn) break;

            hci_pairing_complete(conn, ERROR_CODE_SUCCESS);

            // CVE-2020-26555: ignore NULL link key
            // default link_key_type = INVALID_LINK_KEY asserts that NULL key won't be used for encryption
            if (btstack_is_null(&packet[8], 16)) break;

            link_key_type_t link_key_type = (link_key_type_t)packet[24];
            // Change Connection Encryption keeps link key type
            if (link_key_type != CHANGED_COMBINATION_KEY){
                conn->link_key_type = link_key_type;
            }

            // cache link key. link keys stored in little-endian format for legacy reasons
            memcpy(&conn->link_key, &packet[8], 16);

            // only store link key:
            // - if bondable enabled
            if (hci_stack->bondable == false) break;
            // - if security level sufficient
            if (gap_security_level_for_link_key_type(link_key_type) < conn->requested_security_level) break;
            // - for SSP, also check if remote side requested bonding as well
            if (conn->link_key_type != COMBINATION_KEY){
                bool remote_bonding = conn->io_cap_response_auth_req >= SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_DEDICATED_BONDING;
                if (!remote_bonding){
                    break;
                }
            }
            gap_store_link_key_for_bd_addr(addr, &packet[8], conn->link_key_type);
            break;
        }

        case HCI_EVENT_PIN_CODE_REQUEST:
            hci_event_pin_code_request_get_bd_addr(packet, addr);
            conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
            if (!conn) break;

            hci_pairing_started(conn, false);
            // abort pairing if: non-bondable mode (pin code request is not forwarded to app)
            if (!hci_stack->bondable ){
                conn->authentication_flags |= AUTH_FLAG_DENY_PIN_CODE_REQUEST;
                hci_pairing_complete(conn, ERROR_CODE_PAIRING_NOT_ALLOWED);
                hci_run();
                return;
            }
            // abort pairing if: LEVEL_4 required (pin code request is not forwarded to app)
            if ((hci_stack->gap_secure_connections_only_mode) || (conn->requested_security_level == LEVEL_4)){
                log_info("Level 4 required, but SC not supported -> abort");
                conn->authentication_flags |= AUTH_FLAG_DENY_PIN_CODE_REQUEST;
                hci_pairing_complete(conn, ERROR_CODE_INSUFFICIENT_SECURITY);
                hci_run();
                return;
            }
            break;

        case HCI_EVENT_IO_CAPABILITY_RESPONSE:
            hci_event_io_capability_response_get_bd_addr(packet, addr);
            conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
            if (!conn) break;

            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], AUTH_FLAG_RECV_IO_CAPABILITIES_RESPONSE);
            hci_pairing_started(conn, true);
            conn->io_cap_response_auth_req = hci_event_io_capability_response_get_authentication_requirements(packet);
            conn->io_cap_response_io       = hci_event_io_capability_response_get_io_capability(packet);
#ifdef ENABLE_CLASSIC_PAIRING_OOB
            conn->io_cap_response_oob_data = hci_event_io_capability_response_get_oob_data_present(packet);
#endif
            break;

        case HCI_EVENT_IO_CAPABILITY_REQUEST:
            hci_event_io_capability_response_get_bd_addr(packet, addr);
            conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
            if (!conn) break;

            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], AUTH_FLAG_RECV_IO_CAPABILITIES_REQUEST);
            hci_connection_timestamp(conn);
            hci_pairing_started(conn, true);
            break;

#ifdef ENABLE_CLASSIC_PAIRING_OOB
        case HCI_EVENT_REMOTE_OOB_DATA_REQUEST:
            hci_event_remote_oob_data_request_get_bd_addr(packet, addr);
            conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
            if (!conn) break;

            hci_connection_timestamp(conn);

            hci_pairing_started(conn, true);

            connectionSetAuthenticationFlags(conn, AUTH_FLAG_SEND_REMOTE_OOB_DATA_REPLY);
            break;
#endif

        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
            hci_event_user_confirmation_request_get_bd_addr(packet, addr);
            conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
            if (!conn) break;
            if (hci_ssp_security_level_possible_for_io_cap(conn->requested_security_level, hci_stack->ssp_io_capability, conn->io_cap_response_io)) {
                if (hci_stack->ssp_auto_accept){
                    hci_add_connection_flags_for_flipped_bd_addr(&packet[2], AUTH_FLAG_SEND_USER_CONFIRM_REPLY);
                };
            } else {
                hci_pairing_complete(conn, ERROR_CODE_INSUFFICIENT_SECURITY);
                hci_add_connection_flags_for_flipped_bd_addr(&packet[2], AUTH_FLAG_SEND_USER_CONFIRM_NEGATIVE_REPLY);
                // don't forward event to app
                hci_run();
                return;
            }
            break;

        case HCI_EVENT_USER_PASSKEY_REQUEST:
            // Pairing using Passkey results in MITM protection. If Level 4 is required, support for SC is validated on IO Cap Request
            if (hci_stack->ssp_auto_accept){
                hci_add_connection_flags_for_flipped_bd_addr(&packet[2], AUTH_FLAG_SEND_USER_PASSKEY_REPLY);
            };
            break;

        case HCI_EVENT_MODE_CHANGE:
            handle = hci_event_mode_change_get_handle(packet);
            conn = hci_connection_for_handle(handle);
            if (!conn) break;
            conn->connection_mode = hci_event_mode_change_get_mode(packet);
            log_info("HCI_EVENT_MODE_CHANGE, handle 0x%04x, mode %u", handle, conn->connection_mode);
            break;
#endif

        case HCI_EVENT_ENCRYPTION_CHANGE:
        case HCI_EVENT_ENCRYPTION_CHANGE_V2:
            handle = hci_event_encryption_change_get_connection_handle(packet);
            conn = hci_connection_for_handle(handle);
            if (!conn) break;
            if (hci_event_encryption_change_get_status(packet) == ERROR_CODE_SUCCESS) {
                uint8_t encryption_enabled = hci_event_encryption_change_get_encryption_enabled(packet);
                if (encryption_enabled){
                    if (hci_is_le_connection(conn)){
                        // For LE, we accept connection as encrypted
                        conn->authentication_flags |= AUTH_FLAG_CONNECTION_ENCRYPTED;
                    }
#ifdef ENABLE_CLASSIC
                    else {

                        // Detect Secure Connection -> Legacy Connection Downgrade Attack (BIAS)
                        bool sc_used_during_pairing = gap_secure_connection_for_link_key_type(conn->link_key_type);
                        bool connected_uses_aes_ccm = encryption_enabled == 2;
                        if (hci_stack->secure_connections_active && sc_used_during_pairing && !connected_uses_aes_ccm){
                            log_info("SC during pairing, but only E0 now -> abort");
                            conn->bonding_flags |= BONDING_DISCONNECT_SECURITY_BLOCK;
                            break;
                        }

                        // if AES-CCM is used, authentication used SC -> authentication was mutual and we can skip explicit authentication
                        if (connected_uses_aes_ccm){
                            conn->authentication_flags |= AUTH_FLAG_CONNECTION_AUTHENTICATED;
                        }

#ifdef ENABLE_TESTING_SUPPORT
                        // work around for issue with PTS dongle
                        conn->authentication_flags |= AUTH_FLAG_CONNECTION_AUTHENTICATED;
#endif
                        // validate encryption key size
                        if (hci_event_packet_get_type(packet) == HCI_EVENT_ENCRYPTION_CHANGE_V2) {
                            uint8_t encryption_key_size = hci_event_encryption_change_v2_get_encryption_key_size(packet);
                            // already got encryption key size
                            hci_handle_read_encryption_key_size_complete(conn, encryption_key_size);
                        } else {
                            if (hci_command_supported(SUPPORTED_HCI_COMMAND_READ_ENCRYPTION_KEY_SIZE)) {
                                // For Classic, we need to validate encryption key size first, if possible (== supported by Controller)
                                conn->bonding_flags |= BONDING_SEND_READ_ENCRYPTION_KEY_SIZE;
                            } else {
                                // if not, pretend everything is perfect
                                hci_handle_read_encryption_key_size_complete(conn, 16);
                            }
                        }
                    }
#endif
                } else {
                    conn->authentication_flags &= ~AUTH_FLAG_CONNECTION_ENCRYPTED;
                }
            } else {
                uint8_t status = hci_event_encryption_change_get_status(packet);
                if ((conn->bonding_flags & BONDING_DEDICATED) != 0){
                    conn->bonding_flags &= ~BONDING_DEDICATED;
                    conn->bonding_flags |= BONDING_DISCONNECT_DEDICATED_DONE;
                    conn->bonding_status = status;
                }
            }

            break;

#ifdef ENABLE_CLASSIC
        case HCI_EVENT_AUTHENTICATION_COMPLETE_EVENT:
            handle = hci_event_authentication_complete_get_connection_handle(packet);
            conn = hci_connection_for_handle(handle);
            if (!conn) break;

            // clear authentication active flag
            conn->bonding_flags &= ~BONDING_SENT_AUTHENTICATE_REQUEST;
            hci_pairing_complete(conn, hci_event_authentication_complete_get_status(packet));

            // authenticated only if auth status == 0
            if (hci_event_authentication_complete_get_status(packet) == 0){
                // authenticated
                conn->authentication_flags |= AUTH_FLAG_CONNECTION_AUTHENTICATED;

                // If not already encrypted, start encryption
                if ((conn->authentication_flags & AUTH_FLAG_CONNECTION_ENCRYPTED) == 0){
                    conn->bonding_flags |= BONDING_SEND_ENCRYPTION_REQUEST;
                    break;
                }
            }

            // emit updated security level (will be 0 if not authenticated)
            hci_handle_mutual_authentication_completed(conn);
            break;

        case HCI_EVENT_SIMPLE_PAIRING_COMPLETE:
            hci_event_simple_pairing_complete_get_bd_addr(packet, addr);
            conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
            if (!conn) break;

            // treat successfully paired connection as authenticated
            if (hci_event_simple_pairing_complete_get_status(packet) == ERROR_CODE_SUCCESS){
                conn->authentication_flags |= AUTH_FLAG_CONNECTION_AUTHENTICATED;
            }

            hci_pairing_complete(conn, hci_event_simple_pairing_complete_get_status(packet));
            break;
#endif

        // HCI_EVENT_DISCONNECTION_COMPLETE
        // has been split, to first notify stack before shutting connection down
        // see end of function, too.
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if (packet[2]) break;   // status != 0
            handle = little_endian_read_16(packet, 3);
            // drop outgoing ACL fragments if it is for closed connection and release buffer if tx not active
            if (hci_stack->acl_fragmentation_total_size > 0u) {
                if (handle == READ_ACL_CONNECTION_HANDLE(hci_stack->hci_packet_buffer)){
                    int release_buffer = hci_stack->acl_fragmentation_tx_active == 0u;
                    log_info("drop fragmented ACL data for closed connection, release buffer %u", release_buffer);
                    hci_stack->acl_fragmentation_total_size = 0;
                    hci_stack->acl_fragmentation_pos = 0;
                    if (release_buffer){
                        hci_release_packet_buffer();
                    }
                }
            }

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
            // drop outgoing ISO fragments if it is for closed connection and release buffer if tx not active
            if (hci_stack->iso_fragmentation_total_size > 0u) {
                if (handle == READ_ISO_CONNECTION_HANDLE(hci_stack->hci_packet_buffer)){
                    int release_buffer = hci_stack->iso_fragmentation_tx_active == 0u;
                    log_info("drop fragmented ISO data for closed connection, release buffer %u", release_buffer);
                    hci_stack->iso_fragmentation_total_size = 0;
                    hci_stack->iso_fragmentation_pos = 0;
                    if (release_buffer){
                        hci_release_packet_buffer();
                    }
                }
            }

            // finalize iso stream for CIS handle
            iso_stream = hci_iso_stream_for_con_handle(handle);
            if (iso_stream != NULL){
                hci_iso_stream_finalize(iso_stream);
                break;
            }

            // finalize iso stream(s) for ACL handle
            btstack_linked_list_iterator_t it;
            btstack_linked_list_iterator_init(&it, &hci_stack->iso_streams);
            while (btstack_linked_list_iterator_has_next(&it)){
                hci_iso_stream_t * iso_stream = (hci_iso_stream_t *) btstack_linked_list_iterator_next(&it);
                if (iso_stream->acl_handle == handle ) {
                    hci_iso_stream_finalize(iso_stream);
                }
            }
#endif

            conn = hci_connection_for_handle(handle);
            if (!conn) break;
#ifdef ENABLE_CLASSIC
            // pairing failed if it was ongoing
            hci_pairing_complete(conn, ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION);
#endif

            // emit dedicatd bonding event
            if (conn->bonding_flags & BONDING_EMIT_COMPLETE_ON_DISCONNECT){
                hci_emit_dedicated_bonding_result(conn->address, conn->bonding_status);
            }

            // mark connection for shutdown, stop timers, reset state
            conn->state = RECEIVED_DISCONNECTION_COMPLETE;
            hci_connection_stop_timer(conn);
            hci_connection_init(conn);

#ifdef ENABLE_BLE
#ifdef ENABLE_LE_PERIPHERAL
            // re-enable advertisements for le connections if active
            if (hci_is_le_connection(conn)){
                hci_update_advertisements_enabled_for_current_roles();
            }
#endif
#endif
            break;

        case HCI_EVENT_HARDWARE_ERROR:
            log_error("Hardware Error: 0x%02x", packet[2]);
            if (hci_stack->hardware_error_callback){
                (*hci_stack->hardware_error_callback)(packet[2]);
            } else {
                // if no special requests, just reboot stack
                hci_power_control_off();
                hci_power_control_on();
            }
            break;

#ifdef ENABLE_CLASSIC
        case HCI_EVENT_ROLE_CHANGE:
            if (packet[2]) break;   // status != 0
            reverse_bd_addr(&packet[3], addr);
            addr_type = BD_ADDR_TYPE_ACL;
            conn = hci_connection_for_bd_addr_and_type(addr, addr_type);
            if (!conn) break;
            conn->role = (hci_role_t) packet[9];
            break;
#endif

        case HCI_EVENT_TRANSPORT_PACKET_SENT:
            // release packet buffer only for asynchronous transport and if there are not further fragments
            if (hci_transport_synchronous()) {
                log_error("Synchronous HCI Transport shouldn't send HCI_EVENT_TRANSPORT_PACKET_SENT");
                return; // instead of break: to avoid re-entering hci_run()
            }
            hci_stack->acl_fragmentation_tx_active = 0;
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
            hci_stack->iso_fragmentation_tx_active = 0;
            if (hci_stack->iso_fragmentation_total_size) break;
#endif
            if (hci_stack->acl_fragmentation_total_size) break;
            hci_release_packet_buffer();

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
            hci_iso_notify_can_send_now();
#endif
            // L2CAP receives this event via the hci_emit_event below

#ifdef ENABLE_CLASSIC
            // For SCO, we do the can_send_now_check here
            hci_notify_if_sco_can_send_now();
#endif
            break;

#ifdef ENABLE_CLASSIC
        case HCI_EVENT_SCO_CAN_SEND_NOW:
            // For SCO, we do the can_send_now_check here
            hci_stack->sco_can_send_now = true;
            hci_notify_if_sco_can_send_now();
            return;

        // explode inquriy results for easier consumption
        case HCI_EVENT_INQUIRY_RESULT:
        case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:
        case HCI_EVENT_EXTENDED_INQUIRY_RESPONSE:
            gap_inquiry_explode(packet, size);
            break;
#endif

#ifdef ENABLE_BLE
        case HCI_EVENT_LE_META:
            switch (packet[2]){
#ifdef ENABLE_LE_CENTRAL
                case HCI_SUBEVENT_LE_ADVERTISING_REPORT:
                    if (!hci_stack->le_scanning_enabled) break;
                    le_handle_advertisement_report(packet, size);
                    break;
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
                case HCI_SUBEVENT_LE_EXTENDED_ADVERTISING_REPORT:
                    if (!hci_stack->le_scanning_enabled) break;
                    le_handle_extended_advertisement_report(packet, size);
                    break;
                case HCI_SUBEVENT_LE_PERIODIC_ADVERTISING_SYNC_ESTABLISHMENT:
                    hci_stack->le_periodic_sync_request = LE_CONNECTING_IDLE;
                    hci_stack->le_periodic_sync_state = LE_CONNECTING_IDLE;
                    break;
#endif
#endif
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE_V1:
                case HCI_SUBEVENT_LE_ENHANCED_CONNECTION_COMPLETE_V2:
					event_handle_le_connection_complete(packet);
                    break;

                // log_info("LE buffer size: %u, count %u", little_endian_read_16(packet,6), packet[8]);
                case HCI_SUBEVENT_LE_CONNECTION_UPDATE_COMPLETE:
                    handle = hci_subevent_le_connection_update_complete_get_connection_handle(packet);
                    conn = hci_connection_for_handle(handle);
                    if (!conn) break;
                    conn->le_connection_interval = hci_subevent_le_connection_update_complete_get_conn_interval(packet);
                    break;

                case HCI_SUBEVENT_LE_REMOTE_CONNECTION_PARAMETER_REQUEST:
                    // connection
                    handle = hci_subevent_le_remote_connection_parameter_request_get_connection_handle(packet);
                    conn = hci_connection_for_handle(handle);
                    if (conn) {
                        // read arguments
                        uint16_t le_conn_interval_min   = hci_subevent_le_remote_connection_parameter_request_get_interval_min(packet);
                        uint16_t le_conn_interval_max   = hci_subevent_le_remote_connection_parameter_request_get_interval_max(packet);
                        uint16_t le_conn_latency        = hci_subevent_le_remote_connection_parameter_request_get_latency(packet);
                        uint16_t le_supervision_timeout = hci_subevent_le_remote_connection_parameter_request_get_timeout(packet);

                        // validate against current connection parameter range
                        le_connection_parameter_range_t existing_range;
                        gap_get_connection_parameter_range(&existing_range);
                        int update_parameter = gap_connection_parameter_range_included(&existing_range, le_conn_interval_min, le_conn_interval_max, le_conn_latency, le_supervision_timeout);
                        if (update_parameter){
                            conn->le_con_parameter_update_state = CON_PARAMETER_UPDATE_REPLY;
                            conn->le_conn_interval_min = le_conn_interval_min;
                            conn->le_conn_interval_max = le_conn_interval_max;
                            conn->le_conn_latency = le_conn_latency;
                            conn->le_supervision_timeout = le_supervision_timeout;
                        } else {
                            conn->le_con_parameter_update_state = CON_PARAMETER_UPDATE_NEGATIVE_REPLY;
                        }
                    }
                    break;
#ifdef ENABLE_LE_LIMIT_ACL_FRAGMENT_BY_MAX_OCTETS
                case HCI_SUBEVENT_LE_DATA_LENGTH_CHANGE:
                    handle = hci_subevent_le_data_length_change_get_connection_handle(packet);
                    conn = hci_connection_for_handle(handle);
                    if (conn) {
                        conn->le_max_tx_octets = hci_subevent_le_data_length_change_get_max_tx_octets(packet);
                    }
                    break;
#endif
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
                case HCI_SUBEVENT_LE_CIS_REQUEST:
                    // incoming CIS request, allocate iso stream object and cache metadata
                    iso_stream = hci_iso_stream_create(HCI_ISO_TYPE_CIS, HCI_ISO_STREAM_W4_USER,
                                                       hci_subevent_le_cis_request_get_cig_id(packet),
                                                       hci_subevent_le_cis_request_get_cis_id(packet));
                    // if there's no memory, gap_cis_accept/gap_cis_reject will fail
                    if (iso_stream != NULL){
                        iso_stream->cis_handle = hci_subevent_le_cis_request_get_cis_connection_handle(packet);
                        iso_stream->acl_handle = hci_subevent_le_cis_request_get_acl_connection_handle(packet);
                    }
                    break;
                case HCI_SUBEVENT_LE_CIS_ESTABLISHED:
                    if (hci_stack->iso_active_operation_type == HCI_ISO_TYPE_CIS){
                        handle = hci_subevent_le_cis_established_get_connection_handle(packet);
                        uint8_t status = hci_subevent_le_cis_established_get_status(packet);
                        iso_stream = hci_iso_stream_for_con_handle(handle);
                        btstack_assert(iso_stream != NULL);
                        // track SDU
                        iso_stream->max_sdu_c_to_p = hci_subevent_le_cis_established_get_max_pdu_c_to_p(packet);
                        iso_stream->max_sdu_p_to_c = hci_subevent_le_cis_established_get_max_pdu_p_to_c(packet);
                        if (hci_stack->iso_active_operation_group_id == HCI_ISO_GROUP_ID_SINGLE_CIS){
                            // CIS Accept by Peripheral
                            if (status == ERROR_CODE_SUCCESS){
                                if (iso_stream->max_sdu_p_to_c > 0){
                                    // we're peripheral and we will send data
                                    iso_stream->state = HCI_ISO_STREAM_STATE_W2_SETUP_ISO_INPUT;
                                } else {
                                    // we're peripheral and we will only receive data
                                    iso_stream->state = HCI_ISO_STREAM_STATE_W2_SETUP_ISO_OUTPUT;
                                }
                            } else {
                                hci_cis_handle_created(iso_stream, status);
                            }
                            hci_stack->iso_active_operation_type = HCI_ISO_TYPE_INVALID;
                        } else {
                            // CIG Setup by Central
                            le_audio_cig_t * cig = hci_cig_for_id(hci_stack->iso_active_operation_group_id);
                            btstack_assert(cig != NULL);
                            // update iso stream state
                            if (status == ERROR_CODE_SUCCESS){
                                iso_stream->state = HCI_ISO_STREAM_STATE_ESTABLISHED;
                            } else {
                                iso_stream->state = HCI_ISO_STREAM_STATE_IDLE;
                            }
                            // update cig state
                            uint8_t i;
                            for (i=0;i<cig->num_cis;i++){
                                if (cig->cis_con_handles[i] == handle){
                                    cig->cis_setup_active[i] = false;
                                    if (status == ERROR_CODE_SUCCESS){
                                        cig->cis_established[i] = true;
                                    } else {
                                        hci_cis_handle_created(iso_stream, status);
                                    }
                                }
                            }

                            // trigger iso path setup if complete
                            bool cis_setup_active = false;
                            for (i=0;i<cig->num_cis;i++){
                                cis_setup_active |= cig->cis_setup_active[i];
                            }
                            if (cis_setup_active == false){
                                cig->state_vars.next_cis = 0;
                                cig->state = LE_AUDIO_CIG_STATE_SETUP_ISO_PATH;
                                hci_stack->iso_active_operation_type = HCI_ISO_TYPE_INVALID;
                            }
                        }
                    }
                    break;
                case HCI_SUBEVENT_LE_CREATE_BIG_COMPLETE:
                    hci_stack->iso_active_operation_type = HCI_ISO_TYPE_INVALID;
                    big = hci_big_for_handle(packet[4]);
                    if (big != NULL){
                        uint8_t status = packet[3];
                        if (status == ERROR_CODE_SUCCESS){
                            // store bis_con_handles and trigger iso path setup
                            uint8_t num_bis = btstack_min(MAX_NR_BIS, packet[20]);
                            uint8_t i;
                            for (i=0;i<num_bis;i++){
                                hci_con_handle_t bis_handle = (hci_con_handle_t) little_endian_read_16(packet, 21 + (2 * i));
                                big->bis_con_handles[i] = bis_handle;
                                // assign bis handle
                                btstack_linked_list_iterator_t it;
                                btstack_linked_list_iterator_init(&it, &hci_stack->iso_streams);
                                while (btstack_linked_list_iterator_has_next(&it)){
                                    hci_iso_stream_t * iso_stream = (hci_iso_stream_t *) btstack_linked_list_iterator_next(&it);
                                    if ((iso_stream->state == HCI_ISO_STREAM_STATE_REQUESTED ) &&
                                        (iso_stream->group_id == big->big_handle)){
                                        iso_stream->cis_handle = bis_handle;
                                        iso_stream->state = HCI_ISO_STREAM_STATE_ESTABLISHED;
                                        break;
                                    }
                                }
                            }
                            if (big->state == LE_AUDIO_BIG_STATE_W4_ESTABLISHED) {
                                big->state = LE_AUDIO_BIG_STATE_SETUP_ISO_PATH;
                                big->state_vars.next_bis = 0;
                            }
                        } else {
                            // create BIG failed or has been stopped by us
                            hci_iso_stream_finalize_by_type_and_group_id(HCI_ISO_TYPE_BIS, big->big_handle);
                            btstack_linked_list_remove(&hci_stack->le_audio_bigs, (btstack_linked_item_t *) big);
                            if (big->state == LE_AUDIO_BIG_STATE_W4_ESTABLISHED){
                                hci_emit_big_created(big, status);
                            } else {
                                hci_emit_big_terminated(big);
                            }
                        }
                    }
                    break;
                case HCI_SUBEVENT_LE_TERMINATE_BIG_COMPLETE:
                    hci_stack->iso_active_operation_type = HCI_ISO_TYPE_INVALID;
                    big = hci_big_for_handle(hci_subevent_le_terminate_big_complete_get_big_handle(packet));
                    if (big != NULL){
                        // finalize associated ISO streams
                        btstack_linked_list_iterator_t it;
                        btstack_linked_list_iterator_init(&it, &hci_stack->iso_streams);
                        while (btstack_linked_list_iterator_has_next(&it)){
                            hci_iso_stream_t * iso_stream = (hci_iso_stream_t *) btstack_linked_list_iterator_next(&it);
                            if (iso_stream->group_id == big->big_handle){
                                log_info("BIG Terminated, big_handle 0x%02x, con handle 0x%04x", iso_stream->group_id, iso_stream->cis_handle);
                                btstack_linked_list_iterator_remove(&it);
                                btstack_memory_hci_iso_stream_free(iso_stream);
                            }
                        }
                        btstack_linked_list_remove(&hci_stack->le_audio_bigs, (btstack_linked_item_t *) big);
                        switch (big->state){
                            case LE_AUDIO_BIG_STATE_W4_TERMINATED_AFTER_SETUP_FAILED:
                                hci_emit_big_created(big, big->state_vars.status);
                                break;
                            default:
                                hci_emit_big_terminated(big);
                                break;
                        }
                    }
                    break;
                case HCI_SUBEVENT_LE_BIG_SYNC_ESTABLISHED:
                    hci_stack->iso_active_operation_type = HCI_ISO_TYPE_INVALID;
                    big_sync = hci_big_sync_for_handle(packet[4]);
                    if (big_sync != NULL){
                        uint8_t status = packet[3];
                        uint8_t big_handle = packet[4];
                        if (status == ERROR_CODE_SUCCESS){
                            // store bis_con_handles and trigger iso path setup
                            uint8_t num_bis = btstack_min(MAX_NR_BIS, packet[16]);
                            uint8_t i;
                            for (i=0;i<num_bis;i++){
                                big_sync->bis_con_handles[i] = little_endian_read_16(packet, 17 + (2 * i));
                            }
                            if (big_sync->state == LE_AUDIO_BIG_STATE_W4_ESTABLISHED) {
                                // trigger iso path setup
                                big_sync->state = LE_AUDIO_BIG_STATE_SETUP_ISO_PATH;
                                big_sync->state_vars.next_bis = 0;
                            }
                        } else {
                            // create BIG Sync failed or has been stopped by us
                            btstack_linked_list_remove(&hci_stack->le_audio_big_syncs, (btstack_linked_item_t *) big_sync);
                            if (big_sync->state == LE_AUDIO_BIG_STATE_W4_ESTABLISHED) {
                                hci_emit_big_sync_created(big_sync, status);
                            } else {
                                hci_emit_big_sync_stopped(big_handle);
                            }
                        }
                    }
                    break;
                case HCI_SUBEVENT_LE_BIG_SYNC_LOST:
                    hci_stack->iso_active_operation_type = HCI_ISO_TYPE_INVALID;
                    big_sync = hci_big_sync_for_handle(packet[4]);
                    if (big_sync != NULL){
                        uint8_t big_handle = packet[4];
                        btstack_linked_list_remove(&hci_stack->le_audio_big_syncs, (btstack_linked_item_t *) big_sync);
                        hci_emit_big_sync_stopped(big_handle);
                    }
                    break;
#endif
                default:
                    break;
            }
            break;
#endif
        case HCI_EVENT_VENDOR_SPECIFIC:
            // Vendor specific commands often create vendor specific event instead of num completed packets
            // To avoid getting stuck as num_cmds_packets is zero, reset it to 1 for controllers with this behaviour
            switch (hci_stack->manufacturer){
                case BLUETOOTH_COMPANY_ID_CAMBRIDGE_SILICON_RADIO:
                    hci_stack->num_cmd_packets = 1;
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }

    handle_event_for_current_stack_state(packet, size);

    // notify upper stack
	hci_emit_event(packet, size, 0);   // don't dump, already happened in packet handler

    // moved here to give upper stack a chance to close down everything with hci_connection_t intact
    if ((hci_event_packet_get_type(packet) == HCI_EVENT_DISCONNECTION_COMPLETE) && (packet[2] == 0)){
		handle = little_endian_read_16(packet, 3);
		hci_connection_t * aConn = hci_connection_for_handle(handle);
		// discard connection if app did not trigger a reconnect in the event handler
		if (aConn && aConn->state == RECEIVED_DISCONNECTION_COMPLETE){
			hci_shutdown_connection(aConn);
		}
#ifdef ENABLE_CONTROLLER_DUMP_PACKETS
        hci_controller_dump_packets();
#endif
    }

	// execute main loop
	hci_run();
}

#ifdef ENABLE_CLASSIC

#ifdef ENABLE_SCO_OVER_HCI
static void sco_tx_timeout_handler(btstack_timer_source_t * ts);
static void sco_schedule_tx(hci_connection_t * conn);

static void sco_tx_timeout_handler(btstack_timer_source_t * ts){
    log_debug("SCO TX Timeout");
    hci_con_handle_t con_handle = (hci_con_handle_t) (uintptr_t) btstack_run_loop_get_timer_context(ts);
    hci_connection_t * conn = hci_connection_for_handle(con_handle);
    if (!conn) return;

    // trigger send
    conn->sco_tx_ready = 1;
    // extra packet if CVSD but SCO buffer is too short
    if (((hci_stack->sco_voice_setting_active & 0x03) != 0x03) && (hci_stack->sco_data_packet_length < 123)){
        conn->sco_tx_ready++;
    }
    hci_notify_if_sco_can_send_now();
}


#define SCO_TX_AFTER_RX_MS (6)

static void sco_schedule_tx(hci_connection_t * conn){

    uint32_t now = btstack_run_loop_get_time_ms();
    uint32_t sco_tx_ms = conn->sco_rx_ms + SCO_TX_AFTER_RX_MS;
    int time_delta_ms = sco_tx_ms - now;

    btstack_timer_source_t * timer = (conn->sco_rx_count & 1) ? &conn->timeout : &conn->timeout_sco;

    // log_error("SCO TX at %u in %u", (int) sco_tx_ms, time_delta_ms);
    btstack_run_loop_remove_timer(timer);
    btstack_run_loop_set_timer(timer, time_delta_ms);
    btstack_run_loop_set_timer_context(timer, (void *) (uintptr_t) conn->con_handle);
    btstack_run_loop_set_timer_handler(timer, &sco_tx_timeout_handler);
    btstack_run_loop_add_timer(timer);
}
#endif

static void sco_handler(uint8_t * packet, uint16_t size){
    // lookup connection struct
    hci_con_handle_t con_handle = READ_SCO_CONNECTION_HANDLE(packet);
    hci_connection_t * conn     = hci_connection_for_handle(con_handle);
    if (!conn) return;

#ifdef ENABLE_SCO_OVER_HCI
    // CSR 8811 prefixes 60 byte SCO packet in transparent mode with 20 zero bytes -> skip first 20 payload bytes
    if (hci_stack->manufacturer == BLUETOOTH_COMPANY_ID_CAMBRIDGE_SILICON_RADIO){
        if ((size == 83) && ((hci_stack->sco_voice_setting_active & 0x03) == 0x03)){
            packet[2] = 0x3c;
            memmove(&packet[3], &packet[23], 63);
            size = 63;
        }
    }

    if (hci_have_usb_transport()){
        // Nothing to do
    } else {
        // log_debug("sco flow %u, handle 0x%04x, packets sent %u, bytes send %u", hci_stack->synchronous_flow_control_enabled, (int) con_handle, conn->num_packets_sent, conn->num_sco_bytes_sent);
        if (hci_stack->synchronous_flow_control_enabled == 0){
            uint32_t now = btstack_run_loop_get_time_ms();

            if (!conn->sco_rx_valid){
                // ignore first 10 packets
                conn->sco_rx_count++;
                // log_debug("sco rx count %u", conn->sco_rx_count);
                if (conn->sco_rx_count == 10) {
                    // use first timestamp as is and pretent it just started
                    conn->sco_rx_ms = now;
                    conn->sco_rx_valid = 1;
                    conn->sco_rx_count = 0;
                    sco_schedule_tx(conn);
                }
            } else {
                // track expected arrival timme
                conn->sco_rx_count++;
                conn->sco_rx_ms += 7;
                int delta = (int32_t) (now - conn->sco_rx_ms);
                if (delta > 0){
                    conn->sco_rx_ms++;
                }
                // log_debug("sco rx %u", conn->sco_rx_ms);
                sco_schedule_tx(conn);
            }
        }
    }
#endif

    // deliver to app
    if (hci_stack->sco_packet_handler) {
        hci_stack->sco_packet_handler(HCI_SCO_DATA_PACKET, 0, packet, size);
    }

#ifdef HAVE_SCO_TRANSPORT
    // We can send one packet for each received packet
    conn->sco_tx_ready++;
    hci_notify_if_sco_can_send_now();
#endif

#ifdef ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
    conn->num_packets_completed++;
    hci_stack->host_completed_packets = 1;
    hci_run();
#endif    
}
#endif

static void packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
    hci_dump_packet(packet_type, 1, packet, size);
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            event_handler(packet, size);
            break;
        case HCI_ACL_DATA_PACKET:
            acl_handler(packet, size);
            break;
#ifdef ENABLE_CLASSIC
        case HCI_SCO_DATA_PACKET:
            sco_handler(packet, size);
            break;
#endif
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
        case HCI_ISO_DATA_PACKET:
            hci_iso_packet_handler(packet, size);
            break;
#endif
        default:
            break;
    }
}

/**
 * @brief Add event packet handler. 
 */
void hci_add_event_handler(btstack_packet_callback_registration_t * callback_handler){
    btstack_linked_list_add_tail(&hci_stack->event_handlers, (btstack_linked_item_t*) callback_handler);
}

/**
 * @brief Remove event packet handler.
 */
void hci_remove_event_handler(btstack_packet_callback_registration_t * callback_handler){
    btstack_linked_list_remove(&hci_stack->event_handlers, (btstack_linked_item_t*) callback_handler);
}

/** Register HCI packet handlers */
void hci_register_acl_packet_handler(btstack_packet_handler_t handler){
    hci_stack->acl_packet_handler = handler;
}

#ifdef ENABLE_CLASSIC
/**
 * @brief Registers a packet handler for SCO data. Used for HSP and HFP profiles.
 */
void hci_register_sco_packet_handler(btstack_packet_handler_t handler){
    hci_stack->sco_packet_handler = handler;    
}
#endif

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
void hci_register_iso_packet_handler(btstack_packet_handler_t handler){
    hci_stack->iso_packet_handler = handler;
}
#endif

static void hci_state_reset(void){
    // no connections yet
    hci_stack->connections = NULL;

    // keep discoverable/connectable as this has been requested by the client(s)
    // hci_stack->discoverable = 0;
    // hci_stack->connectable = 0;
    // hci_stack->bondable = 1;
    // hci_stack->own_addr_type = 0;

    // buffer is free
    hci_stack->hci_packet_buffer_reserved = false;

    // no pending cmds
    hci_stack->decline_reason = 0;

    hci_stack->secure_connections_active = false;

#ifdef ENABLE_CLASSIC
    hci_stack->inquiry_lap = GAP_IAC_GENERAL_INQUIRY;
    hci_stack->page_timeout = 0x6000;  // ca. 15 sec

    hci_stack->gap_tasks_classic =
            GAP_TASK_SET_DEFAULT_LINK_POLICY |
            GAP_TASK_SET_CLASS_OF_DEVICE |
            GAP_TASK_SET_LOCAL_NAME |
            GAP_TASK_SET_EIR_DATA |
            GAP_TASK_WRITE_SCAN_ENABLE |
            GAP_TASK_WRITE_PAGE_TIMEOUT;
#endif

#ifdef ENABLE_CLASSIC_PAIRING_OOB
    hci_stack->classic_read_local_oob_data = false;
    hci_stack->classic_oob_con_handle = HCI_CON_HANDLE_INVALID;
#endif

    // LE
#ifdef ENABLE_BLE
    memset(hci_stack->le_random_address, 0, 6);
    hci_stack->le_random_address_set = 0;
#endif
#ifdef ENABLE_LE_CENTRAL
    hci_stack->le_scanning_active  = false;
    hci_stack->le_scanning_param_update = true;
    hci_stack->le_connecting_state = LE_CONNECTING_IDLE;
    hci_stack->le_connecting_request = LE_CONNECTING_IDLE;
    hci_stack->le_whitelist_capacity = 0;
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    hci_stack->le_periodic_terminate_sync_handle = HCI_CON_HANDLE_INVALID;
#endif
#endif
#ifdef ENABLE_LE_PERIPHERAL
    hci_stack->le_advertisements_state &= ~LE_ADVERTISEMENT_STATE_ACTIVE;
    if ((hci_stack->le_advertisements_state & LE_ADVERTISEMENT_STATE_PARAMS_SET) != 0){
        hci_stack->le_advertisements_todo |= LE_ADVERTISEMENT_TASKS_SET_PARAMS;
    }
    if (hci_stack->le_advertisements_data != NULL){
        hci_stack->le_advertisements_todo |= LE_ADVERTISEMENT_TASKS_SET_ADV_DATA;
    }
#endif
#ifdef ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION
    hci_stack->le_resolving_list_state = LE_RESOLVING_LIST_SEND_ENABLE_ADDRESS_RESOLUTION;
#endif
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
    hci_stack->iso_active_operation_type = HCI_ISO_TYPE_INVALID;
    hci_stack->iso_active_operation_group_id = HCI_ISO_GROUP_ID_INVALID;
#endif
}

#ifdef ENABLE_CLASSIC
/**
 * @brief Configure Bluetooth hardware control. Has to be called before power on.
 */
void hci_set_link_key_db(btstack_link_key_db_t const * link_key_db){
    // store and open remote device db
    hci_stack->link_key_db = link_key_db;
    if (hci_stack->link_key_db) {
        hci_stack->link_key_db->open();
    }
}
#endif

void hci_init(const hci_transport_t *transport, const void *config){
    
#ifdef HAVE_MALLOC
    if (!hci_stack) {
        hci_stack = (hci_stack_t*) malloc(sizeof(hci_stack_t));
    }
#else
    hci_stack = &hci_stack_static;
#endif
    memset(hci_stack, 0, sizeof(hci_stack_t));

    // reference to use transport layer implementation
    hci_stack->hci_transport = transport;
        
    // reference to used config
    hci_stack->config = config;
    
    // setup pointer for outgoing packet buffer
    hci_stack->hci_packet_buffer = &hci_stack->hci_packet_buffer_data[HCI_OUTGOING_PRE_BUFFER_SIZE];

    // max acl payload size defined in config.h
    hci_stack->acl_data_packet_length = HCI_ACL_PAYLOAD_SIZE;
    
    // register packet handlers with transport
    transport->register_packet_handler(&packet_handler);

    hci_stack->state = HCI_STATE_OFF;

    // class of device
    hci_stack->class_of_device = 0x007a020c; // Smartphone 

    // bondable by default
    hci_stack->bondable = 1;

#ifdef ENABLE_CLASSIC
    // classic name
    hci_stack->local_name = default_classic_name;

    // Master slave policy
    hci_stack->master_slave_policy = 1;

    // Allow Role Switch
    hci_stack->allow_role_switch = 1;

    // Default / minimum security level = 2
    hci_stack->gap_security_level = LEVEL_2;

    // Default Security Mode 4
    hci_stack->gap_security_mode = GAP_SECURITY_MODE_4;

    // Errata-11838 mandates 7 bytes for GAP Security Level 1-3
    hci_stack->gap_required_encyrption_key_size = 7;

    // Link Supervision Timeout
    hci_stack->link_supervision_timeout = HCI_LINK_SUPERVISION_TIMEOUT_DEFAULT;

#endif

    // Secure Simple Pairing default: enable, no I/O capabilities, general bonding, mitm not required, auto accept 
    hci_stack->ssp_enable = 1;
    hci_stack->ssp_io_capability = SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT;
    hci_stack->ssp_authentication_requirement = SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_GENERAL_BONDING;
    hci_stack->ssp_auto_accept = 1;

    // Secure Connections: enable (requires support from Controller)
    hci_stack->secure_connections_enable = true;

    // voice setting - signed 16 bit pcm data with CVSD over the air
    hci_stack->sco_voice_setting = 0x60;

#ifdef ENABLE_BLE
    hci_stack->le_connection_scan_interval = 0x0060;   //    60 ms
    hci_stack->le_connection_scan_window   = 0x0030;    //   30 ms
    hci_stack->le_connection_interval_min  = 0x0008;    //   10 ms
    hci_stack->le_connection_interval_max  = 0x0018;    //   30 ms
    hci_stack->le_connection_latency       =      4;    //    4
    hci_stack->le_supervision_timeout      = 0x0048;    //  720 ms
    hci_stack->le_minimum_ce_length        =      0;    //    0 ms
    hci_stack->le_maximum_ce_length        =      0;    //    0 ms
#endif

#ifdef ENABLE_LE_CENTRAL
    hci_stack->le_connection_phys          =   0x01;    // LE 1M PHY

    // default LE Scanning
    hci_stack->le_scan_type     =  0x01; // active
    hci_stack->le_scan_interval = 0x1e0; // 300 ms
    hci_stack->le_scan_window   =  0x30; //  30 ms
    hci_stack->le_scan_phys     =  0x01; // LE 1M PHY
#endif

#ifdef ENABLE_LE_PERIPHERAL
    hci_stack->le_max_number_peripheral_connections = 1; // only single connection as peripheral

    // default advertising parameters from Core v5.4 -- needed to use random address without prior adv setup
    hci_stack->le_advertisements_interval_min =                         0x0800;
    hci_stack->le_advertisements_interval_max =                         0x0800;
    hci_stack->le_advertisements_type =                                      0;
    hci_stack->le_own_addr_type =                       BD_ADDR_TYPE_LE_PUBLIC;
    hci_stack->le_advertisements_direct_address_type =  BD_ADDR_TYPE_LE_PUBLIC;
    hci_stack->le_advertisements_channel_map =                            0x07;
    hci_stack->le_advertisements_filter_policy =                             0;
#endif

    // connection parameter range used to answer connection parameter update requests in l2cap
    hci_stack->le_connection_parameter_range.le_conn_interval_min =          6; 
    hci_stack->le_connection_parameter_range.le_conn_interval_max =       3200;
    hci_stack->le_connection_parameter_range.le_conn_latency_min =           0;
    hci_stack->le_connection_parameter_range.le_conn_latency_max =         500;
    hci_stack->le_connection_parameter_range.le_supervision_timeout_min =   10;
    hci_stack->le_connection_parameter_range.le_supervision_timeout_max = 3200;

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
    hci_stack->iso_packets_to_queue = 1;
#endif

#ifdef ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION
    hci_stack->le_privacy_mode = LE_PRIVACY_MODE_DEVICE;
#endif

    hci_state_reset();
}

void hci_deinit(void){
    btstack_run_loop_remove_timer(&hci_stack->timeout);
#ifdef HAVE_MALLOC
    if (hci_stack) {
        free(hci_stack);
    }
#endif
    hci_stack = NULL;

#ifdef ENABLE_CLASSIC
    disable_l2cap_timeouts = 0;
#endif
}

/**
 * @brief Configure Bluetooth chipset driver. Has to be called before power on, or right after receiving the local version information
 */
void hci_set_chipset(const btstack_chipset_t *chipset_driver){
    hci_stack->chipset = chipset_driver;

    // reset chipset driver - init is also called on power_up
    if (hci_stack->chipset && hci_stack->chipset->init){
        hci_stack->chipset->init(hci_stack->config);
    }
}

void hci_enable_custom_pre_init(void){
    hci_stack->chipset_pre_init = true;
}

/**
 * @brief Configure Bluetooth hardware control. Has to be called after hci_init() but before power on.
 */
void hci_set_control(const btstack_control_t *hardware_control){
    // references to used control implementation
    hci_stack->control = hardware_control;
    // init with transport config
    hardware_control->init(hci_stack->config);
}

static void hci_discard_connections(void){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        // cancel all l2cap connections by emitting dicsconnection complete before shutdown (free) connection
        hci_connection_t * connection = (hci_connection_t*) btstack_linked_list_iterator_next(&it);
        hci_emit_disconnection_complete(connection->con_handle, 0x16); // terminated by local host
        hci_shutdown_connection(connection);
    }
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
    while (hci_stack->iso_streams != NULL){
        hci_iso_stream_finalize((hci_iso_stream_t *) hci_stack->iso_streams);
    }
#endif
}

void hci_close(void){

#ifdef ENABLE_CLASSIC
    // close remote device db
    if (hci_stack->link_key_db) {
        hci_stack->link_key_db->close();
    }
#endif

    hci_discard_connections();

    hci_power_control(HCI_POWER_OFF);
    
#ifdef HAVE_MALLOC
    free(hci_stack);
#endif
    hci_stack = NULL;
}

#ifdef HAVE_SCO_TRANSPORT
void hci_set_sco_transport(const btstack_sco_transport_t *sco_transport){
    hci_stack->sco_transport = sco_transport;
    sco_transport->register_packet_handler(&packet_handler);
}
#endif

#ifdef ENABLE_CLASSIC
void gap_set_required_encryption_key_size(uint8_t encryption_key_size){
    // validate ranage and set
    if (encryption_key_size < 7)  return;
    if (encryption_key_size > 16) return;
    hci_stack->gap_required_encyrption_key_size = encryption_key_size;
}

uint8_t gap_set_security_mode(gap_security_mode_t security_mode){
    if ((security_mode == GAP_SECURITY_MODE_4) || (security_mode == GAP_SECURITY_MODE_2)){
        hci_stack->gap_security_mode = security_mode;
        return ERROR_CODE_SUCCESS;
    } else {
        return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
    }
}

gap_security_mode_t gap_get_security_mode(void){
    return hci_stack->gap_security_mode;
}

void gap_set_security_level(gap_security_level_t security_level){
    hci_stack->gap_security_level = security_level;
}

gap_security_level_t gap_get_security_level(void){
    if (hci_stack->gap_secure_connections_only_mode){
        return LEVEL_4;
    }
    return hci_stack->gap_security_level;
}

void gap_set_minimal_service_security_level(gap_security_level_t security_level){
    hci_stack->gap_minimal_service_security_level = security_level;
}

void gap_set_secure_connections_only_mode(bool enable){
    hci_stack->gap_secure_connections_only_mode = enable;
}

bool gap_get_secure_connections_only_mode(void){
    return hci_stack->gap_secure_connections_only_mode;
}
#endif

#ifdef ENABLE_CLASSIC
void gap_set_class_of_device(uint32_t class_of_device){
    hci_stack->class_of_device = class_of_device;
    hci_stack->gap_tasks_classic |= GAP_TASK_SET_CLASS_OF_DEVICE;
    hci_run();
}

void gap_set_default_link_policy_settings(uint16_t default_link_policy_settings){
    hci_stack->default_link_policy_settings = default_link_policy_settings;
    hci_stack->gap_tasks_classic |= GAP_TASK_SET_DEFAULT_LINK_POLICY;
    hci_run();
}

void gap_set_allow_role_switch(bool allow_role_switch){
    hci_stack->allow_role_switch = allow_role_switch ? 1 : 0;
}

uint8_t hci_get_allow_role_switch(void){
    return  hci_stack->allow_role_switch;
}

void gap_set_link_supervision_timeout(uint16_t link_supervision_timeout){
    hci_stack->link_supervision_timeout = link_supervision_timeout;
}

void gap_enable_link_watchdog(uint16_t timeout_ms){
    hci_stack->automatic_flush_timeout = btstack_min(timeout_ms, 1280) * 8 / 5; // divide by 0.625
}

uint16_t hci_automatic_flush_timeout(void){
    return hci_stack->automatic_flush_timeout;
}

void hci_disable_l2cap_timeout_check(void){
    disable_l2cap_timeouts = 1;
}
#endif

#ifndef HAVE_HOST_CONTROLLER_API
// Set Public BD ADDR - passed on to Bluetooth chipset if supported in bt_control_h
void hci_set_bd_addr(bd_addr_t addr){
    (void)memcpy(hci_stack->custom_bd_addr, addr, 6);
    hci_stack->custom_bd_addr_set = 1;
}
#endif

// State-Module-Driver overview
// state                    module  low-level 
// HCI_STATE_OFF             off      close
// HCI_STATE_INITIALIZING,   on       open
// HCI_STATE_WORKING,        on       open
// HCI_STATE_HALTING,        on       open
// HCI_STATE_SLEEPING,    off/sleep   close
// HCI_STATE_FALLING_ASLEEP  on       open

static int hci_power_control_on(void){
    
    // power on
    int err = 0;
    if (hci_stack->control && hci_stack->control->on){
        err = (*hci_stack->control->on)();
    }
    if (err){
        log_error( "POWER_ON failed");
        hci_emit_hci_open_failed();
        return err;
    }
    
    // int chipset driver
    if (hci_stack->chipset && hci_stack->chipset->init){
        hci_stack->chipset->init(hci_stack->config);
    }

    // init transport
    if (hci_stack->hci_transport->init){
        hci_stack->hci_transport->init(hci_stack->config);
    }

    // open transport
    err = hci_stack->hci_transport->open();
    if (err){
        log_error( "HCI_INIT failed, turning Bluetooth off again");
        if (hci_stack->control && hci_stack->control->off){
            (*hci_stack->control->off)();
        }
        hci_emit_hci_open_failed();
        return err;
    }
    return 0;
}

static void hci_power_control_off(void){
    
    log_info("hci_power_control_off");

    // close low-level device
    hci_stack->hci_transport->close();

    log_info("hci_power_control_off - hci_transport closed");
    
    // power off
    if (hci_stack->control && hci_stack->control->off){
        (*hci_stack->control->off)();
    }
    
    log_info("hci_power_control_off - control closed");

    hci_stack->state = HCI_STATE_OFF;
}

static void hci_power_control_sleep(void){
    
    log_info("hci_power_control_sleep");
    
#if 0
    // don't close serial port during sleep
    
    // close low-level device
    hci_stack->hci_transport->close(hci_stack->config);
#endif
    
    // sleep mode
    if (hci_stack->control && hci_stack->control->sleep){
        (*hci_stack->control->sleep)();
    }
    
    hci_stack->state = HCI_STATE_SLEEPING;
}

static int hci_power_control_wake(void){
    
    log_info("hci_power_control_wake");

    // wake on
    if (hci_stack->control && hci_stack->control->wake){
        (*hci_stack->control->wake)();
    }
    
#if 0
    // open low-level device
    int err = hci_stack->hci_transport->open(hci_stack->config);
    if (err){
        log_error( "HCI_INIT failed, turning Bluetooth off again");
        if (hci_stack->control && hci_stack->control->off){
            (*hci_stack->control->off)();
        }
        hci_emit_hci_open_failed();
        return err;
    }
#endif
    
    return 0;
}

static void hci_power_enter_initializing_state(void){
    // set up state machine
    hci_stack->num_cmd_packets = 1; // assume that one cmd can be sent
    hci_stack->hci_packet_buffer_reserved = false;
    hci_stack->state = HCI_STATE_INITIALIZING;

#ifndef HAVE_HOST_CONTROLLER_API
    if (hci_stack->chipset_pre_init) {
        hci_stack->substate = HCI_INIT_CUSTOM_PRE_INIT;
    } else
#endif
    {
        hci_stack->substate = HCI_INIT_SEND_RESET;
    }
}

static void hci_power_enter_halting_state(void){
#ifdef ENABLE_BLE
    // drop entries scheduled for removal, mark others for re-adding
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->le_whitelist);
    while (btstack_linked_list_iterator_has_next(&it)){
        whitelist_entry_t * entry = (whitelist_entry_t*) btstack_linked_list_iterator_next(&it);
        if ((entry->state & (LE_WHITELIST_REMOVE_FROM_CONTROLLER | LE_WHITELIST_ADD_TO_CONTROLLER)) == LE_WHITELIST_REMOVE_FROM_CONTROLLER){
            btstack_linked_list_iterator_remove(&it);
            btstack_memory_whitelist_entry_free(entry);
        } else {
            entry->state = LE_WHITELIST_ADD_TO_CONTROLLER;
        }
    }
#ifdef ENABLE_LE_PERIODIC_ADVERTISING
    btstack_linked_list_iterator_init(&it, &hci_stack->le_periodic_advertiser_list);
    const uint8_t mask = LE_PERIODIC_ADVERTISER_LIST_ENTRY_REMOVE_FROM_CONTROLLER | LE_PERIODIC_ADVERTISER_LIST_ENTRY_REMOVE_FROM_CONTROLLER;
    while (btstack_linked_list_iterator_has_next(&it)){
        periodic_advertiser_list_entry_t * entry = (periodic_advertiser_list_entry_t*) btstack_linked_list_iterator_next(&it);
        if ((entry->state & mask) == LE_PERIODIC_ADVERTISER_LIST_ENTRY_REMOVE_FROM_CONTROLLER) {
            btstack_linked_list_iterator_remove(&it);
            btstack_memory_periodic_advertiser_list_entry_free(entry);
        } else {
            entry->state |= LE_PERIODIC_ADVERTISER_LIST_ENTRY_ADD_TO_CONTROLLER;
            continue;
        }
    }
#endif
#endif
    // see hci_run
    hci_stack->state = HCI_STATE_HALTING;
    hci_stack->substate = HCI_HALTING_CLASSIC_STOP;
    // setup watchdog timer for disconnect - only triggers if Controller does not respond anymore
    btstack_run_loop_set_timer(&hci_stack->timeout, 1000);
    btstack_run_loop_set_timer_handler(&hci_stack->timeout, hci_halting_timeout_handler);
    btstack_run_loop_add_timer(&hci_stack->timeout);
}

// returns error
static int hci_power_control_state_off(HCI_POWER_MODE power_mode){
    int err;
    switch (power_mode){
        case HCI_POWER_ON:
            err = hci_power_control_on();
            if (err != 0) {
                log_error("hci_power_control_on() error %d", err);
                return err;
            }
            hci_power_enter_initializing_state();
            break;
        case HCI_POWER_OFF:
            // do nothing
            break;
        case HCI_POWER_SLEEP:
            // do nothing (with SLEEP == OFF)
            break;
        default:
            btstack_assert(false);
            break;
    }
    return ERROR_CODE_SUCCESS;
}

static int hci_power_control_state_initializing(HCI_POWER_MODE power_mode){
    switch (power_mode){
        case HCI_POWER_ON:
            // do nothing
            break;
        case HCI_POWER_OFF:
            // no connections yet, just turn it off
            hci_power_control_off();
            break;
        case HCI_POWER_SLEEP:
            // no connections yet, just turn it off
            hci_power_control_sleep();
            break;
        default:
            btstack_assert(false);
            break;
    }
    return ERROR_CODE_SUCCESS;
}

static int hci_power_control_state_working(HCI_POWER_MODE power_mode) {
    switch (power_mode){
        case HCI_POWER_ON:
            // do nothing
            break;
        case HCI_POWER_OFF:
            hci_power_enter_halting_state();
            break;
        case HCI_POWER_SLEEP:
            // see hci_run
            hci_stack->state = HCI_STATE_FALLING_ASLEEP;
            hci_stack->substate = HCI_FALLING_ASLEEP_DISCONNECT;
            break;
        default:
            btstack_assert(false);
            break;
    }
    return ERROR_CODE_SUCCESS;
}

static int hci_power_control_state_halting(HCI_POWER_MODE power_mode) {
    switch (power_mode){
        case HCI_POWER_ON:
            hci_power_enter_initializing_state();
            break;
        case HCI_POWER_OFF:
            // do nothing
            break;
        case HCI_POWER_SLEEP:
            // see hci_run
            hci_stack->state = HCI_STATE_FALLING_ASLEEP;
            hci_stack->substate = HCI_FALLING_ASLEEP_DISCONNECT;
            break;
        default:
            btstack_assert(false);
            break;
    }
    return ERROR_CODE_SUCCESS;
}

static int hci_power_control_state_falling_asleep(HCI_POWER_MODE power_mode) {
    switch (power_mode){
        case HCI_POWER_ON:
            hci_power_enter_initializing_state();
            break;
        case HCI_POWER_OFF:
            hci_power_enter_halting_state();
            break;
        case HCI_POWER_SLEEP:
            // do nothing
            break;
        default:
            btstack_assert(false);
            break;
    }
    return ERROR_CODE_SUCCESS;
}

static int hci_power_control_state_sleeping(HCI_POWER_MODE power_mode) {
    int err;
    switch (power_mode){
        case HCI_POWER_ON:
            err = hci_power_control_wake();
            if (err) return err;
            hci_power_enter_initializing_state();
            break;
        case HCI_POWER_OFF:
            hci_power_enter_halting_state();
            break;
        case HCI_POWER_SLEEP:
            // do nothing
            break;
        default:
            btstack_assert(false);
            break;
    }
    return ERROR_CODE_SUCCESS;
}

int hci_power_control(HCI_POWER_MODE power_mode){
    log_info("hci_power_control: %d, current mode %u", power_mode, hci_stack->state);
    btstack_run_loop_remove_timer(&hci_stack->timeout);
    int err = 0;
    switch (hci_stack->state){
        case HCI_STATE_OFF:
            err = hci_power_control_state_off(power_mode);
            break;
        case HCI_STATE_INITIALIZING:
            err = hci_power_control_state_initializing(power_mode);
            break;
        case HCI_STATE_WORKING:
            err = hci_power_control_state_working(power_mode);
            break;
        case HCI_STATE_HALTING:
            err = hci_power_control_state_halting(power_mode);
            break;
        case HCI_STATE_FALLING_ASLEEP:
            err = hci_power_control_state_falling_asleep(power_mode);
            break;
        case HCI_STATE_SLEEPING:
            err = hci_power_control_state_sleeping(power_mode);
            break;
        default:
            btstack_assert(false);
            break;
    }
    if (err != 0){
        return err;
    }

    // create internal event
	hci_emit_state();
    
	// trigger next/first action
	hci_run();
	
    return 0;
}


static void hci_halting_run(void) {

    log_info("HCI_STATE_HALTING, substate %x\n", hci_stack->substate);

    hci_connection_t *connection;
#ifdef ENABLE_BLE
#ifdef ENABLE_LE_PERIPHERAL
    bool stop_advertismenets;
#endif
#endif

    switch (hci_stack->substate) {
        case HCI_HALTING_CLASSIC_STOP:
#ifdef ENABLE_CLASSIC
            if (!hci_can_send_command_packet_now()) return;

            if (hci_stack->connectable || hci_stack->discoverable){
                hci_stack->substate = HCI_HALTING_LE_ADV_STOP;
                hci_send_cmd(&hci_write_scan_enable, 0);
                return;
            }
#endif
            /* fall through */

        case HCI_HALTING_LE_ADV_STOP:
            hci_stack->substate = HCI_HALTING_LE_ADV_STOP;

#ifdef ENABLE_BLE
#ifdef ENABLE_LE_PERIPHERAL
            if (!hci_can_send_command_packet_now()) return;

            stop_advertismenets = (hci_stack->le_advertisements_state & LE_ADVERTISEMENT_STATE_ACTIVE) != 0;

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
            if (hci_extended_advertising_supported()){
#ifdef ENABLE_LE_PERIODIC_ADVERTISING
                btstack_linked_list_iterator_t it;
                btstack_linked_list_iterator_init(&it, &hci_stack->le_advertising_sets);
                // stop all periodic advertisements and check if an extended set is active
                while (btstack_linked_list_iterator_has_next(&it)){
                    le_advertising_set_t * advertising_set = (le_advertising_set_t*) btstack_linked_list_iterator_next(&it);
                    if ((advertising_set->state & LE_ADVERTISEMENT_STATE_PERIODIC_ACTIVE) != 0) {
                        advertising_set->state &= ~LE_ADVERTISEMENT_STATE_PERIODIC_ACTIVE;
                        hci_send_cmd(&hci_le_set_periodic_advertising_enable, 0, advertising_set->advertising_handle);
                        return;
                    }
                    if ((advertising_set->state & LE_ADVERTISEMENT_STATE_ACTIVE) != 0) {
                        stop_advertismenets = true;
                        advertising_set->state &= ~LE_ADVERTISEMENT_STATE_ACTIVE;
                    }
                }
#endif /* ENABLE_LE_PERIODIC_ADVERTISING */
                if (stop_advertismenets){
                    hci_stack->le_advertisements_state &= ~LE_ADVERTISEMENT_STATE_ACTIVE;
                    hci_send_cmd(&hci_le_set_extended_advertising_enable, 0, 0, NULL, NULL, NULL);
                    return;
                }
            } else
#else /* ENABLE_LE_PERIPHERAL */
            {
                if (stop_advertismenets) {
                    hci_stack->le_advertisements_state &= ~LE_ADVERTISEMENT_STATE_ACTIVE;
                    hci_send_cmd(&hci_le_set_advertise_enable, 0);
                    return;
                }
            }
#endif  /* ENABLE_LE_EXTENDED_ADVERTISING*/
#endif  /* ENABLE_LE_PERIPHERAL */
#endif  /* ENABLE_BLE */

            /* fall through */

        case HCI_HALTING_LE_SCAN_STOP:
            hci_stack->substate = HCI_HALTING_LE_SCAN_STOP;
            if (!hci_can_send_command_packet_now()) return;

#ifdef ENABLE_BLE
#ifdef ENABLE_LE_CENTRAL
            if (hci_stack->le_scanning_active){
                hci_le_scan_stop();
                hci_stack->substate = HCI_HALTING_DISCONNECT_ALL;
                return;
            }
#endif
#endif

            /* fall through */

        case HCI_HALTING_DISCONNECT_ALL:
            hci_stack->substate = HCI_HALTING_DISCONNECT_ALL;
            if (!hci_can_send_command_packet_now()) return;

            // close all open connections
            connection = (hci_connection_t *) hci_stack->connections;
            if (connection) {
                hci_con_handle_t con_handle = (uint16_t) connection->con_handle;

                log_info("HCI_STATE_HALTING, connection %p, handle %u, state %u", connection, con_handle, connection->state);

                // check state
                switch(connection->state) {
                    case SENT_DISCONNECT:
                    case RECEIVED_DISCONNECTION_COMPLETE:
                        // wait until connection is gone
                        return;
                    default:
                        break;
                }

                // finally, send the disconnect command
                connection->state = SENT_DISCONNECT;
                hci_send_cmd(&hci_disconnect, con_handle, ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION);
                return;
            }

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
            // stop BIGs and BIG Syncs
            if (hci_stack->le_audio_bigs != NULL){
                le_audio_big_t * big = (le_audio_big_t*) hci_stack->le_audio_bigs;
                if (big->state == LE_AUDIO_BIG_STATE_W4_TERMINATED) return;
                big->state = LE_AUDIO_BIG_STATE_W4_TERMINATED;
                hci_send_cmd(&hci_le_terminate_big, big->big_handle);
                return;
            }
            if (hci_stack->le_audio_big_syncs != NULL){
                le_audio_big_sync_t * big_sync = (le_audio_big_sync_t*) hci_stack->le_audio_big_syncs;
                if (big_sync->state == LE_AUDIO_BIG_STATE_W4_TERMINATED) return;
                big_sync->state = LE_AUDIO_BIG_STATE_W4_TERMINATED;
                hci_send_cmd(&hci_le_big_terminate_sync, big_sync->big_handle);
                return;
            }
#endif

            btstack_run_loop_remove_timer(&hci_stack->timeout);

            // no connections left, wait a bit to assert that btstack_cyrpto isn't waiting for an HCI event
            log_info("HCI_STATE_HALTING: wait 50 ms");
            hci_stack->substate = HCI_HALTING_W4_CLOSE_TIMER;
            btstack_run_loop_set_timer(&hci_stack->timeout, 50);
            btstack_run_loop_set_timer_handler(&hci_stack->timeout, hci_halting_timeout_handler);
            btstack_run_loop_add_timer(&hci_stack->timeout);
            break;

        case HCI_HALTING_W4_CLOSE_TIMER:
            // keep waiting
            break;

        case HCI_HALTING_CLOSE:
            // close left over connections (that had not been properly closed before)
            hci_stack->substate = HCI_HALTING_CLOSE_DISCARDING_CONNECTIONS;
            hci_discard_connections();

            log_info("HCI_STATE_HALTING, calling off");

            // switch mode
            hci_power_control_off();

            log_info("HCI_STATE_HALTING, emitting state");
            hci_emit_state();
            log_info("HCI_STATE_HALTING, done");
            break;

        default:
            break;
    }
};

static void hci_falling_asleep_run(void){
    hci_connection_t * connection;
    switch(hci_stack->substate) {
        case HCI_FALLING_ASLEEP_DISCONNECT:
            log_info("HCI_STATE_FALLING_ASLEEP");
            // close all open connections
            connection =  (hci_connection_t *) hci_stack->connections;
            if (connection){

                // send disconnect
                if (!hci_can_send_command_packet_now()) return;

                log_info("HCI_STATE_FALLING_ASLEEP, connection %p, handle %u", connection, (uint16_t)connection->con_handle);
                hci_send_cmd(&hci_disconnect, connection->con_handle, ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION);

                // send disconnected event right away - causes higher layer connections to get closed, too.
                hci_shutdown_connection(connection);
                return;
            }

            if (hci_classic_supported()){
                // disable page and inquiry scan
                if (!hci_can_send_command_packet_now()) return;

                log_info("HCI_STATE_HALTING, disabling inq scans");
                hci_send_cmd(&hci_write_scan_enable, hci_stack->connectable << 1); // drop inquiry scan but keep page scan

                // continue in next sub state
                hci_stack->substate = HCI_FALLING_ASLEEP_W4_WRITE_SCAN_ENABLE;
                break;
            }

            /* fall through */

            case HCI_FALLING_ASLEEP_COMPLETE:
                log_info("HCI_STATE_HALTING, calling sleep");
                // switch mode
                hci_power_control_sleep();  // changes hci_stack->state to SLEEP
                hci_emit_state();
                break;

                default:
                    break;
    }
}

#ifdef ENABLE_CLASSIC

static void hci_update_scan_enable(void){
    // 2 = page scan, 1 = inq scan
    hci_stack->new_scan_enable_value  = (hci_stack->connectable << 1) | hci_stack->discoverable;
    hci_stack->gap_tasks_classic |= GAP_TASK_WRITE_SCAN_ENABLE;
    hci_run();
}

void gap_discoverable_control(uint8_t enable){
    if (enable) enable = 1; // normalize argument
    
    if (hci_stack->discoverable == enable){
        hci_emit_scan_mode_changed(hci_stack->discoverable, hci_stack->connectable);
        return;
    }

    hci_stack->discoverable = enable;
    hci_update_scan_enable();
}

void gap_connectable_control(uint8_t enable){
    if (enable) enable = 1; // normalize argument
    
    // don't emit event
    if (hci_stack->connectable == enable) return;

    hci_stack->connectable = enable;
    hci_update_scan_enable();
}
#endif

void gap_local_bd_addr(bd_addr_t address_buffer){
    (void)memcpy(address_buffer, hci_stack->local_bd_addr, 6);
}

#ifdef ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
static void hci_host_num_completed_packets(void){

    // create packet manually as arrays are not supported and num_commands should not get reduced
    hci_reserve_packet_buffer();
    uint8_t * packet = hci_get_outgoing_packet_buffer();

    uint16_t size = 0;
    uint16_t num_handles = 0;
    packet[size++] = 0x35;
    packet[size++] = 0x0c;
    size++;  // skip param len
    size++;  // skip num handles

    // add { handle, packets } entries
    btstack_linked_item_t * it;
    for (it = (btstack_linked_item_t *) hci_stack->connections; it ; it = it->next){
        hci_connection_t * connection = (hci_connection_t *) it;
        if (connection->num_packets_completed){
            little_endian_store_16(packet, size, connection->con_handle);
            size += 2;
            little_endian_store_16(packet, size, connection->num_packets_completed);
            size += 2;
            //
            num_handles++;
            connection->num_packets_completed = 0;
        }
    }    

    packet[2] = size - 3;
    packet[3] = num_handles;

    hci_stack->host_completed_packets = 0;

    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, packet, size);
    hci_stack->hci_transport->send_packet(HCI_COMMAND_DATA_PACKET, packet, size);

    // release packet buffer for synchronous transport implementations    
    if (hci_transport_synchronous()){
        hci_release_packet_buffer();
        hci_emit_transport_packet_sent();
    }
}
#endif

static void hci_halting_timeout_handler(btstack_timer_source_t * ds){
    UNUSED(ds);
    hci_stack->substate = HCI_HALTING_CLOSE;
    hci_halting_run();
}   

static bool hci_run_acl_fragments(void){
    if (hci_stack->acl_fragmentation_total_size > 0u) {
        hci_con_handle_t con_handle = READ_ACL_CONNECTION_HANDLE(hci_stack->hci_packet_buffer);
        hci_connection_t *connection = hci_connection_for_handle(con_handle);
        if (connection) {
            if (hci_can_send_prepared_acl_packet_now(con_handle)){
                hci_send_acl_packet_fragments(connection);
                return true;
            }
        } else {
            // connection gone -> discard further fragments
            log_info("hci_run: fragmented ACL packet no connection -> discard fragment");
            hci_stack->acl_fragmentation_total_size = 0;
            hci_stack->acl_fragmentation_pos = 0;
        }
    }
    return false;
}

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
static bool hci_run_iso_fragments(void){
    if (hci_stack->iso_fragmentation_total_size > 0u) {
        // TODO: flow control
        if (hci_transport_can_send_prepared_packet_now(HCI_ISO_DATA_PACKET)){
            hci_send_iso_packet_fragments();
            return true;
        }
    }
    return false;
}
#endif

#ifdef ENABLE_CLASSIC

#ifdef ENABLE_HCI_SERIALIZED_CONTROLLER_OPERATIONS
static bool hci_classic_operation_active(void) {
    if (hci_stack->inquiry_state >= GAP_INQUIRY_STATE_W4_ACTIVE){
        return true;
    }
    if (hci_stack->remote_name_state == GAP_REMOTE_NAME_STATE_W4_COMPLETE){
        return true;
    }
    btstack_linked_item_t * it;
    for (it = (btstack_linked_item_t *) hci_stack->connections; it != NULL; it = it->next) {
        hci_connection_t *connection = (hci_connection_t *) it;
        switch (connection->state) {
            case SENT_CREATE_CONNECTION:
            case SENT_CANCEL_CONNECTION:
            case SENT_DISCONNECT:
                return true;
            default:
                break;
        }
    }
    return false;
}
#endif

static bool hci_run_general_gap_classic(void){

    // assert stack is working and classic is active
    if (hci_classic_supported() == false)      return false;
    if (hci_stack->state != HCI_STATE_WORKING) return false;

    // decline incoming connections
    if (hci_stack->decline_reason){
        uint8_t reason = hci_stack->decline_reason;
        hci_stack->decline_reason = 0;
        hci_send_cmd(&hci_reject_connection_request, hci_stack->decline_addr, reason);
        return true;
    }

    if (hci_stack->gap_tasks_classic != 0){
        hci_run_gap_tasks_classic();
        return true;
    }

    // start/stop inquiry
    if ((hci_stack->inquiry_state >= GAP_INQUIRY_DURATION_MIN) && (hci_stack->inquiry_state <= GAP_INQUIRY_DURATION_MAX)){
#ifdef ENABLE_HCI_SERIALIZED_CONTROLLER_OPERATIONS
        if (hci_classic_operation_active() == false)
#endif
        {
            uint8_t duration = hci_stack->inquiry_state;
            hci_stack->inquiry_state = GAP_INQUIRY_STATE_W4_ACTIVE;
            if (hci_stack->inquiry_max_period_length != 0){
                hci_send_cmd(&hci_periodic_inquiry_mode, hci_stack->inquiry_max_period_length, hci_stack->inquiry_min_period_length, hci_stack->inquiry_lap, duration, 0);
            } else {
                hci_send_cmd(&hci_inquiry, hci_stack->inquiry_lap, duration, 0);
            }
            return true;
        }
    }
    if (hci_stack->inquiry_state == GAP_INQUIRY_STATE_W2_CANCEL){
        hci_stack->inquiry_state = GAP_INQUIRY_STATE_W4_CANCELLED;
        hci_send_cmd(&hci_inquiry_cancel);
        return true;
    }

    if (hci_stack->inquiry_state == GAP_INQUIRY_STATE_W2_EXIT_PERIODIC){
        hci_stack->inquiry_state = GAP_INQUIRY_STATE_W4_CANCELLED;
        hci_send_cmd(&hci_exit_periodic_inquiry_mode);
        return true;
    }

    // remote name request
    if (hci_stack->remote_name_state == GAP_REMOTE_NAME_STATE_W2_SEND){
#ifdef ENABLE_HCI_SERIALIZED_CONTROLLER_OPERATIONS
        if (hci_classic_operation_active() == false)
#endif
        {
            hci_stack->remote_name_state = GAP_REMOTE_NAME_STATE_W4_COMPLETE;
            hci_send_cmd(&hci_remote_name_request, hci_stack->remote_name_addr,
                         hci_stack->remote_name_page_scan_repetition_mode, 0, hci_stack->remote_name_clock_offset);
            return true;
        }
    }
#ifdef ENABLE_CLASSIC_PAIRING_OOB
    // Local OOB data
    if (hci_stack->classic_read_local_oob_data){
        hci_stack->classic_read_local_oob_data = false;
        if (hci_command_supported(SUPPORTED_HCI_COMMAND_READ_LOCAL_OOB_EXTENDED_DATA_COMMAND)){
            hci_send_cmd(&hci_read_local_extended_oob_data);
        } else {
            hci_send_cmd(&hci_read_local_oob_data);
        }
    }
#endif
    // pairing
    if (hci_stack->gap_pairing_state != GAP_PAIRING_STATE_IDLE){
        uint8_t state = hci_stack->gap_pairing_state;
        uint8_t pin_code[16];
        switch (state){
            case GAP_PAIRING_STATE_SEND_PIN:
                hci_stack->gap_pairing_state = GAP_PAIRING_STATE_IDLE;
                memset(pin_code, 0, 16);
                memcpy(pin_code, hci_stack->gap_pairing_input.gap_pairing_pin, hci_stack->gap_pairing_pin_len);
                hci_send_cmd(&hci_pin_code_request_reply, hci_stack->gap_pairing_addr, hci_stack->gap_pairing_pin_len, pin_code);
                break;
            case GAP_PAIRING_STATE_SEND_PIN_NEGATIVE:
                hci_stack->gap_pairing_state = GAP_PAIRING_STATE_WAIT_FOR_COMMAND_COMPLETE;
                hci_send_cmd(&hci_pin_code_request_negative_reply, hci_stack->gap_pairing_addr);
                break;
            case GAP_PAIRING_STATE_SEND_PASSKEY:
                hci_stack->gap_pairing_state = GAP_PAIRING_STATE_IDLE;
                hci_send_cmd(&hci_user_passkey_request_reply, hci_stack->gap_pairing_addr, hci_stack->gap_pairing_input.gap_pairing_passkey);
                break;
            case GAP_PAIRING_STATE_SEND_PASSKEY_NEGATIVE:
                hci_stack->gap_pairing_state = GAP_PAIRING_STATE_WAIT_FOR_COMMAND_COMPLETE;
                hci_send_cmd(&hci_user_passkey_request_negative_reply, hci_stack->gap_pairing_addr);
                break;
            case GAP_PAIRING_STATE_SEND_CONFIRMATION:
                hci_stack->gap_pairing_state = GAP_PAIRING_STATE_IDLE;
                hci_send_cmd(&hci_user_confirmation_request_reply, hci_stack->gap_pairing_addr);
                break;
            case GAP_PAIRING_STATE_SEND_CONFIRMATION_NEGATIVE:
                hci_stack->gap_pairing_state = GAP_PAIRING_STATE_WAIT_FOR_COMMAND_COMPLETE;
                hci_send_cmd(&hci_user_confirmation_request_negative_reply, hci_stack->gap_pairing_addr);
                break;
            default:
                break;
        }
        return true;
    }
    return false;
}
#endif

#ifdef ENABLE_BLE

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
static uint8_t hci_le_num_phys(uint8_t phys){
    const uint8_t num_bits_set[] = { 0, 1, 1, 2, 1, 2, 2, 3 };
    btstack_assert(phys);
    return num_bits_set[phys];
}
#endif

#ifdef ENABLE_LE_CENTRAL
static void hci_le_scan_stop(void){
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    if (hci_extended_advertising_supported()) {
            hci_send_cmd(&hci_le_set_extended_scan_enable, 0, 0, 0, 0);
    } else
#endif
    {
        hci_send_cmd(&hci_le_set_scan_enable, 0, 0);
    }
}

static void
hci_send_le_create_connection(uint8_t initiator_filter_policy, bd_addr_type_t address_type, uint8_t *address) {
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    if (hci_extended_advertising_supported()) {
        // prepare arrays for all phys (LE Coded, LE 1M, LE 2M PHY)
        uint16_t le_connection_scan_interval[3];
        uint16_t le_connection_scan_window[3];
        uint16_t le_connection_interval_min[3];
        uint16_t le_connection_interval_max[3];
        uint16_t le_connection_latency[3];
        uint16_t le_supervision_timeout[3];
        uint16_t le_minimum_ce_length[3];
        uint16_t le_maximum_ce_length[3];

        uint8_t i;
        uint8_t num_phys = hci_le_num_phys(hci_stack->le_connection_phys);
        for (i=0;i<num_phys;i++){
            le_connection_scan_interval[i] = hci_stack->le_connection_scan_interval;
            le_connection_scan_window[i]   = hci_stack->le_connection_scan_window;
            le_connection_interval_min[i]  = hci_stack->le_connection_interval_min;
            le_connection_interval_max[i]  = hci_stack->le_connection_interval_max;
            le_connection_latency[i]       = hci_stack->le_connection_latency;
            le_supervision_timeout[i]      = hci_stack->le_supervision_timeout;
            le_minimum_ce_length[i]        = hci_stack->le_minimum_ce_length;
            le_maximum_ce_length[i]        = hci_stack->le_maximum_ce_length;
        }
        hci_send_cmd(&hci_le_extended_create_connection,
                     initiator_filter_policy,
                     hci_stack->le_connection_own_addr_type,   // our addr type:
                     address_type,                  // peer address type
                     address,                       // peer bd addr
                     hci_stack->le_connection_phys, // initiating PHY
                     le_connection_scan_interval,   // conn scan interval
                     le_connection_scan_window,     // conn scan windows
                     le_connection_interval_min,    // conn interval min
                     le_connection_interval_max,    // conn interval max
                     le_connection_latency,         // conn latency
                     le_supervision_timeout,        // conn latency
                     le_minimum_ce_length,          // min ce length
                     le_maximum_ce_length           // max ce length
        );
    } else
#endif
    {
        hci_send_cmd(&hci_le_create_connection,
                     hci_stack->le_connection_scan_interval,  // conn scan interval
                     hci_stack->le_connection_scan_window,    // conn scan windows
                     initiator_filter_policy,                 // don't use whitelist
                     address_type,                            // peer address type
                     address,                                 // peer bd addr
                     hci_stack->le_connection_own_addr_type,  // our addr type:
                     hci_stack->le_connection_interval_min,   // conn interval min
                     hci_stack->le_connection_interval_max,   // conn interval max
                     hci_stack->le_connection_latency,        // conn latency
                     hci_stack->le_supervision_timeout,       // conn latency
                     hci_stack->le_minimum_ce_length,         // min ce length
                     hci_stack->le_maximum_ce_length          // max ce length
        );
    }
}
#endif

#ifdef ENABLE_LE_PERIPHERAL
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
uint8_t hci_le_extended_advertising_operation_for_chunk(uint16_t pos, uint16_t len){
    uint8_t  operation = 0;
    if (pos == 0){
        // first fragment or complete data
        operation |= 1;
    }
    if (pos + LE_EXTENDED_ADVERTISING_MAX_CHUNK_LEN >= len){
        // last fragment or complete data
        operation |= 2;
    }
    return operation;
}
#endif
#endif

static bool hci_run_general_gap_le(void){

    btstack_linked_list_iterator_t lit;

    // Phase 1: collect what to stop

#ifdef ENABLE_LE_CENTRAL
    bool scanning_stop = false;
    bool connecting_stop = false;
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
#ifdef ENABLE_LE_PERIODIC_ADVERTISING
    bool periodic_sync_stop = false;
#endif
#endif
#endif

#ifdef ENABLE_LE_PERIPHERAL
    bool advertising_stop = false;
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    le_advertising_set_t * advertising_stop_set = NULL;
#ifdef ENABLE_LE_PERIODIC_ADVERTISING
    bool periodic_advertising_stop = false;
#endif
#endif
#endif

    // check if own address changes
    uint8_t address_change_mask = LE_ADVERTISEMENT_TASKS_SET_ADDRESS | LE_ADVERTISEMENT_TASKS_SET_ADDRESS_SET_0;
    bool random_address_change = (hci_stack->le_advertisements_todo & address_change_mask) != 0;

    // check if whitelist needs modification
    bool whitelist_modification_pending = false;
    btstack_linked_list_iterator_init(&lit, &hci_stack->le_whitelist);
    while (btstack_linked_list_iterator_has_next(&lit)){
        whitelist_entry_t * entry = (whitelist_entry_t*) btstack_linked_list_iterator_next(&lit);
        if (entry->state & (LE_WHITELIST_REMOVE_FROM_CONTROLLER | LE_WHITELIST_ADD_TO_CONTROLLER)){
            whitelist_modification_pending = true;
            break;
        }
    }

    // check if resolving list needs modification
    bool resolving_list_modification_pending = false;
#ifdef ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION
    bool resolving_list_supported = hci_command_supported(SUPPORTED_HCI_COMMAND_LE_SET_ADDRESS_RESOLUTION_ENABLE);
	if (resolving_list_supported && hci_stack->le_resolving_list_state != LE_RESOLVING_LIST_DONE){
        resolving_list_modification_pending = true;
    }
#endif

#ifdef ENABLE_LE_CENTRAL

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    // check if periodic advertiser list needs modification
    bool periodic_list_modification_pending = false;
    btstack_linked_list_iterator_init(&lit, &hci_stack->le_periodic_advertiser_list);
    while (btstack_linked_list_iterator_has_next(&lit)){
        periodic_advertiser_list_entry_t * entry = (periodic_advertiser_list_entry_t*) btstack_linked_list_iterator_next(&lit);
        if (entry->state & (LE_PERIODIC_ADVERTISER_LIST_ENTRY_ADD_TO_CONTROLLER | LE_PERIODIC_ADVERTISER_LIST_ENTRY_REMOVE_FROM_CONTROLLER)){
            periodic_list_modification_pending = true;
            break;
        }
    }
#endif

    // scanning control
    if (hci_stack->le_scanning_active) {
        // stop if:
        // - parameter change required
        // - it's disabled
        // - whitelist change required but used for scanning
        // - resolving list modified
        // - own address changes
        bool scanning_uses_whitelist = (hci_stack->le_scan_filter_policy & 1) == 1;
        if ((hci_stack->le_scanning_param_update) ||
            !hci_stack->le_scanning_enabled ||
            (scanning_uses_whitelist && whitelist_modification_pending) ||
            resolving_list_modification_pending ||
            random_address_change){

            scanning_stop = true;
        }
    }

    // connecting control
    bool connecting_with_whitelist;
    switch (hci_stack->le_connecting_state){
        case LE_CONNECTING_DIRECT:
        case LE_CONNECTING_WHITELIST:
            // stop connecting if:
            // - connecting uses white and whitelist modification pending
            // - if it got disabled
            // - resolving list modified
            // - own address changes
            connecting_with_whitelist = hci_stack->le_connecting_state == LE_CONNECTING_WHITELIST;
            if ((connecting_with_whitelist && whitelist_modification_pending) ||
                (hci_stack->le_connecting_request == LE_CONNECTING_IDLE) ||
                resolving_list_modification_pending ||
                random_address_change) {

                connecting_stop = true;
            }
            break;
        default:
            break;
    }

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
#ifdef ENABLE_LE_PERIODIC_ADVERTISING
    // periodic sync control
    bool sync_with_advertiser_list;
    switch(hci_stack->le_periodic_sync_state){
        case LE_CONNECTING_DIRECT:
        case LE_CONNECTING_WHITELIST:
            // stop sync if:
            // - sync with advertiser list and advertiser list modification pending
            // - if it got disabled
            sync_with_advertiser_list = hci_stack->le_periodic_sync_state == LE_CONNECTING_WHITELIST;
            if ((sync_with_advertiser_list && periodic_list_modification_pending) ||
                    (hci_stack->le_periodic_sync_request == LE_CONNECTING_IDLE)){
                periodic_sync_stop = true;
            }
            break;
        default:
            break;
    }
#endif
#endif

#endif /* ENABLE_LE_CENTRAL */

#ifdef ENABLE_LE_PERIPHERAL
    // le advertisement control
    if ((hci_stack->le_advertisements_state & LE_ADVERTISEMENT_STATE_ACTIVE) != 0){
        // stop if:
        // - parameter change required
        // - random address used in advertising and changes
        // - it's disabled
        // - whitelist change required but used for advertisement filter policy
        // - resolving list modified
        // - own address changes
        bool advertising_uses_whitelist = hci_stack->le_advertisements_filter_policy != 0;
        bool advertising_uses_random_address = hci_stack->le_own_addr_type != BD_ADDR_TYPE_LE_PUBLIC;
        bool advertising_change    = (hci_stack->le_advertisements_todo & LE_ADVERTISEMENT_TASKS_SET_PARAMS)  != 0;
        if (advertising_change ||
            (advertising_uses_random_address && random_address_change) ||
            (hci_stack->le_advertisements_enabled_for_current_roles == 0) ||
            (advertising_uses_whitelist && whitelist_modification_pending) ||
            resolving_list_modification_pending ||
            random_address_change) {

            advertising_stop = true;
        }
    }

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    if (hci_extended_advertising_supported() && (advertising_stop == false)){
        btstack_linked_list_iterator_t it;
        btstack_linked_list_iterator_init(&it, &hci_stack->le_advertising_sets);
        while (btstack_linked_list_iterator_has_next(&it)){
            le_advertising_set_t * advertising_set = (le_advertising_set_t*) btstack_linked_list_iterator_next(&it);
            if ((advertising_set->state & LE_ADVERTISEMENT_STATE_ACTIVE) != 0) {
                // stop if:
                // - parameter change required
                // - random address used in connectable advertising and changes
                // - it's disabled
                // - whitelist change required but used for advertisement filter policy
                // - resolving list modified
                // - own address changes
                // - advertisement set will be removed
                bool advertising_uses_whitelist = advertising_set->extended_params.advertising_filter_policy != 0;
                bool advertising_connectable = (advertising_set->extended_params.advertising_event_properties & 1) != 0;
                bool advertising_uses_random_address =
                        (advertising_set->extended_params.own_address_type != BD_ADDR_TYPE_LE_PUBLIC) &&
                        advertising_connectable;
                bool advertising_parameter_change = (advertising_set->tasks & LE_ADVERTISEMENT_TASKS_SET_PARAMS) != 0;
                bool advertising_enabled = (advertising_set->state & LE_ADVERTISEMENT_STATE_ENABLED) != 0;
                bool advertising_set_random_address_change =
                        (advertising_set->tasks & LE_ADVERTISEMENT_TASKS_SET_ADDRESS) != 0;
                bool advertising_set_will_be_removed =
                        (advertising_set->state & LE_ADVERTISEMENT_TASKS_REMOVE_SET) != 0;
                if (advertising_parameter_change ||
                    (advertising_uses_random_address && advertising_set_random_address_change) ||
                    (advertising_enabled == false) ||
                    (advertising_uses_whitelist && whitelist_modification_pending) ||
                    resolving_list_modification_pending ||
                    advertising_set_will_be_removed) {

                    advertising_stop = true;
                    advertising_stop_set = advertising_set;
                    break;
                }
            }
#ifdef ENABLE_LE_PERIODIC_ADVERTISING
            if ((advertising_set->state & LE_ADVERTISEMENT_STATE_PERIODIC_ACTIVE) != 0) {
                // stop if:
                // - it's disabled
                // - parameter change required
                bool periodic_enabled = (advertising_set->state & LE_ADVERTISEMENT_STATE_PERIODIC_ENABLED) != 0;
                bool periodic_parameter_change = (advertising_set->tasks & LE_ADVERTISEMENT_TASKS_SET_PERIODIC_PARAMS) != 0;
                if ((periodic_enabled == false) || periodic_parameter_change){
                    periodic_advertising_stop = true;
                    advertising_stop_set = advertising_set;
                }
            }
#endif /* ENABLE_LE_PERIODIC_ADVERTISING */
        }
    }
#endif

#endif


    // Phase 2: stop everything that should be off during modifications


    // 2.1 Outgoing connection
#ifdef ENABLE_LE_CENTRAL
    if (connecting_stop){
        hci_send_cmd(&hci_le_create_connection_cancel);
        return true;
    }
#endif

    // 2.2 Scanning
#ifdef ENABLE_LE_CENTRAL
    if (scanning_stop){
        hci_stack->le_scanning_active = false;
        hci_le_scan_stop();
        return true;
    }

    // 2.3 Periodic Sync
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    if (hci_stack->le_periodic_terminate_sync_handle != HCI_CON_HANDLE_INVALID){
        uint16_t sync_handle = hci_stack->le_periodic_terminate_sync_handle;
        hci_stack->le_periodic_terminate_sync_handle = HCI_CON_HANDLE_INVALID;
        hci_send_cmd(&hci_le_periodic_advertising_terminate_sync, sync_handle);
        return true;
    }
#ifdef ENABLE_LE_PERIODIC_ADVERTISING
    if (periodic_sync_stop){
        hci_stack->le_periodic_sync_state = LE_CONNECTING_CANCEL;
        hci_send_cmd(&hci_le_periodic_advertising_create_sync_cancel);
        return true;
    }
#endif /* ENABLE_LE_PERIODIC_ADVERTISING */
#endif /* ENABLE_LE_EXTENDED_ADVERTISING */
#endif /* ENABLE_LE_CENTRAL */

    // 2.4 Advertising: legacy, extended, periodic
#ifdef ENABLE_LE_PERIPHERAL
    if (advertising_stop){
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
        if (hci_extended_advertising_supported()) {
            uint8_t advertising_stop_handle;
            if (advertising_stop_set != NULL){
                advertising_stop_handle = advertising_stop_set->advertising_handle;
                advertising_stop_set->state &= ~LE_ADVERTISEMENT_STATE_ACTIVE;
            } else {
                advertising_stop_handle = 0;
                hci_stack->le_advertisements_state &= ~LE_ADVERTISEMENT_STATE_ACTIVE;
            }
            const uint8_t advertising_handles[] = { advertising_stop_handle };
            const uint16_t durations[] = { 0 };
            const uint16_t max_events[] = { 0 };
            hci_send_cmd(&hci_le_set_extended_advertising_enable, 0, 1, advertising_handles, durations, max_events);
        } else
#endif
        {
            hci_stack->le_advertisements_state &= ~LE_ADVERTISEMENT_STATE_ACTIVE;
            hci_send_cmd(&hci_le_set_advertise_enable, 0);
        }
        return true;
    }
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
#ifdef ENABLE_LE_PERIODIC_ADVERTISING
    if (periodic_advertising_stop){
        advertising_stop_set->state &= ~LE_ADVERTISEMENT_STATE_PERIODIC_ACTIVE;
        hci_send_cmd(&hci_le_set_periodic_advertising_enable, 0, advertising_stop_set->advertising_handle);
        return true;
    }
#endif /* ENABLE_LE_PERIODIC_ADVERTISING */
#endif /* ENABLE_LE_EXTENDED_ADVERTISING */
#endif /* ENABLE_LE_PERIPHERAL */


    // Phase 3: modify

    if (hci_stack->le_advertisements_todo & LE_ADVERTISEMENT_TASKS_SET_ADDRESS){
        hci_stack->le_advertisements_todo &= ~LE_ADVERTISEMENT_TASKS_SET_ADDRESS;
        hci_send_cmd(&hci_le_set_random_address, hci_stack->le_random_address);
#ifdef ENABLE_LE_SET_ADV_PARAMS_ON_RANDOM_ADDRESS_CHANGE
        // workaround: on some Controllers, address in advertisements is updated only after next dv params set
        hci_stack->le_advertisements_todo |= LE_ADVERTISEMENT_TASKS_SET_PARAMS;
#endif
        return true;
    }

#ifdef ENABLE_LE_CENTRAL
    if (hci_stack->le_scanning_param_update){
        hci_stack->le_scanning_param_update = false;
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
        if (hci_extended_advertising_supported()){
            // prepare arrays for all phys (LE Coded and LE 1M PHY)
            uint8_t  scan_types[2];
            uint16_t scan_intervals[2];
            uint16_t scan_windows[2];

            uint8_t i;
            uint8_t num_phys = hci_le_num_phys(hci_stack->le_scan_phys);
            for (i=0;i<num_phys;i++){
                scan_types[i]     = hci_stack->le_scan_type;
                scan_intervals[i] = hci_stack->le_scan_interval;
                scan_windows[i]   = hci_stack->le_scan_window;
            }
            hci_send_cmd(&hci_le_set_extended_scan_parameters, hci_stack->le_own_addr_type,
                         hci_stack->le_scan_filter_policy, hci_stack->le_scan_phys, scan_types, scan_intervals, scan_windows);
        } else
#endif
        {
            hci_send_cmd(&hci_le_set_scan_parameters, hci_stack->le_scan_type, hci_stack->le_scan_interval, hci_stack->le_scan_window,
                         hci_stack->le_own_addr_type, hci_stack->le_scan_filter_policy);
        }
        return true;
    }
#endif

#ifdef ENABLE_LE_PERIPHERAL
    if (hci_stack->le_advertisements_todo & LE_ADVERTISEMENT_TASKS_SET_PARAMS){
        hci_stack->le_advertisements_todo &= ~LE_ADVERTISEMENT_TASKS_SET_PARAMS;
        hci_stack->le_advertisements_own_addr_type = hci_stack->le_own_addr_type;
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
        if (hci_extended_advertising_supported()){
            // map advertisment type to advertising event properties
            uint16_t adv_event_properties = 0;
            const uint16_t mapping[] = { 0b00010011, 0b00010101, 0b00011101, 0b00010010, 0b00010000};
            if (hci_stack->le_advertisements_type < (sizeof(mapping)/sizeof(uint16_t))){
                adv_event_properties = mapping[hci_stack->le_advertisements_type];
            }
            hci_stack->le_advertising_set_in_current_command = 0;
            hci_send_cmd(&hci_le_set_extended_advertising_parameters,
                         0,
                         adv_event_properties,
                         hci_stack->le_advertisements_interval_min,
                         hci_stack->le_advertisements_interval_max,
                         hci_stack->le_advertisements_channel_map,
                         hci_stack->le_advertisements_own_addr_type,
                         hci_stack->le_advertisements_direct_address_type,
                         hci_stack->le_advertisements_direct_address,
                         hci_stack->le_advertisements_filter_policy,
                         0x7f,  // tx power: no preference
                         0x01,  // primary adv phy: LE 1M
                         0,     // secondary adv max skip
                         0x01,  // secondary adv phy
                         0,     // adv sid
                         0      // scan request notification
                         );
        } else
#endif
        {
            hci_send_cmd(&hci_le_set_advertising_parameters,
                         hci_stack->le_advertisements_interval_min,
                         hci_stack->le_advertisements_interval_max,
                         hci_stack->le_advertisements_type,
                         hci_stack->le_advertisements_own_addr_type,
                         hci_stack->le_advertisements_direct_address_type,
                         hci_stack->le_advertisements_direct_address,
                         hci_stack->le_advertisements_channel_map,
                         hci_stack->le_advertisements_filter_policy);
        }
        return true;
    }

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    // assumption: only set if extended advertising is supported
    if ((hci_stack->le_advertisements_todo & LE_ADVERTISEMENT_TASKS_SET_ADDRESS_SET_0) != 0){
        hci_stack->le_advertisements_todo &= ~LE_ADVERTISEMENT_TASKS_SET_ADDRESS_SET_0;
        hci_send_cmd(&hci_le_set_advertising_set_random_address, 0, hci_stack->le_random_address);
        return true;
    }
#endif

    if (hci_stack->le_advertisements_todo & LE_ADVERTISEMENT_TASKS_SET_ADV_DATA){
        hci_stack->le_advertisements_todo &= ~LE_ADVERTISEMENT_TASKS_SET_ADV_DATA;
        uint8_t adv_data_clean[31];
        memset(adv_data_clean, 0, sizeof(adv_data_clean));
        (void)memcpy(adv_data_clean, hci_stack->le_advertisements_data,
                     hci_stack->le_advertisements_data_len);
        btstack_replace_bd_addr_placeholder(adv_data_clean, hci_stack->le_advertisements_data_len, hci_stack->local_bd_addr);
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
        if (hci_extended_advertising_supported()){
            hci_stack->le_advertising_set_in_current_command = 0;
            hci_send_cmd(&hci_le_set_extended_advertising_data, 0, 0x03, 0x01, hci_stack->le_advertisements_data_len, adv_data_clean);
        } else
#endif
        {
            hci_send_cmd(&hci_le_set_advertising_data, hci_stack->le_advertisements_data_len, adv_data_clean);
        }
        return true;
    }

    if (hci_stack->le_advertisements_todo & LE_ADVERTISEMENT_TASKS_SET_SCAN_DATA){
        hci_stack->le_advertisements_todo &= ~LE_ADVERTISEMENT_TASKS_SET_SCAN_DATA;
        uint8_t scan_data_clean[31];
        memset(scan_data_clean, 0, sizeof(scan_data_clean));
        (void)memcpy(scan_data_clean, hci_stack->le_scan_response_data,
                     hci_stack->le_scan_response_data_len);
        btstack_replace_bd_addr_placeholder(scan_data_clean, hci_stack->le_scan_response_data_len, hci_stack->local_bd_addr);
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
        if (hci_extended_advertising_supported()){
            hci_stack->le_advertising_set_in_current_command = 0;
            hci_send_cmd(&hci_le_set_extended_scan_response_data, 0, 0x03, 0x01, hci_stack->le_scan_response_data_len, scan_data_clean);
        } else
#endif
        {
            hci_send_cmd(&hci_le_set_scan_response_data, hci_stack->le_scan_response_data_len, scan_data_clean);
        }
        return true;
    }

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    if (hci_extended_advertising_supported()) {
        btstack_linked_list_iterator_t it;
        btstack_linked_list_iterator_init(&it, &hci_stack->le_advertising_sets);
        while (btstack_linked_list_iterator_has_next(&it)){
            le_advertising_set_t * advertising_set = (le_advertising_set_t*) btstack_linked_list_iterator_next(&it);
            if ((advertising_set->tasks & LE_ADVERTISEMENT_TASKS_REMOVE_SET) != 0) {
                advertising_set->tasks &= ~LE_ADVERTISEMENT_TASKS_REMOVE_SET;
                hci_stack->le_advertising_set_in_current_command = advertising_set->advertising_handle;
                hci_send_cmd(&hci_le_remove_advertising_set, advertising_set->advertising_handle);
                return true;
            }
            if ((advertising_set->tasks & LE_ADVERTISEMENT_TASKS_SET_PARAMS) != 0){
                advertising_set->tasks &= ~LE_ADVERTISEMENT_TASKS_SET_PARAMS;
                hci_stack->le_advertising_set_in_current_command = advertising_set->advertising_handle;
                hci_send_cmd(&hci_le_set_extended_advertising_parameters,
                             advertising_set->advertising_handle,
                             advertising_set->extended_params.advertising_event_properties,
                             advertising_set->extended_params.primary_advertising_interval_min,
                             advertising_set->extended_params.primary_advertising_interval_max,
                             advertising_set->extended_params.primary_advertising_channel_map,
                             advertising_set->extended_params.own_address_type,
                             advertising_set->extended_params.peer_address_type,
                             advertising_set->extended_params.peer_address,
                             advertising_set->extended_params.advertising_filter_policy,
                             advertising_set->extended_params.advertising_tx_power,
                             advertising_set->extended_params.primary_advertising_phy,
                             advertising_set->extended_params.secondary_advertising_max_skip,
                             advertising_set->extended_params.secondary_advertising_phy,
                             advertising_set->extended_params.advertising_sid,
                             advertising_set->extended_params.scan_request_notification_enable
                );
                return true;
            }
            if ((advertising_set->tasks & LE_ADVERTISEMENT_TASKS_SET_ADDRESS) != 0){
                advertising_set->tasks &= ~LE_ADVERTISEMENT_TASKS_SET_ADDRESS;
                hci_send_cmd(&hci_le_set_advertising_set_random_address, advertising_set->advertising_handle, advertising_set->random_address);
                return true;
            }
            if ((advertising_set->tasks & LE_ADVERTISEMENT_TASKS_SET_ADV_DATA) != 0) {
                uint16_t pos = advertising_set->adv_data_pos;
                uint8_t  operation = hci_le_extended_advertising_operation_for_chunk(pos, advertising_set->adv_data_len);
                uint16_t data_to_upload = btstack_min(advertising_set->adv_data_len - pos, LE_EXTENDED_ADVERTISING_MAX_CHUNK_LEN);
                if ((operation & 0x02) != 0){
                    // last fragment or complete data
                    operation |= 2;
                    advertising_set->adv_data_pos = 0;
                    advertising_set->tasks &= ~LE_ADVERTISEMENT_TASKS_SET_ADV_DATA;
                } else {
                    advertising_set->adv_data_pos += data_to_upload;
                }
                hci_stack->le_advertising_set_in_current_command = advertising_set->advertising_handle;
                hci_send_cmd(&hci_le_set_extended_advertising_data, advertising_set->advertising_handle, operation, 0x01, data_to_upload, &advertising_set->adv_data[pos]);
                return true;
            }
            if ((advertising_set->tasks & LE_ADVERTISEMENT_TASKS_SET_SCAN_DATA) != 0) {
                uint16_t pos = advertising_set->scan_data_pos;
                uint8_t  operation = hci_le_extended_advertising_operation_for_chunk(pos, advertising_set->scan_data_len);
                uint16_t data_to_upload = btstack_min(advertising_set->scan_data_len - pos, LE_EXTENDED_ADVERTISING_MAX_CHUNK_LEN);
                if ((operation & 0x02) != 0){
                    advertising_set->scan_data_pos = 0;
                    advertising_set->tasks &= ~LE_ADVERTISEMENT_TASKS_SET_SCAN_DATA;
                } else {
                    advertising_set->scan_data_pos += data_to_upload;
                }
                hci_stack->le_advertising_set_in_current_command = advertising_set->advertising_handle;
                hci_send_cmd(&hci_le_set_extended_scan_response_data, advertising_set->advertising_handle, operation, 0x01, data_to_upload, &advertising_set->scan_data[pos]);
                return true;
            }
#ifdef ENABLE_LE_PERIODIC_ADVERTISING
            if ((advertising_set->tasks & LE_ADVERTISEMENT_TASKS_SET_PERIODIC_PARAMS) != 0){
                advertising_set->tasks &= ~LE_ADVERTISEMENT_TASKS_SET_PERIODIC_PARAMS;
                hci_stack->le_advertising_set_in_current_command = advertising_set->advertising_handle;
                hci_send_cmd(&hci_le_set_periodic_advertising_parameters,
                             advertising_set->advertising_handle,
                             advertising_set->periodic_params.periodic_advertising_interval_min,
                             advertising_set->periodic_params.periodic_advertising_interval_max,
                             advertising_set->periodic_params.periodic_advertising_properties);
                return true;
            }
            if ((advertising_set->tasks & LE_ADVERTISEMENT_TASKS_SET_PERIODIC_DATA) != 0) {
                uint16_t pos = advertising_set->periodic_data_pos;
                uint8_t  operation = hci_le_extended_advertising_operation_for_chunk(pos, advertising_set->periodic_data_len);
                uint16_t data_to_upload = btstack_min(advertising_set->periodic_data_len - pos, LE_EXTENDED_ADVERTISING_MAX_CHUNK_LEN);
                if ((operation & 0x02) != 0){
                    // last fragment or complete data
                    operation |= 2;
                    advertising_set->periodic_data_pos = 0;
                    advertising_set->tasks &= ~LE_ADVERTISEMENT_TASKS_SET_PERIODIC_DATA;
                } else {
                    advertising_set->periodic_data_pos += data_to_upload;
                }
                hci_stack->le_advertising_set_in_current_command = advertising_set->advertising_handle;
                hci_send_cmd(&hci_le_set_periodic_advertising_data, advertising_set->advertising_handle, operation, data_to_upload, &advertising_set->periodic_data[pos]);
                return true;
            }
#endif /* ENABLE_LE_PERIODIC_ADVERTISING */
        }
    }
#endif

#endif

#ifdef ENABLE_LE_CENTRAL
    // if connect with whitelist was active and is not cancelled yet, wait until next time
    if (hci_stack->le_connecting_state == LE_CONNECTING_CANCEL) return false;
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    // if periodic sync with advertiser list was active and is not cancelled yet, wait until next time
    if (hci_stack->le_periodic_sync_state == LE_CONNECTING_CANCEL) return false;
#endif
#endif

    // LE Whitelist Management
    if (whitelist_modification_pending){
        // add/remove entries
        btstack_linked_list_iterator_init(&lit, &hci_stack->le_whitelist);
        while (btstack_linked_list_iterator_has_next(&lit)){
            whitelist_entry_t * entry = (whitelist_entry_t*) btstack_linked_list_iterator_next(&lit);
			if (entry->state & LE_WHITELIST_REMOVE_FROM_CONTROLLER){
				entry->state &= ~LE_WHITELIST_REMOVE_FROM_CONTROLLER;
				hci_send_cmd(&hci_le_remove_device_from_white_list, entry->address_type, entry->address);
				return true;
			}
            if (entry->state & LE_WHITELIST_ADD_TO_CONTROLLER){
				entry->state &= ~LE_WHITELIST_ADD_TO_CONTROLLER;
                entry->state |= LE_WHITELIST_ON_CONTROLLER;
                hci_send_cmd(&hci_le_add_device_to_white_list, entry->address_type, entry->address);
                return true;
            }
            if ((entry->state & LE_WHITELIST_ON_CONTROLLER) == 0){
				btstack_linked_list_remove(&hci_stack->le_whitelist, (btstack_linked_item_t *) entry);
				btstack_memory_whitelist_entry_free(entry);
            }
        }
    }

#ifdef ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION
    // LE Resolving List Management
    if (resolving_list_supported) {
		uint16_t i;
		switch (hci_stack->le_resolving_list_state) {
			case LE_RESOLVING_LIST_SEND_ENABLE_ADDRESS_RESOLUTION:
				hci_stack->le_resolving_list_state = LE_RESOLVING_LIST_READ_SIZE;
				hci_send_cmd(&hci_le_set_address_resolution_enabled, 1);
				return true;
			case LE_RESOLVING_LIST_READ_SIZE:
				hci_stack->le_resolving_list_state = LE_RESOLVING_LIST_SEND_CLEAR;
				hci_send_cmd(&hci_le_read_resolving_list_size);
				return true;
			case LE_RESOLVING_LIST_SEND_CLEAR:
				hci_stack->le_resolving_list_state = LE_RESOLVING_LIST_UPDATES_ENTRIES;
				(void) memset(hci_stack->le_resolving_list_add_entries, 0xff,
							  sizeof(hci_stack->le_resolving_list_add_entries));
                (void) memset(hci_stack->le_resolving_list_set_privacy_mode, 0xff,
                              sizeof(hci_stack->le_resolving_list_set_privacy_mode));
				(void) memset(hci_stack->le_resolving_list_remove_entries, 0,
							  sizeof(hci_stack->le_resolving_list_remove_entries));
				hci_send_cmd(&hci_le_clear_resolving_list);
				return true;
			case LE_RESOLVING_LIST_UPDATES_ENTRIES:
                // first remove old entries
				for (i = 0; i < MAX_NUM_RESOLVING_LIST_ENTRIES && i < le_device_db_max_count(); i++) {
					uint8_t offset = i >> 3;
					uint8_t mask = 1 << (i & 7);
					if ((hci_stack->le_resolving_list_remove_entries[offset] & mask) == 0) continue;
					hci_stack->le_resolving_list_remove_entries[offset] &= ~mask;
					bd_addr_t peer_identity_addreses;
					int peer_identity_addr_type = (int) BD_ADDR_TYPE_UNKNOWN;
					sm_key_t peer_irk;
					le_device_db_info(i, &peer_identity_addr_type, peer_identity_addreses, peer_irk);
					if (peer_identity_addr_type == BD_ADDR_TYPE_UNKNOWN) continue;

#ifdef ENABLE_LE_WHITELIST_TOUCH_AFTER_RESOLVING_LIST_UPDATE
					// trigger whitelist entry 'update' (work around for controller bug)
					btstack_linked_list_iterator_init(&lit, &hci_stack->le_whitelist);
					while (btstack_linked_list_iterator_has_next(&lit)) {
						whitelist_entry_t *entry = (whitelist_entry_t *) btstack_linked_list_iterator_next(&lit);
						if (entry->address_type != peer_identity_addr_type) continue;
						if (memcmp(entry->address, peer_identity_addreses, 6) != 0) continue;
						log_info("trigger whitelist update %s", bd_addr_to_str(peer_identity_addreses));
						entry->state |= LE_WHITELIST_REMOVE_FROM_CONTROLLER | LE_WHITELIST_ADD_TO_CONTROLLER;
					}
#endif

					hci_send_cmd(&hci_le_remove_device_from_resolving_list, peer_identity_addr_type,
								 peer_identity_addreses);
					return true;
				}

                // then add new entries
				for (i = 0; i < MAX_NUM_RESOLVING_LIST_ENTRIES && i < le_device_db_max_count(); i++) {
					uint8_t offset = i >> 3;
					uint8_t mask = 1 << (i & 7);
					if ((hci_stack->le_resolving_list_add_entries[offset] & mask) == 0) continue;
					hci_stack->le_resolving_list_add_entries[offset] &= ~mask;
					bd_addr_t peer_identity_addreses;
					int peer_identity_addr_type = (int) BD_ADDR_TYPE_UNKNOWN;
					sm_key_t peer_irk;
					le_device_db_info(i, &peer_identity_addr_type, peer_identity_addreses, peer_irk);
					if (peer_identity_addr_type == BD_ADDR_TYPE_UNKNOWN) continue;
                    if (btstack_is_null(peer_irk, 16)) continue;
					const uint8_t *local_irk = gap_get_persistent_irk();
					// command uses format specifier 'P' that stores 16-byte value without flip
					uint8_t local_irk_flipped[16];
					uint8_t peer_irk_flipped[16];
					reverse_128(local_irk, local_irk_flipped);
					reverse_128(peer_irk, peer_irk_flipped);
					hci_send_cmd(&hci_le_add_device_to_resolving_list, peer_identity_addr_type, peer_identity_addreses,
								 peer_irk_flipped, local_irk_flipped);
					return true;
				}

                // finally, set privacy mode
                for (i = 0; i < MAX_NUM_RESOLVING_LIST_ENTRIES && i < le_device_db_max_count(); i++) {
                    uint8_t offset = i >> 3;
                    uint8_t mask = 1 << (i & 7);
                    if ((hci_stack->le_resolving_list_set_privacy_mode[offset] & mask) == 0) continue;
                    hci_stack->le_resolving_list_set_privacy_mode[offset] &= ~mask;
                    if (hci_stack->le_privacy_mode == LE_PRIVACY_MODE_NETWORK) {
                        // Network Privacy Mode is default
                        continue;
                    }
                    bd_addr_t peer_identity_address;
                    int peer_identity_addr_type = (int) BD_ADDR_TYPE_UNKNOWN;
                    sm_key_t peer_irk;
                    le_device_db_info(i, &peer_identity_addr_type, peer_identity_address, peer_irk);
                    if (peer_identity_addr_type == BD_ADDR_TYPE_UNKNOWN) continue;
                    if (btstack_is_null(peer_irk, 16)) continue;
                    // command uses format specifier 'P' that stores 16-byte value without flip
                    uint8_t peer_irk_flipped[16];
                    reverse_128(peer_irk, peer_irk_flipped);
                    hci_send_cmd(&hci_le_set_privacy_mode, peer_identity_addr_type, peer_identity_address, hci_stack->le_privacy_mode);
                    return true;
                }
				hci_stack->le_resolving_list_state = LE_RESOLVING_LIST_DONE;
				break;

			default:
				break;
		}
	}
    hci_stack->le_resolving_list_state = LE_RESOLVING_LIST_DONE;
#endif

#ifdef ENABLE_LE_CENTRAL
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    // LE Whitelist Management
    if (periodic_list_modification_pending){
        // add/remove entries
        btstack_linked_list_iterator_init(&lit, &hci_stack->le_periodic_advertiser_list);
        while (btstack_linked_list_iterator_has_next(&lit)){
            periodic_advertiser_list_entry_t * entry = (periodic_advertiser_list_entry_t*) btstack_linked_list_iterator_next(&lit);
            if (entry->state & LE_PERIODIC_ADVERTISER_LIST_ENTRY_REMOVE_FROM_CONTROLLER){
                entry->state &= ~LE_PERIODIC_ADVERTISER_LIST_ENTRY_REMOVE_FROM_CONTROLLER;
                hci_send_cmd(&hci_le_remove_device_from_periodic_advertiser_list, entry->address_type, entry->address);
                return true;
            }
            if (entry->state & LE_PERIODIC_ADVERTISER_LIST_ENTRY_ADD_TO_CONTROLLER){
                entry->state &= ~LE_PERIODIC_ADVERTISER_LIST_ENTRY_ADD_TO_CONTROLLER;
                entry->state |= LE_PERIODIC_ADVERTISER_LIST_ENTRY_ON_CONTROLLER;
                hci_send_cmd(&hci_le_add_device_to_periodic_advertiser_list, entry->address_type, entry->address, entry->sid);
                return true;
            }
            if ((entry->state & LE_PERIODIC_ADVERTISER_LIST_ENTRY_ON_CONTROLLER) == 0){
                btstack_linked_list_remove(&hci_stack->le_periodic_advertiser_list, (btstack_linked_item_t *) entry);
                btstack_memory_periodic_advertiser_list_entry_free(entry);
            }
        }
    }
#endif
#endif

#ifdef ENABLE_LE_CENTRAL
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
#ifdef ENABLE_LE_PERIODIC_ADVERTISING
    if (hci_stack->le_past_set_default_params){
        hci_stack->le_past_set_default_params = false;
        hci_send_cmd(&hci_le_set_default_periodic_advertising_sync_transfer_parameters,
                     hci_stack->le_past_mode,
                     hci_stack->le_past_skip,
                     hci_stack->le_past_sync_timeout,
                     hci_stack->le_past_cte_type);
        return true;
    }
#endif
#endif
#endif

    // post-pone all actions until stack is fully working
    if (hci_stack->state != HCI_STATE_WORKING) return false;

    // advertisements, active scanning, and creating connections requires random address to be set if using private address
    if ( (hci_stack->le_own_addr_type != BD_ADDR_TYPE_LE_PUBLIC) && (hci_stack->le_random_address_set == 0u) ) return false;

    // Phase 4: restore state

#ifdef ENABLE_LE_CENTRAL
    // re-start scanning
    if ((hci_stack->le_scanning_enabled && !hci_stack->le_scanning_active)){
        hci_stack->le_scanning_active = true;
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
        if (hci_extended_advertising_supported()){
            hci_send_cmd(&hci_le_set_extended_scan_enable, 1, hci_stack->le_scan_filter_duplicates, 0, 0);
        } else
#endif
        {
            hci_send_cmd(&hci_le_set_scan_enable, 1, hci_stack->le_scan_filter_duplicates);
        }
        return true;
    }
#endif

#ifdef ENABLE_LE_CENTRAL
    // re-start connecting
    if ( (hci_stack->le_connecting_state == LE_CONNECTING_IDLE) && (hci_stack->le_connecting_request == LE_CONNECTING_WHITELIST)){
        bd_addr_t null_addr;
        memset(null_addr, 0, 6);
        hci_stack->le_connection_own_addr_type =  hci_stack->le_own_addr_type;
        hci_get_own_address_for_addr_type(hci_stack->le_connection_own_addr_type, hci_stack->le_connection_own_address);
        hci_send_le_create_connection(1, 0, null_addr);
        return true;
    }
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    if (hci_stack->le_periodic_sync_state == LE_CONNECTING_IDLE){
        switch(hci_stack->le_periodic_sync_request){
            case LE_CONNECTING_DIRECT:
            case LE_CONNECTING_WHITELIST:
                hci_stack->le_periodic_sync_state = ((hci_stack->le_periodic_sync_options & 1) != 0) ? LE_CONNECTING_WHITELIST : LE_CONNECTING_DIRECT;
                hci_send_cmd(&hci_le_periodic_advertising_create_sync,
                             hci_stack->le_periodic_sync_options,
                             hci_stack->le_periodic_sync_advertising_sid,
                             hci_stack->le_periodic_sync_advertiser_address_type,
                             hci_stack->le_periodic_sync_advertiser_address,
                             hci_stack->le_periodic_sync_skip,
                             hci_stack->le_periodic_sync_timeout,
                             hci_stack->le_periodic_sync_cte_type);
                return true;
            default:
                break;
        }
    }
#endif
#endif

#ifdef ENABLE_LE_PERIPHERAL
    // re-start advertising
    if (hci_stack->le_advertisements_enabled_for_current_roles && ((hci_stack->le_advertisements_state & LE_ADVERTISEMENT_STATE_ACTIVE) == 0)){
        // check if advertisements should be enabled given
        hci_stack->le_advertisements_state |= LE_ADVERTISEMENT_STATE_ACTIVE;
        hci_get_own_address_for_addr_type(hci_stack->le_advertisements_own_addr_type, hci_stack->le_advertisements_own_address);

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
        if (hci_extended_advertising_supported()){
            const uint8_t advertising_handles[] = { 0 };
            const uint16_t durations[] = { 0 };
            const uint16_t max_events[] = { 0 };
            hci_send_cmd(&hci_le_set_extended_advertising_enable, 1, 1, advertising_handles, durations, max_events);
        } else
#endif
        {
            hci_send_cmd(&hci_le_set_advertise_enable, 1);
        }
        return true;
    }

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    if (hci_extended_advertising_supported()) {
        btstack_linked_list_iterator_t it;
        btstack_linked_list_iterator_init(&it, &hci_stack->le_advertising_sets);
        while (btstack_linked_list_iterator_has_next(&it)) {
            le_advertising_set_t *advertising_set = (le_advertising_set_t *) btstack_linked_list_iterator_next(&it);
            if (((advertising_set->state & LE_ADVERTISEMENT_STATE_ENABLED) != 0) && ((advertising_set->state & LE_ADVERTISEMENT_STATE_ACTIVE) == 0)){
                advertising_set->state |= LE_ADVERTISEMENT_STATE_ACTIVE;
                const uint8_t advertising_handles[] = { advertising_set->advertising_handle };
                const uint16_t durations[] = { advertising_set->enable_timeout };
                const uint16_t max_events[] = { advertising_set->enable_max_scan_events };
                hci_send_cmd(&hci_le_set_extended_advertising_enable, 1, 1, advertising_handles, durations, max_events);
                return true;
            }
#ifdef ENABLE_LE_PERIODIC_ADVERTISING
            if (((advertising_set->state & LE_ADVERTISEMENT_STATE_PERIODIC_ENABLED) != 0) && ((advertising_set->state & LE_ADVERTISEMENT_STATE_PERIODIC_ACTIVE) == 0)){
                advertising_set->state |= LE_ADVERTISEMENT_STATE_PERIODIC_ACTIVE;
                uint8_t enable = 1;
                if (advertising_set->periodic_include_adi){
                    enable |= 2;
                }
                hci_send_cmd(&hci_le_set_periodic_advertising_enable, enable, advertising_set->advertising_handle);
                return true;
            }
#endif /* ENABLE_LE_PERIODIC_ADVERTISING */
        }
    }
#endif
#endif

    return false;
}

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
static bool hci_run_iso_tasks(void){
    btstack_linked_list_iterator_t it;

    if (hci_stack->iso_active_operation_type != HCI_ISO_TYPE_INVALID) {
        return false;
    }

    // BIG
    btstack_linked_list_iterator_init(&it, &hci_stack->le_audio_bigs);
    while (btstack_linked_list_iterator_has_next(&it)){
        le_audio_big_t * big = (le_audio_big_t *) btstack_linked_list_iterator_next(&it);
        switch (big->state){
            case LE_AUDIO_BIG_STATE_CREATE:
                hci_stack->iso_active_operation_group_id = big->params->big_handle;
                hci_stack->iso_active_operation_type = HCI_ISO_TYPE_BIS;
                big->state = LE_AUDIO_BIG_STATE_W4_ESTABLISHED;
                hci_send_cmd(&hci_le_create_big,
                             big->params->big_handle,
                             big->params->advertising_handle,
                             big->params->num_bis,
                             big->params->sdu_interval_us,
                             big->params->max_sdu,
                             big->params->max_transport_latency_ms,
                             big->params->rtn,
                             big->params->phy,
                             big->params->packing,
                             big->params->framing,
                             big->params->encryption,
                             big->params->broadcast_code);
                return true;
            case LE_AUDIO_BIG_STATE_SETUP_ISO_PATH:
                big->state = LE_AUDIO_BIG_STATE_W4_SETUP_ISO_PATH;
                hci_send_cmd(&hci_le_setup_iso_data_path, big->bis_con_handles[big->state_vars.next_bis], 0, 0,  0, 0, 0,  0, 0, NULL);
                return true;
            case LE_AUDIO_BIG_STATE_SETUP_ISO_PATHS_FAILED:
                big->state = LE_AUDIO_BIG_STATE_W4_TERMINATED_AFTER_SETUP_FAILED;
                hci_send_cmd(&hci_le_terminate_big, big->big_handle, big->state_vars.status);
                return true;
            case LE_AUDIO_BIG_STATE_TERMINATE:
                big->state = LE_AUDIO_BIG_STATE_W4_TERMINATED;
                hci_send_cmd(&hci_le_terminate_big, big->big_handle, ERROR_CODE_SUCCESS);
                return true;
            default:
                break;
        }
    }

    // BIG Sync
    btstack_linked_list_iterator_init(&it, &hci_stack->le_audio_big_syncs);
    while (btstack_linked_list_iterator_has_next(&it)){
        le_audio_big_sync_t * big_sync = (le_audio_big_sync_t *) btstack_linked_list_iterator_next(&it);
        switch (big_sync->state){
            case LE_AUDIO_BIG_STATE_CREATE:
                hci_stack->iso_active_operation_group_id = big_sync->params->big_handle;
                hci_stack->iso_active_operation_type = HCI_ISO_TYPE_BIS;
                big_sync->state = LE_AUDIO_BIG_STATE_W4_ESTABLISHED;
                hci_send_cmd(&hci_le_big_create_sync,
                             big_sync->params->big_handle,
                             big_sync->params->sync_handle,
                             big_sync->params->encryption,
                             big_sync->params->broadcast_code,
                             big_sync->params->mse,
                             big_sync->params->big_sync_timeout_10ms,
                             big_sync->params->num_bis,
                             big_sync->params->bis_indices);
                return true;
            case LE_AUDIO_BIG_STATE_SETUP_ISO_PATH:
                big_sync->state = LE_AUDIO_BIG_STATE_W4_SETUP_ISO_PATH;
                hci_send_cmd(&hci_le_setup_iso_data_path, big_sync->bis_con_handles[big_sync->state_vars.next_bis], 1, 0, 0, 0, 0, 0, 0, NULL);
                return true;
            case LE_AUDIO_BIG_STATE_SETUP_ISO_PATHS_FAILED:
                big_sync->state = LE_AUDIO_BIG_STATE_W4_TERMINATED_AFTER_SETUP_FAILED;
                hci_send_cmd(&hci_le_big_terminate_sync, big_sync->big_handle);
                return true;
            case LE_AUDIO_BIG_STATE_TERMINATE:
                big_sync->state = LE_AUDIO_BIG_STATE_W4_TERMINATED;
                hci_send_cmd(&hci_le_big_terminate_sync, big_sync->big_handle);
                return true;
            default:
                break;
        }
    }

    // CIG
    btstack_linked_list_iterator_init(&it, &hci_stack->le_audio_cigs);
    while (btstack_linked_list_iterator_has_next(&it)) {
        le_audio_cig_t *cig = (le_audio_cig_t *) btstack_linked_list_iterator_next(&it);
        uint8_t i;
        // Set CIG Parameters
        uint8_t cis_id[MAX_NR_CIS];
        uint16_t max_sdu_c_to_p[MAX_NR_CIS];
        uint16_t max_sdu_p_to_c[MAX_NR_CIS];
        uint8_t phy_c_to_p[MAX_NR_CIS];
        uint8_t phy_p_to_c[MAX_NR_CIS];
        uint8_t rtn_c_to_p[MAX_NR_CIS];
        uint8_t rtn_p_to_c[MAX_NR_CIS];
        switch (cig->state) {
            case LE_AUDIO_CIG_STATE_CREATE:
                hci_stack->iso_active_operation_group_id = cig->params->cig_id;
                hci_stack->iso_active_operation_type = HCI_ISO_TYPE_CIS;
                cig->state = LE_AUDIO_CIG_STATE_W4_ESTABLISHED;
                le_audio_cig_params_t * params = cig->params;
                for (i = 0; i < params->num_cis; i++) {
                    le_audio_cis_params_t * cis_params = &cig->params->cis_params[i];
                    cis_id[i]         = cis_params->cis_id;
                    max_sdu_c_to_p[i] = cis_params->max_sdu_c_to_p;
                    max_sdu_p_to_c[i] = cis_params->max_sdu_p_to_c;
                    phy_c_to_p[i]     = cis_params->phy_c_to_p;
                    phy_p_to_c[i]     = cis_params->phy_p_to_c;
                    rtn_c_to_p[i]     = cis_params->rtn_c_to_p;
                    rtn_p_to_c[i]     = cis_params->rtn_p_to_c;
                }
                hci_send_cmd(&hci_le_set_cig_parameters,
                             cig->cig_id,
                             params->sdu_interval_c_to_p,
                             params->sdu_interval_p_to_c,
                             params->worst_case_sca,
                             params->packing,
                             params->framing,
                             params->max_transport_latency_c_to_p,
                             params->max_transport_latency_p_to_c,
                             params->num_cis,
                             cis_id,
                             max_sdu_c_to_p,
                             max_sdu_p_to_c,
                             phy_c_to_p,
                             phy_p_to_c,
                             rtn_c_to_p,
                             rtn_p_to_c
                );
                return true;
            case LE_AUDIO_CIG_STATE_CREATE_CIS:
                hci_stack->iso_active_operation_group_id = cig->params->cig_id;
                hci_stack->iso_active_operation_type = HCI_ISO_TYPE_CIS;
                cig->state = LE_AUDIO_CIG_STATE_W4_CREATE_CIS;
                for (i=0;i<cig->num_cis;i++){
                    cig->cis_setup_active[i] = true;
                }
                hci_send_cmd(&hci_le_create_cis, cig->num_cis, cig->cis_con_handles, cig->acl_con_handles);
                return true;
            case LE_AUDIO_CIG_STATE_SETUP_ISO_PATH:
                while (cig->state_vars.next_cis < (cig->num_cis * 2)){
                    // find next path to setup
                    uint8_t cis_index = cig->state_vars.next_cis >> 1;
                    if (cig->cis_established[cis_index] == false) {
                        continue;
                    }
                    uint8_t cis_direction = cig->state_vars.next_cis & 1;
                    bool setup = true;
                    if (cis_direction == 0){
                        // 0 - input - host to controller
                        // we are central => central to peripheral
                        setup &= cig->params->cis_params[cis_index].max_sdu_c_to_p > 0;
                    } else {
                        // 1 - output - controller to host
                        // we are central => peripheral to central
                        setup &= cig->params->cis_params[cis_index].max_sdu_p_to_c > 0;
                    }
                    if (setup){
                        hci_stack->iso_active_operation_group_id = cig->params->cig_id;
                        hci_stack->iso_active_operation_type = HCI_ISO_TYPE_CIS;
                        cig->state = LE_AUDIO_CIG_STATE_W4_SETUP_ISO_PATH;
                        hci_send_cmd(&hci_le_setup_iso_data_path, cig->cis_con_handles[cis_index], cis_direction, 0, 0, 0, 0, 0, 0, NULL);
                        return true;
                    }
                    cig->state_vars.next_cis++;
                }
                // emit done
                cig->state = LE_AUDIO_CIG_STATE_ACTIVE;
            default:
                break;
        }
    }

    // CIS Accept/Reject
    btstack_linked_list_iterator_init(&it, &hci_stack->iso_streams);
    while (btstack_linked_list_iterator_has_next(&it)) {
        hci_iso_stream_t *iso_stream = (hci_iso_stream_t *) btstack_linked_list_iterator_next(&it);
        hci_con_handle_t con_handle;
        switch (iso_stream->state){
            case HCI_ISO_STREAM_W2_ACCEPT:
                iso_stream->state = HCI_ISO_STREAM_STATE_W4_ESTABLISHED;
                hci_stack->iso_active_operation_type = HCI_ISO_TYPE_CIS;
                hci_stack->iso_active_operation_group_id = HCI_ISO_GROUP_ID_SINGLE_CIS;
                hci_send_cmd(&hci_le_accept_cis_request, iso_stream->cis_handle);
                return true;
            case HCI_ISO_STREAM_W2_REJECT:
                con_handle = iso_stream->cis_handle;
                hci_stack->iso_active_operation_type = HCI_ISO_TYPE_CIS;
                hci_stack->iso_active_operation_group_id = HCI_ISO_GROUP_ID_SINGLE_CIS;
                hci_iso_stream_finalize(iso_stream);
                hci_send_cmd(&hci_le_reject_cis_request, con_handle, ERROR_CODE_REMOTE_DEVICE_TERMINATED_CONNECTION_DUE_TO_LOW_RESOURCES);
                return true;
            case HCI_ISO_STREAM_STATE_W2_SETUP_ISO_INPUT:
                hci_stack->iso_active_operation_group_id = HCI_ISO_GROUP_ID_SINGLE_CIS;
                hci_stack->iso_active_operation_type = HCI_ISO_TYPE_CIS;
                iso_stream->state = HCI_ISO_STREAM_STATE_W4_ISO_SETUP_INPUT;
                hci_send_cmd(&hci_le_setup_iso_data_path, iso_stream->cis_handle, 0, 0, 0, 0, 0, 0, 0, NULL);
                break;
            case HCI_ISO_STREAM_STATE_W2_SETUP_ISO_OUTPUT:
                hci_stack->iso_active_operation_group_id = HCI_ISO_GROUP_ID_SINGLE_CIS;
                hci_stack->iso_active_operation_type = HCI_ISO_TYPE_CIS;
                iso_stream->state = HCI_ISO_STREAM_STATE_W4_ISO_SETUP_OUTPUT;
                hci_send_cmd(&hci_le_setup_iso_data_path, iso_stream->cis_handle, 1, 0, 0, 0, 0, 0, 0, NULL);
                break;
            default:
                break;
        }
    }

    return false;
}
#endif /* ENABLE_LE_ISOCHRONOUS_STREAMS */
#endif

static bool hci_run_general_pending_commands(void){
    btstack_linked_item_t * it;
    for (it = (btstack_linked_item_t *) hci_stack->connections; it != NULL; it = it->next){
        hci_connection_t * connection = (hci_connection_t *) it;

        switch(connection->state){
            case SEND_CREATE_CONNECTION:
                switch(connection->address_type){
#ifdef ENABLE_CLASSIC
                    case BD_ADDR_TYPE_ACL:
                        log_info("sending hci_create_connection");
                        hci_send_cmd(&hci_create_connection, connection->address, hci_usable_acl_packet_types(), 0, 0, 0, hci_stack->allow_role_switch);
                        break;
#endif
                    default:
#ifdef ENABLE_BLE
#ifdef ENABLE_LE_CENTRAL
                        log_info("sending hci_le_create_connection");
                        hci_stack->le_connection_own_addr_type =  hci_stack->le_own_addr_type;
                        hci_get_own_address_for_addr_type(hci_stack->le_connection_own_addr_type, hci_stack->le_connection_own_address);
                        hci_send_le_create_connection(0, connection->address_type, connection->address);
                        connection->state = SENT_CREATE_CONNECTION;
#endif
#endif
                        break;
                }
                return true;

#ifdef ENABLE_CLASSIC
            case RECEIVED_CONNECTION_REQUEST:
                if (connection->address_type == BD_ADDR_TYPE_ACL){
                    log_info("sending hci_accept_connection_request");
                    connection->state = ACCEPTED_CONNECTION_REQUEST;
                    hci_send_cmd(&hci_accept_connection_request, connection->address, hci_stack->master_slave_policy);
                    return true;
                }
                break;
#endif
            case SEND_DISCONNECT:
                connection->state = SENT_DISCONNECT;
                hci_send_cmd(&hci_disconnect, connection->con_handle, ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION);
                return true;

            default:
                break;
        }

        // no further commands if connection is about to get shut down
        if (connection->state == SENT_DISCONNECT) continue;

#ifdef ENABLE_CLASSIC

        // Handling link key request requires remote supported features
        if (((connection->authentication_flags & AUTH_FLAG_HANDLE_LINK_KEY_REQUEST) != 0)){
            log_info("responding to link key request, have link key db: %u", hci_stack->link_key_db != NULL);
            connectionClearAuthenticationFlags(connection, AUTH_FLAG_HANDLE_LINK_KEY_REQUEST);

            bool have_link_key = connection->link_key_type != INVALID_LINK_KEY;
            bool security_level_sufficient = have_link_key && (gap_security_level_for_link_key_type(connection->link_key_type) >= connection->requested_security_level);
            if (have_link_key && security_level_sufficient){
                hci_send_cmd(&hci_link_key_request_reply, connection->address, &connection->link_key);
            } else {
                hci_send_cmd(&hci_link_key_request_negative_reply, connection->address);
            }
            return true;
        }

        if (connection->authentication_flags & AUTH_FLAG_DENY_PIN_CODE_REQUEST){
            log_info("denying to pin request");
            connectionClearAuthenticationFlags(connection, AUTH_FLAG_DENY_PIN_CODE_REQUEST);
            hci_send_cmd(&hci_pin_code_request_negative_reply, connection->address);
            return true;
        }

        // security assessment requires remote features
        if ((connection->authentication_flags & AUTH_FLAG_RECV_IO_CAPABILITIES_REQUEST) != 0){
            connectionClearAuthenticationFlags(connection, AUTH_FLAG_RECV_IO_CAPABILITIES_REQUEST);
            hci_ssp_assess_security_on_io_cap_request(connection);
            // no return here as hci_ssp_assess_security_on_io_cap_request only sets AUTH_FLAG_SEND_IO_CAPABILITIES_REPLY or AUTH_FLAG_SEND_IO_CAPABILITIES_NEGATIVE_REPLY
        }

        if (connection->authentication_flags & AUTH_FLAG_SEND_IO_CAPABILITIES_REPLY){
            connectionClearAuthenticationFlags(connection, AUTH_FLAG_SEND_IO_CAPABILITIES_REPLY);
            // set authentication requirements:
            // - MITM = ssp_authentication_requirement (USER) | requested_security_level (dynamic)
            // - BONDING MODE: dedicated if requested, bondable otherwise. Drop bondable if not set for remote
            uint8_t authreq = hci_stack->ssp_authentication_requirement & 1;
            if (gap_mitm_protection_required_for_security_level(connection->requested_security_level)){
                authreq |= 1;
            }
            bool bonding = hci_stack->bondable;
            if (connection->authentication_flags & AUTH_FLAG_RECV_IO_CAPABILITIES_RESPONSE){
                // if we have received IO Cap Response, we're in responder role
                bool remote_bonding = connection->io_cap_response_auth_req >= SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_DEDICATED_BONDING;
                if (bonding && !remote_bonding){
                    log_info("Remote not bonding, dropping local flag");
                    bonding = false;
                }
            }
            if (bonding){
                if (connection->bonding_flags & BONDING_DEDICATED){
                    authreq |= SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_DEDICATED_BONDING;
                } else {
                    authreq |= SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_GENERAL_BONDING;
                }
            }
            uint8_t have_oob_data = 0;
#ifdef ENABLE_CLASSIC_PAIRING_OOB
            if (connection->classic_oob_c_192 != NULL){
                    have_oob_data |= 1;
            }
            if (connection->classic_oob_c_256 != NULL){
                have_oob_data |= 2;
            }
#endif
            hci_send_cmd(&hci_io_capability_request_reply, &connection->address, hci_stack->ssp_io_capability, have_oob_data, authreq);
            return true;
        }

        if (connection->authentication_flags & AUTH_FLAG_SEND_IO_CAPABILITIES_NEGATIVE_REPLY) {
            connectionClearAuthenticationFlags(connection, AUTH_FLAG_SEND_IO_CAPABILITIES_NEGATIVE_REPLY);
            hci_send_cmd(&hci_io_capability_request_negative_reply, &connection->address, ERROR_CODE_PAIRING_NOT_ALLOWED);
            return true;
        }

#ifdef ENABLE_CLASSIC_PAIRING_OOB
        if (connection->authentication_flags & AUTH_FLAG_SEND_REMOTE_OOB_DATA_REPLY){
            connectionClearAuthenticationFlags(connection, AUTH_FLAG_SEND_REMOTE_OOB_DATA_REPLY);
            const uint8_t zero[16] = { 0 };
            const uint8_t * r_192 = zero;
            const uint8_t * c_192 = zero;
            const uint8_t * r_256 = zero;
            const uint8_t * c_256 = zero;
            // verify P-256 OOB
            if ((connection->classic_oob_c_256 != NULL) && hci_command_supported(SUPPORTED_HCI_COMMAND_REMOTE_OOB_EXTENDED_DATA_REQUEST_REPLY)) {
                c_256 = connection->classic_oob_c_256;
                if (connection->classic_oob_r_256 != NULL) {
                    r_256 = connection->classic_oob_r_256;
                }
            }
            // verify P-192 OOB
            if ((connection->classic_oob_c_192 != NULL)) {
                c_192 = connection->classic_oob_c_192;
                if (connection->classic_oob_r_192 != NULL) {
                    r_192 = connection->classic_oob_r_192;
                }
            }

            // assess security
            bool need_level_4 = hci_stack->gap_secure_connections_only_mode || (connection->requested_security_level == LEVEL_4);
            bool can_reach_level_4 = hci_remote_sc_enabled(connection) && (c_256 != NULL);
            if (need_level_4 && !can_reach_level_4){
                log_info("Level 4 required, but not possible -> abort");
                hci_pairing_complete(connection, ERROR_CODE_INSUFFICIENT_SECURITY);
                // send oob negative reply
                c_256 = NULL;
                c_192 = NULL;
            }

            // Reply
            if (c_256 != zero) {
                hci_send_cmd(&hci_remote_oob_extended_data_request_reply, &connection->address, c_192, r_192, c_256, r_256);
            } else if (c_192 != zero){
                hci_send_cmd(&hci_remote_oob_data_request_reply, &connection->address, c_192, r_192);
            } else {
                hci_stack->classic_oob_con_handle = connection->con_handle;
                hci_send_cmd(&hci_remote_oob_data_request_negative_reply, &connection->address);
            }
            return true;
        }
#endif

        if (connection->authentication_flags & AUTH_FLAG_SEND_USER_CONFIRM_REPLY){
            connectionClearAuthenticationFlags(connection, AUTH_FLAG_SEND_USER_CONFIRM_REPLY);
            hci_send_cmd(&hci_user_confirmation_request_reply, &connection->address);
            return true;
        }

        if (connection->authentication_flags & AUTH_FLAG_SEND_USER_CONFIRM_NEGATIVE_REPLY){
            connectionClearAuthenticationFlags(connection, AUTH_FLAG_SEND_USER_CONFIRM_NEGATIVE_REPLY);
            hci_send_cmd(&hci_user_confirmation_request_negative_reply, &connection->address);
            return true;
        }

        if (connection->authentication_flags & AUTH_FLAG_SEND_USER_PASSKEY_REPLY){
            connectionClearAuthenticationFlags(connection, AUTH_FLAG_SEND_USER_PASSKEY_REPLY);
            hci_send_cmd(&hci_user_passkey_request_reply, &connection->address, 000000);
            return true;
        }

        if ((connection->bonding_flags & (BONDING_DISCONNECT_DEDICATED_DONE | BONDING_DEDICATED_DEFER_DISCONNECT)) == BONDING_DISCONNECT_DEDICATED_DONE){
            connection->bonding_flags &= ~BONDING_DISCONNECT_DEDICATED_DONE;
            connection->bonding_flags |= BONDING_EMIT_COMPLETE_ON_DISCONNECT;
            connection->state = SENT_DISCONNECT;
            hci_send_cmd(&hci_disconnect, connection->con_handle, ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION);
            return true;
        }

        if ((connection->bonding_flags & BONDING_SEND_AUTHENTICATE_REQUEST) && ((connection->bonding_flags & BONDING_RECEIVED_REMOTE_FEATURES) != 0)){
            connection->bonding_flags &= ~BONDING_SEND_AUTHENTICATE_REQUEST;
            connection->bonding_flags |= BONDING_SENT_AUTHENTICATE_REQUEST;
            hci_send_cmd(&hci_authentication_requested, connection->con_handle);
            return true;
        }

        if (connection->bonding_flags & BONDING_SEND_ENCRYPTION_REQUEST){
            connection->bonding_flags &= ~BONDING_SEND_ENCRYPTION_REQUEST;
            hci_send_cmd(&hci_set_connection_encryption, connection->con_handle, 1);
            return true;
        }

        if (connection->bonding_flags & BONDING_SEND_READ_ENCRYPTION_KEY_SIZE){
            connection->bonding_flags &= ~BONDING_SEND_READ_ENCRYPTION_KEY_SIZE;
            hci_send_cmd(&hci_read_encryption_key_size, connection->con_handle, 1);
            return true;
        }

        if (connection->bonding_flags & BONDING_REQUEST_REMOTE_FEATURES_PAGE_0){
            connection->bonding_flags &= ~BONDING_REQUEST_REMOTE_FEATURES_PAGE_0;
            hci_send_cmd(&hci_read_remote_supported_features_command, connection->con_handle);
            return true;
        }

        if (connection->bonding_flags & BONDING_REQUEST_REMOTE_FEATURES_PAGE_1){
            connection->bonding_flags &= ~BONDING_REQUEST_REMOTE_FEATURES_PAGE_1;
            hci_send_cmd(&hci_read_remote_extended_features_command, connection->con_handle, 1);
            return true;
        }

        if (connection->bonding_flags & BONDING_REQUEST_REMOTE_FEATURES_PAGE_2){
            connection->bonding_flags &= ~BONDING_REQUEST_REMOTE_FEATURES_PAGE_2;
            hci_send_cmd(&hci_read_remote_extended_features_command, connection->con_handle, 2);
            return true;
        }
#endif

        if (connection->bonding_flags & BONDING_DISCONNECT_SECURITY_BLOCK){
            connection->bonding_flags &= ~BONDING_DISCONNECT_SECURITY_BLOCK;
#ifdef ENABLE_CLASSIC
            hci_pairing_complete(connection, ERROR_CODE_CONNECTION_REJECTED_DUE_TO_SECURITY_REASONS);
#endif
            if (connection->state != SENT_DISCONNECT){
                connection->state = SENT_DISCONNECT;
                hci_send_cmd(&hci_disconnect, connection->con_handle, ERROR_CODE_AUTHENTICATION_FAILURE);
                return true;
            }
        }

#ifdef ENABLE_CLASSIC
        uint16_t sniff_min_interval;
        switch (connection->sniff_min_interval){
            case 0:
                break;
            case 0xffff:
                connection->sniff_min_interval = 0;
                hci_send_cmd(&hci_exit_sniff_mode, connection->con_handle);
                return true;
            default:
                sniff_min_interval = connection->sniff_min_interval;
                connection->sniff_min_interval = 0;
                hci_send_cmd(&hci_sniff_mode, connection->con_handle, connection->sniff_max_interval, sniff_min_interval, connection->sniff_attempt, connection->sniff_timeout);
                return true;
        }

        if (connection->sniff_subrating_max_latency != 0xffff){
            uint16_t max_latency = connection->sniff_subrating_max_latency;
            connection->sniff_subrating_max_latency = 0;
            hci_send_cmd(&hci_sniff_subrating, connection->con_handle, max_latency, connection->sniff_subrating_min_remote_timeout, connection->sniff_subrating_min_local_timeout);
            return true;
        }

        if (connection->qos_service_type != HCI_SERVICE_TYPE_INVALID){
            uint8_t service_type = (uint8_t) connection->qos_service_type;
            connection->qos_service_type = HCI_SERVICE_TYPE_INVALID;
            hci_send_cmd(&hci_qos_setup, connection->con_handle, 0, service_type, connection->qos_token_rate, connection->qos_peak_bandwidth, connection->qos_latency, connection->qos_delay_variation);
            return true;
        }

        if (connection->request_role != HCI_ROLE_INVALID){
            hci_role_t role = connection->request_role;
            connection->request_role = HCI_ROLE_INVALID;
            hci_send_cmd(&hci_switch_role_command, connection->address, role);
            return true;
        }
#endif

        if (connection->gap_connection_tasks != 0){
#ifdef ENABLE_CLASSIC
            if ((connection->gap_connection_tasks & GAP_CONNECTION_TASK_WRITE_AUTOMATIC_FLUSH_TIMEOUT) != 0){
                connection->gap_connection_tasks &= ~GAP_CONNECTION_TASK_WRITE_AUTOMATIC_FLUSH_TIMEOUT;
                hci_send_cmd(&hci_write_automatic_flush_timeout, connection->con_handle, hci_stack->automatic_flush_timeout);
                return true;
            }
            if (connection->gap_connection_tasks & GAP_CONNECTION_TASK_WRITE_SUPERVISION_TIMEOUT){
                connection->gap_connection_tasks &= ~GAP_CONNECTION_TASK_WRITE_SUPERVISION_TIMEOUT;
                hci_send_cmd(&hci_write_link_supervision_timeout, connection->con_handle, hci_stack->link_supervision_timeout);
                return true;
            }
#endif
            if (connection->gap_connection_tasks & GAP_CONNECTION_TASK_READ_RSSI){
                connection->gap_connection_tasks &= ~GAP_CONNECTION_TASK_READ_RSSI;
                hci_send_cmd(&hci_read_rssi, connection->con_handle);
                return true;
            }
#ifdef ENABLE_BLE
            if (connection->gap_connection_tasks & GAP_CONNECTION_TASK_LE_READ_REMOTE_FEATURES){
                connection->gap_connection_tasks &= ~GAP_CONNECTION_TASK_LE_READ_REMOTE_FEATURES;
                hci_send_cmd(&hci_le_read_remote_used_features, connection->con_handle);
                return true;
            }
#endif
        }

#ifdef ENABLE_BLE
        switch (connection->le_con_parameter_update_state){
            // response to L2CAP CON PARAMETER UPDATE REQUEST
            case CON_PARAMETER_UPDATE_CHANGE_HCI_CON_PARAMETERS:
                connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_NONE;
                hci_send_cmd(&hci_le_connection_update, connection->con_handle, connection->le_conn_interval_min,
                             connection->le_conn_interval_max, connection->le_conn_latency, connection->le_supervision_timeout,
                             hci_stack->le_minimum_ce_length, hci_stack->le_maximum_ce_length);
                return true;
            case CON_PARAMETER_UPDATE_REPLY:
                connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_NONE;
                hci_send_cmd(&hci_le_remote_connection_parameter_request_reply, connection->con_handle, connection->le_conn_interval_min,
                             connection->le_conn_interval_max, connection->le_conn_latency, connection->le_supervision_timeout,
                             hci_stack->le_minimum_ce_length, hci_stack->le_maximum_ce_length);
                return true;
            case CON_PARAMETER_UPDATE_NEGATIVE_REPLY:
                connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_NONE;
                hci_send_cmd(&hci_le_remote_connection_parameter_request_negative_reply, connection->con_handle,
                             ERROR_CODE_UNACCEPTABLE_CONNECTION_PARAMETERS);
                return true;
            default:
                break;
        }
        if (connection->le_phy_update_all_phys != 0xffu){
            uint8_t all_phys = connection->le_phy_update_all_phys;
            connection->le_phy_update_all_phys = 0xff;
            hci_send_cmd(&hci_le_set_phy, connection->con_handle, all_phys, connection->le_phy_update_tx_phys, connection->le_phy_update_rx_phys, connection->le_phy_update_phy_options);
            return true;
        }
#ifdef ENABLE_LE_PERIODIC_ADVERTISING
        if (connection->le_past_sync_handle != HCI_CON_HANDLE_INVALID){
            hci_con_handle_t sync_handle = connection->le_past_sync_handle;
            connection->le_past_sync_handle = HCI_CON_HANDLE_INVALID;
            hci_send_cmd(&hci_le_periodic_advertising_sync_transfer, connection->con_handle, connection->le_past_service_data, sync_handle);
            return true;
        }
#endif
#endif
    }
    return false;
}

static void hci_run(void){

    // stack state sub statemachines
    switch (hci_stack->state) {
        case HCI_STATE_INITIALIZING:
            hci_initializing_run();
            break;
        case HCI_STATE_HALTING:
            hci_halting_run();
            break;
        case HCI_STATE_FALLING_ASLEEP:
            hci_falling_asleep_run();
            break;
        default:
            break;
    }

    // allow to run after initialization to working transition
    if (hci_stack->state != HCI_STATE_WORKING){
        return;
    }

    bool done;

    // send continuation fragments first, as they block the prepared packet buffer
    done = hci_run_acl_fragments();
    if (done) return;

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
    done = hci_run_iso_fragments();
    if (done) return;
#endif

#ifdef ENABLE_HCI_CONTROLLER_TO_HOST_FLOW_CONTROL
    // send host num completed packets next as they don't require num_cmd_packets > 0
    if (!hci_can_send_comand_packet_transport()) return;
    if (hci_stack->host_completed_packets){
        hci_host_num_completed_packets();        
        return;
    }
#endif

    if (!hci_can_send_command_packet_now()) return;

    // global/non-connection oriented commands


#ifdef ENABLE_CLASSIC
    // general gap classic
    done = hci_run_general_gap_classic();
    if (done) return;
#endif

#ifdef ENABLE_BLE
    // general gap le
    done = hci_run_general_gap_le();
    if (done) return;

#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
    // ISO related tasks, e.g. BIG create/terminate/sync
    done = hci_run_iso_tasks();
    if (done) return;
#endif
#endif

    // send pending HCI commands
    hci_run_general_pending_commands();
}

uint8_t hci_send_cmd_packet(uint8_t *packet, int size){
    // house-keeping
    
#ifdef ENABLE_CLASSIC
    bd_addr_t addr;
    hci_connection_t * conn;
#endif
#ifdef ENABLE_LE_CENTRAL
    uint8_t initiator_filter_policy;
#endif

    uint16_t opcode = little_endian_read_16(packet, 0);
    switch (opcode) {
        case HCI_OPCODE_HCI_WRITE_LOOPBACK_MODE:
            hci_stack->loopback_mode = packet[3];
            break;

#ifdef ENABLE_CLASSIC
        case HCI_OPCODE_HCI_CREATE_CONNECTION:
            reverse_bd_addr(&packet[3], addr);
            log_info("Create_connection to %s", bd_addr_to_str(addr));

            // CVE-2020-26555: reject outgoing connection to device with same BD ADDR
            if (memcmp(hci_stack->local_bd_addr, addr, 6) == 0) {
                hci_emit_connection_complete(addr, 0, ERROR_CODE_CONNECTION_REJECTED_DUE_TO_UNACCEPTABLE_BD_ADDR);
                return ERROR_CODE_CONNECTION_REJECTED_DUE_TO_UNACCEPTABLE_BD_ADDR;
            }

            conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
            if (!conn) {
                conn = create_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL, HCI_ROLE_MASTER);
                if (!conn) {
                    // notify client that alloc failed
                    hci_emit_connection_complete(addr, 0, BTSTACK_MEMORY_ALLOC_FAILED);
                    return BTSTACK_MEMORY_ALLOC_FAILED; // packet not sent to controller
                }
                conn->state = SEND_CREATE_CONNECTION;
            }

            log_info("conn state %u", conn->state);
            // TODO: L2CAP should not send create connection command, instead a (new) gap function should be used
            switch (conn->state) {
                // if connection active exists
                case OPEN:
                    // and OPEN, emit connection complete command
                    hci_emit_connection_complete(addr, conn->con_handle, ERROR_CODE_SUCCESS);
                    // packet not sent to controller
                    return ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS;
                case RECEIVED_DISCONNECTION_COMPLETE:
                    // create connection triggered in disconnect complete event, let's do it now
                    break;
                case SEND_CREATE_CONNECTION:
#ifdef ENABLE_HCI_SERIALIZED_CONTROLLER_OPERATIONS
                    if (hci_classic_operation_active()){
                        return ERROR_CODE_SUCCESS;
                    }
#endif
                    // connection created by hci, e.g. dedicated bonding, but not executed yet, let's do it now
                    break;
                default:
                    // otherwise, just ignore as it is already in the open process
                    // packet not sent to controller
                    return ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS;
            }
            conn->state = SENT_CREATE_CONNECTION;

            // track outgoing connection
            hci_stack->outgoing_addr_type = BD_ADDR_TYPE_ACL;
            (void) memcpy(hci_stack->outgoing_addr, addr, 6);
            break;

        case HCI_OPCODE_HCI_SETUP_SYNCHRONOUS_CONNECTION:
            conn = hci_connection_for_handle(little_endian_read_16(packet, 3));
            if (conn == NULL) {
                // neither SCO nor ACL connection for con handle
                return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
            } else {
                switch (conn->address_type){
                    case BD_ADDR_TYPE_ACL:
                        // assert SCO connection does not exit
                        if (hci_connection_for_bd_addr_and_type(conn->address, BD_ADDR_TYPE_SCO) != NULL){
                            return ERROR_CODE_COMMAND_DISALLOWED;
                        }
                        // allocate connection struct
                        conn = create_connection_for_bd_addr_and_type(conn->address, BD_ADDR_TYPE_SCO,
                                                                      HCI_ROLE_MASTER);
                        if (!conn) {
                            return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
                        }
                        break;
                    case BD_ADDR_TYPE_SCO:
                        // update of existing SCO connection
                        break;
                    default:
                        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
                }
            }

            // conn refers to hci connection of type sco now

            conn->state = SENT_CREATE_CONNECTION;

            // track outgoing connection to handle command status with error
            hci_stack->outgoing_addr_type = BD_ADDR_TYPE_SCO;
            (void) memcpy(hci_stack->outgoing_addr, addr, 6);

            // setup_synchronous_connection? Voice setting at offset 22
            // TODO: compare to current setting if sco connection already active
            hci_stack->sco_voice_setting_active = little_endian_read_16(packet, 15);
            break;

        case HCI_OPCODE_HCI_ACCEPT_SYNCHRONOUS_CONNECTION:
            // get SCO connection
            reverse_bd_addr(&packet[3], addr);
            conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_SCO);
            if (conn == NULL){
                return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
            }

            conn->state = ACCEPTED_CONNECTION_REQUEST;

            // track outgoing connection to handle command status with error
            hci_stack->outgoing_addr_type = BD_ADDR_TYPE_SCO;
            (void) memcpy(hci_stack->outgoing_addr, addr, 6);

            // accept_synchronous_connection? Voice setting at offset 18
            // TODO: compare to current setting if sco connection already active
            hci_stack->sco_voice_setting_active = little_endian_read_16(packet, 19);
            break;
#endif

#ifdef ENABLE_BLE
#ifdef ENABLE_LE_CENTRAL
        case HCI_OPCODE_HCI_LE_CREATE_CONNECTION:
            // white list used?
            initiator_filter_policy = packet[7];
            switch (initiator_filter_policy) {
                case 0:
                    // whitelist not used
                    hci_stack->le_connecting_state = LE_CONNECTING_DIRECT;
                    break;
                case 1:
                    hci_stack->le_connecting_state = LE_CONNECTING_WHITELIST;
                    break;
                default:
                    log_error("Invalid initiator_filter_policy in LE Create Connection %u", initiator_filter_policy);
                    break;
            }
            // track outgoing connection
            hci_stack->outgoing_addr_type = (bd_addr_type_t) packet[8]; // peer address type
            reverse_bd_addr( &packet[9], hci_stack->outgoing_addr); // peer address
            break;
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
        case HCI_OPCODE_HCI_LE_EXTENDED_CREATE_CONNECTION:
            // white list used?
            initiator_filter_policy = packet[3];
            switch (initiator_filter_policy) {
                case 0:
                    // whitelist not used
                    hci_stack->le_connecting_state = LE_CONNECTING_DIRECT;
                    break;
                case 1:
                    hci_stack->le_connecting_state = LE_CONNECTING_WHITELIST;
                    break;
                default:
                    log_error("Invalid initiator_filter_policy in LE Create Connection %u", initiator_filter_policy);
                    break;
            }
            // track outgoing connection
            hci_stack->outgoing_addr_type = (bd_addr_type_t) packet[5]; // peer address type
            reverse_bd_addr( &packet[6], hci_stack->outgoing_addr); // peer address
            break;
#endif
        case HCI_OPCODE_HCI_LE_CREATE_CONNECTION_CANCEL:
            hci_stack->le_connecting_state = LE_CONNECTING_CANCEL;
            break;
#endif
#endif /* ENABLE_BLE */
        default:
            break;
    }

    hci_stack->num_cmd_packets--;

    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, packet, size);
    int err = hci_stack->hci_transport->send_packet(HCI_COMMAND_DATA_PACKET, packet, size);
    if (err != 0){
        return ERROR_CODE_HARDWARE_FAILURE;
    }
    return ERROR_CODE_SUCCESS;
}

// disconnect because of security block
void hci_disconnect_security_block(hci_con_handle_t con_handle){
    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection) return;
    connection->bonding_flags |= BONDING_DISCONNECT_SECURITY_BLOCK;
}


// Configure Secure Simple Pairing

#ifdef ENABLE_CLASSIC

// enable will enable SSP during init
void gap_ssp_set_enable(int enable){
    hci_stack->ssp_enable = enable;
}

static int hci_local_ssp_activated(void){
    return gap_ssp_supported() && hci_stack->ssp_enable;
}

// if set, BTstack will respond to io capability request using authentication requirement
void gap_ssp_set_io_capability(int io_capability){
    hci_stack->ssp_io_capability = io_capability;
}
void gap_ssp_set_authentication_requirement(int authentication_requirement){
    hci_stack->ssp_authentication_requirement = authentication_requirement;
}

// if set, BTstack will confirm a numberic comparion and enter '000000' if requested
void gap_ssp_set_auto_accept(int auto_accept){
    hci_stack->ssp_auto_accept = auto_accept;
}

void gap_secure_connections_enable(bool enable){
    hci_stack->secure_connections_enable = enable;
}
bool gap_secure_connections_active(void){
    return hci_stack->secure_connections_active;
}

#endif

// va_list part of hci_send_cmd
uint8_t hci_send_cmd_va_arg(const hci_cmd_t * cmd, va_list argptr){
    if (!hci_can_send_command_packet_now()){ 
        log_error("hci_send_cmd called but cannot send packet now");
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    // for HCI INITIALIZATION
    // log_info("hci_send_cmd: opcode %04x", cmd->opcode);
    hci_stack->last_cmd_opcode = cmd->opcode;

    hci_reserve_packet_buffer();
    uint8_t * packet = hci_stack->hci_packet_buffer;
    uint16_t size = hci_cmd_create_from_template(packet, cmd, argptr);
    uint8_t status = hci_send_cmd_packet(packet, size);

    // release packet buffer on error or for synchronous transport implementations
    if ((status != ERROR_CODE_SUCCESS) || hci_transport_synchronous()){
        hci_release_packet_buffer();
        hci_emit_transport_packet_sent();
    }

    return status;
}

/**
 * pre: numcmds >= 0 - it's allowed to send a command to the controller
 */
uint8_t hci_send_cmd(const hci_cmd_t * cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint8_t status = hci_send_cmd_va_arg(cmd, argptr);
    va_end(argptr);
    return status;
}

// Create various non-HCI events. 
// TODO: generalize, use table similar to hci_create_command

static void hci_emit_event(uint8_t * event, uint16_t size, int dump){
    // dump packet
    if (dump) {
        hci_dump_packet( HCI_EVENT_PACKET, 1, event, size);
    } 

    // dispatch to all event handlers
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->event_handlers);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_packet_callback_registration_t * entry = (btstack_packet_callback_registration_t*) btstack_linked_list_iterator_next(&it);
        entry->callback(HCI_EVENT_PACKET, 0, event, size);
    }
}

static void hci_emit_acl_packet(uint8_t * packet, uint16_t size){
    if (!hci_stack->acl_packet_handler) return;
    hci_stack->acl_packet_handler(HCI_ACL_DATA_PACKET, 0, packet, size);
}

#ifdef ENABLE_CLASSIC
static void hci_notify_if_sco_can_send_now(void){
    // notify SCO sender if waiting
    if (!hci_stack->sco_waiting_for_can_send_now) return;
    if (hci_can_send_sco_packet_now()){
        hci_stack->sco_waiting_for_can_send_now = 0;
        uint8_t event[2] = { HCI_EVENT_SCO_CAN_SEND_NOW, 0 };
        hci_dump_packet(HCI_EVENT_PACKET, 1, event, sizeof(event));
        hci_stack->sco_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
    }
}

// parsing end emitting has been merged to reduce code size
static void gap_inquiry_explode(uint8_t *packet, uint16_t size) {
    uint8_t event[28+GAP_INQUIRY_MAX_NAME_LEN];

    uint8_t * eir_data;
    ad_context_t context;
    const uint8_t * name;
    uint8_t         name_len;

    if (size < 3) return;

    int event_type = hci_event_packet_get_type(packet);
    int num_reserved_fields = (event_type == HCI_EVENT_INQUIRY_RESULT) ? 2 : 1;    // 2 for old event, 1 otherwise
    int num_responses       = hci_event_inquiry_result_get_num_responses(packet);

    switch (event_type){
        case HCI_EVENT_INQUIRY_RESULT:
        case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:
            if (size != (3 + (num_responses * 14))) return;
            break;
        case HCI_EVENT_EXTENDED_INQUIRY_RESPONSE:
            if (size != 257) return;
            if (num_responses != 1) return;
            break;
        default:
            return;
    }

    // event[1] is set at the end
    int i;
    for (i=0; i<num_responses;i++){
        memset(event, 0, sizeof(event));
        event[0] = GAP_EVENT_INQUIRY_RESULT;
        uint8_t event_size = 27;    // if name is not set by EIR

        (void)memcpy(&event[2], &packet[3 + (i * 6)], 6); // bd_addr
        event[8] =          packet[3 + (num_responses*(6))                         + (i*1)];     // page_scan_repetition_mode
        (void)memcpy(&event[9],
                     &packet[3 + (num_responses * (6 + 1 + num_reserved_fields)) + (i * 3)],
                     3); // class of device
        (void)memcpy(&event[12],
                     &packet[3 + (num_responses * (6 + 1 + num_reserved_fields + 3)) + (i * 2)],
                     2); // clock offset

        switch (event_type){
            case HCI_EVENT_INQUIRY_RESULT:
                // 14,15,16,17 = 0, size 18
                break;
            case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:
                event[14] = 1;
                event[15] = packet [3 + (num_responses*(6+1+num_reserved_fields+3+2)) + (i*1)]; // rssi
                // 16,17 = 0, size 18
                break;
            case HCI_EVENT_EXTENDED_INQUIRY_RESPONSE:
                event[14] = 1;
                event[15] = packet [3 + (num_responses*(6+1+num_reserved_fields+3+2)) + (i*1)]; // rssi
                // EIR packets only contain a single inquiry response
                eir_data = &packet[3 + (6+1+num_reserved_fields+3+2+1)];
                name = NULL;
                // Iterate over EIR data
                for (ad_iterator_init(&context, EXTENDED_INQUIRY_RESPONSE_DATA_LEN, eir_data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
                    uint8_t data_type    = ad_iterator_get_data_type(&context);
                    uint8_t data_size    = ad_iterator_get_data_len(&context);
                    const uint8_t * data = ad_iterator_get_data(&context);
                    // Prefer Complete Local Name over Shortened Local Name
                    switch (data_type){
                        case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
                            if (name) continue;
                            /* fall through */
                        case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                            name = data;
                            name_len = data_size;
                            break;
                        case BLUETOOTH_DATA_TYPE_DEVICE_ID:
                            if (data_size != 8) break;
                            event[16] = 1;
                            memcpy(&event[17], data, 8);
                            break;
                        default:
                            break;
                    }
                }
                if (name){
                    event[25] = 1;
                    // truncate name if needed
                    int len = btstack_min(name_len, GAP_INQUIRY_MAX_NAME_LEN);
                    event[26] = len;
                    (void)memcpy(&event[27], name, len);
                    event_size += len;
                }
                break;
            default:
                return;
        }
        event[1] = event_size - 2;
        hci_emit_event(event, event_size, 1);
    }
}
#endif

void hci_emit_state(void){
    log_info("BTSTACK_EVENT_STATE %u", hci_stack->state);
    uint8_t event[3];
    event[0] = BTSTACK_EVENT_STATE;
    event[1] = sizeof(event) - 2u;
    event[2] = hci_stack->state;
    hci_emit_event(event, sizeof(event), 1);
}

#ifdef ENABLE_CLASSIC
static void hci_emit_connection_complete(bd_addr_t address, hci_con_handle_t con_handle, uint8_t status){
    uint8_t event[13];
    event[0] = HCI_EVENT_CONNECTION_COMPLETE;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    little_endian_store_16(event, 3, con_handle);
    reverse_bd_addr(address, &event[5]);
    event[11] = 1; // ACL connection
    event[12] = 0; // encryption disabled
    hci_emit_event(event, sizeof(event), 1);
}
static void hci_emit_l2cap_check_timeout(hci_connection_t *conn){
    if (disable_l2cap_timeouts) return;
    log_info("L2CAP_EVENT_TIMEOUT_CHECK");
    uint8_t event[4];
    event[0] = L2CAP_EVENT_TIMEOUT_CHECK;
    event[1] = sizeof(event) - 2;
    little_endian_store_16(event, 2, conn->con_handle);
    hci_emit_event(event, sizeof(event), 1);
}
#endif

#ifdef ENABLE_BLE
#ifdef ENABLE_LE_CENTRAL
static void hci_emit_le_connection_complete(uint8_t address_type, const bd_addr_t address, hci_con_handle_t con_handle, uint8_t status){
    uint8_t event[21];
    event[0] = HCI_EVENT_LE_META;
    event[1] = sizeof(event) - 2u;
    event[2] = HCI_SUBEVENT_LE_CONNECTION_COMPLETE;
    event[3] = status;
    little_endian_store_16(event, 4, con_handle);
    event[6] = 0; // TODO: role
    event[7] = address_type;
    reverse_bd_addr(address, &event[8]);
    little_endian_store_16(event, 14, 0); // interval
    little_endian_store_16(event, 16, 0); // latency
    little_endian_store_16(event, 18, 0); // supervision timeout
    event[20] = 0; // master clock accuracy
    hci_emit_event(event, sizeof(event), 1);
}
#endif
#endif

static void hci_emit_transport_packet_sent(void){
    // notify upper stack that it might be possible to send again
    uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
    hci_emit_event(&event[0], sizeof(event), 0);  // don't dump
}

static void hci_emit_disconnection_complete(hci_con_handle_t con_handle, uint8_t reason){
    uint8_t event[6];
    event[0] = HCI_EVENT_DISCONNECTION_COMPLETE;
    event[1] = sizeof(event) - 2u;
    event[2] = 0; // status = OK
    little_endian_store_16(event, 3, con_handle);
    event[5] = reason;
    hci_emit_event(event, sizeof(event), 1);
}

static void hci_emit_nr_connections_changed(void){
    log_info("BTSTACK_EVENT_NR_CONNECTIONS_CHANGED %u", nr_hci_connections());
    uint8_t event[3];
    event[0] = BTSTACK_EVENT_NR_CONNECTIONS_CHANGED;
    event[1] = sizeof(event) - 2u;
    event[2] = nr_hci_connections();
    hci_emit_event(event, sizeof(event), 1);
}

static void hci_emit_hci_open_failed(void){
    log_info("BTSTACK_EVENT_POWERON_FAILED");
    uint8_t event[2];
    event[0] = BTSTACK_EVENT_POWERON_FAILED;
    event[1] = sizeof(event) - 2u;
    hci_emit_event(event, sizeof(event), 1);
}

static void hci_emit_dedicated_bonding_result(bd_addr_t address, uint8_t status){
    log_info("hci_emit_dedicated_bonding_result %u ", status);
    uint8_t event[9];
    int pos = 0;
    event[pos++] = GAP_EVENT_DEDICATED_BONDING_COMPLETED;
    event[pos++] = sizeof(event) - 2u;
    event[pos++] = status;
    reverse_bd_addr(address, &event[pos]);
    hci_emit_event(event, sizeof(event), 1);
}


#ifdef ENABLE_CLASSIC

static void hci_emit_security_level(hci_con_handle_t con_handle, gap_security_level_t level){
    log_info("hci_emit_security_level %u for handle %x", level, con_handle);
    uint8_t event[5];
    int pos = 0;
    event[pos++] = GAP_EVENT_SECURITY_LEVEL;
    event[pos++] = sizeof(event) - 2;
    little_endian_store_16(event, 2, con_handle);
    pos += 2;
    event[pos++] = level;
    hci_emit_event(event, sizeof(event), 1);
}

static gap_security_level_t gap_security_level_for_connection(hci_connection_t * connection){
    if (!connection) return LEVEL_0;
    if ((connection->authentication_flags & AUTH_FLAG_CONNECTION_ENCRYPTED) == 0) return LEVEL_0;
    // BIAS: we only consider Authenticated if the connection is already encrypted, which requires that both sides have link key
    if ((connection->authentication_flags & AUTH_FLAG_CONNECTION_AUTHENTICATED) == 0) return LEVEL_0;
    if (connection->encryption_key_size < hci_stack->gap_required_encyrption_key_size) return LEVEL_0;
    gap_security_level_t security_level = gap_security_level_for_link_key_type(connection->link_key_type);
    // LEVEL 4 always requires 128 bit encrytion key size
    if ((security_level == LEVEL_4) && (connection->encryption_key_size < 16)){
        security_level = LEVEL_3;
    }
    return security_level;
}    

static void hci_emit_scan_mode_changed(uint8_t discoverable, uint8_t connectable){
    uint8_t event[4];
    event[0] = BTSTACK_EVENT_SCAN_MODE_CHANGED;
    event[1] = sizeof(event) - 2;
    event[2] = discoverable;
    event[3] = connectable;
    hci_emit_event(event, sizeof(event), 1);
}

// query if remote side supports eSCO
bool hci_remote_esco_supported(hci_con_handle_t con_handle){
    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection) return false;
    return (connection->remote_supported_features[0] & 1) != 0;
}

uint16_t hci_remote_sco_packet_types(hci_con_handle_t con_handle){
    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection) return 0;
    return connection->remote_supported_sco_packets;
}

static bool hci_ssp_supported(hci_connection_t * connection){
    const uint8_t mask = BONDING_REMOTE_SUPPORTS_SSP_CONTROLLER | BONDING_REMOTE_SUPPORTS_SSP_HOST;
    return (connection->bonding_flags & mask) == mask;
}

// query if remote side supports SSP
bool hci_remote_ssp_supported(hci_con_handle_t con_handle){
    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection) return false;
    return hci_ssp_supported(connection) ? 1 : 0;
}

bool gap_ssp_supported_on_both_sides(hci_con_handle_t handle){
    return hci_local_ssp_activated() && hci_remote_ssp_supported(handle);
}

/**
 * Check if remote supported features query has completed
 */
bool hci_remote_features_available(hci_con_handle_t handle){
    hci_connection_t * connection = hci_connection_for_handle(handle);
    if (!connection) return false;
    return (connection->bonding_flags & BONDING_RECEIVED_REMOTE_FEATURES) != 0;
}

/**
 * Trigger remote supported features query
 */

static void hci_trigger_remote_features_for_connection(hci_connection_t * connection){
    if ((connection->bonding_flags & (BONDING_REMOTE_FEATURES_QUERY_ACTIVE | BONDING_RECEIVED_REMOTE_FEATURES)) == 0){
        connection->bonding_flags |= BONDING_REMOTE_FEATURES_QUERY_ACTIVE | BONDING_REQUEST_REMOTE_FEATURES_PAGE_0;
    }
}

void hci_remote_features_query(hci_con_handle_t con_handle){
    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection) return;
    hci_trigger_remote_features_for_connection(connection);
    hci_run();
}

// GAP API
/**
 * @bbrief enable/disable bonding. default is enabled
 * @praram enabled
 */
void gap_set_bondable_mode(int enable){
    hci_stack->bondable = enable ? 1 : 0;
}
/**  
 * @brief Get bondable mode.
 * @return 1 if bondable
 */
int gap_get_bondable_mode(void){
    return hci_stack->bondable;
}

/**
 * @brief map link keys to security levels
 */
gap_security_level_t gap_security_level_for_link_key_type(link_key_type_t link_key_type){
    switch (link_key_type){
        case AUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P256:
            return LEVEL_4;
        case COMBINATION_KEY:
        case AUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P192:
            return LEVEL_3;
        default:
            return LEVEL_2;
    }
}

/**
 * @brief map link keys to secure connection yes/no
 */
bool gap_secure_connection_for_link_key_type(link_key_type_t link_key_type){
    switch (link_key_type){
        case AUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P256:
        case UNAUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P256:
            return true;
        default:
            return false;
    }
}

/**
 * @brief map link keys to authenticated
 */
bool gap_authenticated_for_link_key_type(link_key_type_t link_key_type){
    switch (link_key_type){
        case AUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P256:
        case AUTHENTICATED_COMBINATION_KEY_GENERATED_FROM_P192:
            return true;
        default:
            return false;
    }
}

bool gap_mitm_protection_required_for_security_level(gap_security_level_t level){
    log_info("gap_mitm_protection_required_for_security_level %u", level);
    return level > LEVEL_2;
}

/**
 * @brief get current security level
 */
gap_security_level_t gap_security_level(hci_con_handle_t con_handle){
    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection) return LEVEL_0;
    return gap_security_level_for_connection(connection);
}

/**
 * @brief request connection to device to
 * @result GAP_AUTHENTICATION_RESULT
 */
void gap_request_security_level(hci_con_handle_t con_handle, gap_security_level_t requested_level){
    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection){
        hci_emit_security_level(con_handle, LEVEL_0);
        return;
    }

    btstack_assert(hci_is_le_connection(connection) == false);

    // Core Spec 5.2, GAP 5.2.2: "When in Secure Connections Only mode, all services (except those allowed to have Security Mode 4, Level 0)
    // available on the BR/EDR physical transport require Security Mode 4, Level 4 "
    if (hci_stack->gap_secure_connections_only_mode && (requested_level != LEVEL_0)){
        requested_level = LEVEL_4;
    }

    gap_security_level_t current_level = gap_security_level(con_handle);
    log_info("gap_request_security_level requested level %u, planned level %u, current level %u", 
        requested_level, connection->requested_security_level, current_level);

    // authentication active if authentication request was sent or planned level > 0
    bool authentication_active = ((connection->bonding_flags & BONDING_SENT_AUTHENTICATE_REQUEST) != 0) || (connection->requested_security_level > LEVEL_0);
    if (authentication_active){
        // authentication already active
        if (connection->requested_security_level < requested_level){
            // increase requested level as new level is higher
            // TODO: handle re-authentication when done
            connection->requested_security_level = requested_level;
        }
    } else {
        // no request active, notify if security sufficient
        if (requested_level <= current_level){
            hci_emit_security_level(con_handle, current_level);
            return;
        }

        // store request
        connection->requested_security_level = requested_level;

        // start to authenticate connection
        connection->bonding_flags |= BONDING_SEND_AUTHENTICATE_REQUEST;

        // request remote features if not already active, also trigger hci_run
        hci_remote_features_query(con_handle);
    }
}

/**
 * @brief start dedicated bonding with device. disconnect after bonding
 * @param device
 * @param request MITM protection
 * @result GAP_DEDICATED_BONDING_COMPLETE
 */
int gap_dedicated_bonding(bd_addr_t device, int mitm_protection_required){

    // create connection state machine
    hci_connection_t * connection = create_connection_for_bd_addr_and_type(device, BD_ADDR_TYPE_ACL, HCI_ROLE_MASTER);

    if (!connection){
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }

    // delete link key
    gap_drop_link_key_for_bd_addr(device);

    // configure LEVEL_2/3, dedicated bonding
    connection->state = SEND_CREATE_CONNECTION;    
    connection->requested_security_level = mitm_protection_required ? LEVEL_3 : LEVEL_2;
    log_info("gap_dedicated_bonding, mitm %d -> level %u", mitm_protection_required, connection->requested_security_level);
    connection->bonding_flags = BONDING_DEDICATED;

    hci_run();

    return 0;
}

uint8_t hci_dedicated_bonding_defer_disconnect(hci_con_handle_t con_handle, bool defer){
    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (defer){
        connection->bonding_flags |= BONDING_DEDICATED_DEFER_DISCONNECT;
    } else {
        connection->bonding_flags &= ~BONDING_DEDICATED_DEFER_DISCONNECT;
        // trigger disconnect
        hci_run();
    }
    return ERROR_CODE_SUCCESS;
}

void gap_set_local_name(const char * local_name){
    hci_stack->local_name = local_name;
    hci_stack->gap_tasks_classic |= GAP_TASK_SET_LOCAL_NAME;
    // also update EIR if not set by user
    if (hci_stack->eir_data == NULL){
        hci_stack->gap_tasks_classic |= GAP_TASK_SET_EIR_DATA;
    }
    hci_run();
}
#endif


#ifdef ENABLE_BLE

#ifdef ENABLE_LE_CENTRAL
void gap_start_scan(void){
    hci_stack->le_scanning_enabled = true;
    hci_run();
}

void gap_stop_scan(void){
    hci_stack->le_scanning_enabled = false;
    hci_run();
}

void gap_set_scan_params(uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window, uint8_t scanning_filter_policy){
    hci_stack->le_scan_type          = scan_type;
    hci_stack->le_scan_filter_policy = scanning_filter_policy;
    hci_stack->le_scan_interval      = scan_interval;
    hci_stack->le_scan_window        = scan_window;
    hci_stack->le_scanning_param_update = true;
    hci_run();
}

void gap_set_scan_parameters(uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window){
    gap_set_scan_params(scan_type, scan_interval, scan_window, 0);
}

void gap_set_scan_duplicate_filter(bool enabled){
    hci_stack->le_scan_filter_duplicates = enabled ? 1 : 0;
}

void gap_set_scan_phys(uint8_t phys){
    // LE Coded and LE 1M PHY
    hci_stack->le_scan_phys = phys & 0x05;
}

uint8_t gap_connect(const bd_addr_t addr, bd_addr_type_t addr_type){
    hci_connection_t * conn = hci_connection_for_bd_addr_and_type(addr, addr_type);
    if (!conn){
        // disallow if le connection is already outgoing
        if (hci_is_le_connection_type(addr_type) && hci_stack->le_connecting_request != LE_CONNECTING_IDLE){
            log_error("le connection already active");
            return ERROR_CODE_COMMAND_DISALLOWED;
        }

        log_info("gap_connect: no connection exists yet, creating context");
        conn = create_connection_for_bd_addr_and_type(addr, addr_type, HCI_ROLE_MASTER);
        if (!conn){
            // notify client that alloc failed
            hci_emit_le_connection_complete(addr_type, addr, 0, BTSTACK_MEMORY_ALLOC_FAILED);
            log_info("gap_connect: failed to alloc hci_connection_t");
            return GATT_CLIENT_NOT_CONNECTED; // don't sent packet to controller
        }

        // set le connecting state
        if (hci_is_le_connection_type(addr_type)){
            hci_stack->le_connecting_request = LE_CONNECTING_DIRECT;
        }

        conn->state = SEND_CREATE_CONNECTION;
        log_info("gap_connect: send create connection next");
        hci_run();
        return ERROR_CODE_SUCCESS;
    }
    
    if (!hci_is_le_connection(conn) ||
        (conn->state == SEND_CREATE_CONNECTION) ||
        (conn->state == SENT_CREATE_CONNECTION)) {
        hci_emit_le_connection_complete(conn->address_type, conn->address, 0, ERROR_CODE_COMMAND_DISALLOWED);
        log_error("gap_connect: classic connection or connect is already being created");
        return GATT_CLIENT_IN_WRONG_STATE;
    }

    // check if connection was just disconnected
    if (conn->state == RECEIVED_DISCONNECTION_COMPLETE){
        log_info("gap_connect: send create connection (again)");
        conn->state = SEND_CREATE_CONNECTION;
        hci_run();
        return ERROR_CODE_SUCCESS;
    }

    log_info("gap_connect: context exists with state %u", conn->state);
    hci_emit_le_connection_complete(conn->address_type, conn->address, conn->con_handle, ERROR_CODE_SUCCESS);
    hci_run();
    return ERROR_CODE_SUCCESS;
}

// @assumption: only a single outgoing LE Connection exists
static hci_connection_t * gap_get_outgoing_connection(void){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) hci_stack->connections; it != NULL; it = it->next){
        hci_connection_t * conn = (hci_connection_t *) it;
        if (!hci_is_le_connection(conn)) continue;
        switch (conn->state){
            case SEND_CREATE_CONNECTION:
            case SENT_CREATE_CONNECTION:
                return conn;
            default:
                break;
        };
    }
    return NULL;
}

uint8_t gap_connect_cancel(void){
    hci_connection_t * conn;
    switch (hci_stack->le_connecting_request){
        case LE_CONNECTING_IDLE:
            break;
        case LE_CONNECTING_WHITELIST:
            hci_stack->le_connecting_request = LE_CONNECTING_IDLE;
            hci_run();
            break;
        case LE_CONNECTING_DIRECT:
            hci_stack->le_connecting_request = LE_CONNECTING_IDLE;
            conn = gap_get_outgoing_connection();
            if (conn == NULL){
                hci_run();
            } else {
                switch (conn->state){
                    case SEND_CREATE_CONNECTION:
                        // skip sending create connection and emit event instead
                        hci_emit_le_connection_complete(conn->address_type, conn->address, 0, ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER);
                        btstack_linked_list_remove(&hci_stack->connections, (btstack_linked_item_t *) conn);
                        btstack_memory_hci_connection_free( conn );
                        break;
                    case SENT_CREATE_CONNECTION:
                        // let hci_run_general_gap_le cancel outgoing connection
                        hci_run();
                        break;
                    default:
                        break;
                }
            }
            break;
        default:
            btstack_unreachable();
            break;
    }
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Set connection parameters for outgoing connections
 * @param conn_scan_interval (unit: 0.625 msec), default: 60 ms
 * @param conn_scan_window (unit: 0.625 msec), default: 30 ms
 * @param conn_interval_min (unit: 1.25ms), default: 10 ms
 * @param conn_interval_max (unit: 1.25ms), default: 30 ms
 * @param conn_latency, default: 4
 * @param supervision_timeout (unit: 10ms), default: 720 ms
 * @param min_ce_length (unit: 0.625ms), default: 10 ms
 * @param max_ce_length (unit: 0.625ms), default: 30 ms
 */

void gap_set_connection_phys(uint8_t phys){
    // LE Coded, LE 1M, LE 2M PHY
    hci_stack->le_connection_phys = phys & 7;
}

#endif

void gap_set_connection_parameters(uint16_t conn_scan_interval, uint16_t conn_scan_window,
                                   uint16_t conn_interval_min, uint16_t conn_interval_max, uint16_t conn_latency,
                                   uint16_t supervision_timeout, uint16_t min_ce_length, uint16_t max_ce_length){
    hci_stack->le_connection_scan_interval = conn_scan_interval;
    hci_stack->le_connection_scan_window = conn_scan_window;
    hci_stack->le_connection_interval_min = conn_interval_min;
    hci_stack->le_connection_interval_max = conn_interval_max;
    hci_stack->le_connection_latency = conn_latency;
    hci_stack->le_supervision_timeout = supervision_timeout;
    hci_stack->le_minimum_ce_length = min_ce_length;
    hci_stack->le_maximum_ce_length = max_ce_length;
}

/**
 * @brief Updates the connection parameters for a given LE connection
 * @param handle
 * @param conn_interval_min (unit: 1.25ms)
 * @param conn_interval_max (unit: 1.25ms)
 * @param conn_latency
 * @param supervision_timeout (unit: 10ms)
 * @return 0 if ok
 */
int gap_update_connection_parameters(hci_con_handle_t con_handle, uint16_t conn_interval_min,
    uint16_t conn_interval_max, uint16_t conn_latency, uint16_t supervision_timeout){
    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    connection->le_conn_interval_min = conn_interval_min;
    connection->le_conn_interval_max = conn_interval_max;
    connection->le_conn_latency = conn_latency;
    connection->le_supervision_timeout = supervision_timeout;
    connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_CHANGE_HCI_CON_PARAMETERS;
    hci_run();
    return 0;
}

/**
 * @brief Request an update of the connection parameter for a given LE connection
 * @param handle
 * @param conn_interval_min (unit: 1.25ms)
 * @param conn_interval_max (unit: 1.25ms)
 * @param conn_latency
 * @param supervision_timeout (unit: 10ms)
 * @return 0 if ok
 */
int gap_request_connection_parameter_update(hci_con_handle_t con_handle, uint16_t conn_interval_min,
    uint16_t conn_interval_max, uint16_t conn_latency, uint16_t supervision_timeout){
    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    connection->le_conn_interval_min = conn_interval_min;
    connection->le_conn_interval_max = conn_interval_max;
    connection->le_conn_latency = conn_latency;
    connection->le_supervision_timeout = supervision_timeout;
    connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_SEND_REQUEST;
    uint8_t l2cap_trigger_run_event[2] = { L2CAP_EVENT_TRIGGER_RUN, 0};
    hci_emit_event(l2cap_trigger_run_event, sizeof(l2cap_trigger_run_event), 0);
    return 0;
}

#ifdef ENABLE_LE_PERIPHERAL

/**
 * @brief Set Advertisement Data
 * @param advertising_data_length
 * @param advertising_data (max 31 octets)
 * @note data is not copied, pointer has to stay valid
 */
void gap_advertisements_set_data(uint8_t advertising_data_length, uint8_t * advertising_data){
    hci_stack->le_advertisements_data_len = advertising_data_length;
    hci_stack->le_advertisements_data = advertising_data;
    hci_stack->le_advertisements_todo |= LE_ADVERTISEMENT_TASKS_SET_ADV_DATA;
    hci_run();
}

/** 
 * @brief Set Scan Response Data
 * @param advertising_data_length
 * @param advertising_data (max 31 octets)
 * @note data is not copied, pointer has to stay valid
 */
void gap_scan_response_set_data(uint8_t scan_response_data_length, uint8_t * scan_response_data){
    hci_stack->le_scan_response_data_len = scan_response_data_length;
    hci_stack->le_scan_response_data = scan_response_data;
    hci_stack->le_advertisements_todo |= LE_ADVERTISEMENT_TASKS_SET_SCAN_DATA;
    hci_run();
}

/**
 * @brief Set Advertisement Parameters
 * @param adv_int_min
 * @param adv_int_max
 * @param adv_type
 * @param direct_address_type
 * @param direct_address
 * @param channel_map
 * @param filter_policy
 *
 * @note internal use. use gap_advertisements_set_params from gap_le.h instead.
 */
 void hci_le_advertisements_set_params(uint16_t adv_int_min, uint16_t adv_int_max, uint8_t adv_type,
    uint8_t direct_address_typ, bd_addr_t direct_address,
    uint8_t channel_map, uint8_t filter_policy) {

    hci_stack->le_advertisements_interval_min = adv_int_min;
    hci_stack->le_advertisements_interval_max = adv_int_max;
    hci_stack->le_advertisements_type = adv_type;
    hci_stack->le_advertisements_direct_address_type = direct_address_typ;
    hci_stack->le_advertisements_channel_map = channel_map;
    hci_stack->le_advertisements_filter_policy = filter_policy;
    (void)memcpy(hci_stack->le_advertisements_direct_address, direct_address,
                 6);

    hci_stack->le_advertisements_todo  |= LE_ADVERTISEMENT_TASKS_SET_PARAMS;
    hci_stack->le_advertisements_state |= LE_ADVERTISEMENT_STATE_PARAMS_SET;
    hci_run();
 }

/**
 * @brief Enable/Disable Advertisements
 * @param enabled
 */
void gap_advertisements_enable(int enabled){
    if (enabled == 0){
        hci_stack->le_advertisements_state &= ~LE_ADVERTISEMENT_STATE_ENABLED;
    } else {
        hci_stack->le_advertisements_state |= LE_ADVERTISEMENT_STATE_ENABLED;
    }
    hci_update_advertisements_enabled_for_current_roles();
    hci_run();
}

#ifdef ENABLE_LE_EXTENDED_ADVERTISING
static le_advertising_set_t * hci_advertising_set_for_handle(uint8_t advertising_handle){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->le_advertising_sets);
    while (btstack_linked_list_iterator_has_next(&it)){
        le_advertising_set_t * item = (le_advertising_set_t *) btstack_linked_list_iterator_next(&it);
        if ( item->advertising_handle == advertising_handle ) {
            return item;
        }
    }
    return NULL;
}

uint8_t gap_extended_advertising_setup(le_advertising_set_t * storage, const le_extended_advertising_parameters_t * advertising_parameters, uint8_t * out_advertising_handle){
    // find free advertisement handle
    uint8_t advertisement_handle;
    for (advertisement_handle = 1; advertisement_handle <= LE_EXTENDED_ADVERTISING_MAX_HANDLE; advertisement_handle++){
        if (hci_advertising_set_for_handle(advertisement_handle) == NULL) break;
    }
    if (advertisement_handle > LE_EXTENDED_ADVERTISING_MAX_HANDLE) return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    // clear
    memset(storage, 0, sizeof(le_advertising_set_t));
    // copy params
    storage->advertising_handle = advertisement_handle;
    memcpy(&storage->extended_params, advertising_parameters, sizeof(le_extended_advertising_parameters_t));
    // add to list
    bool add_ok = btstack_linked_list_add(&hci_stack->le_advertising_sets, (btstack_linked_item_t *) storage);
    if (!add_ok) return ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS;
    *out_advertising_handle = advertisement_handle;
    // set tasks and start
    storage->tasks = LE_ADVERTISEMENT_TASKS_SET_PARAMS;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_extended_advertising_set_params(uint8_t advertising_handle, const le_extended_advertising_parameters_t * advertising_parameters){
    le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(advertising_handle);
    if (advertising_set == NULL) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    memcpy(&advertising_set->extended_params, advertising_parameters, sizeof(le_extended_advertising_parameters_t));
    // set tasks and start
    advertising_set->tasks |= LE_ADVERTISEMENT_TASKS_SET_PARAMS;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_extended_advertising_get_params(uint8_t advertising_handle, le_extended_advertising_parameters_t * advertising_parameters){
    le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(advertising_handle);
    if (advertising_set == NULL) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    memcpy(advertising_parameters, &advertising_set->extended_params, sizeof(le_extended_advertising_parameters_t));
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_extended_advertising_set_random_address(uint8_t advertising_handle, bd_addr_t random_address){
    le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(advertising_handle);
    if (advertising_set == NULL) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    memcpy(advertising_set->random_address, random_address, 6);
    // set tasks and start
    advertising_set->tasks |= LE_ADVERTISEMENT_TASKS_SET_ADDRESS;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_extended_advertising_set_adv_data(uint8_t advertising_handle, uint16_t advertising_data_length, const uint8_t * advertising_data){
    le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(advertising_handle);
    if (advertising_set == NULL) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    advertising_set->adv_data = advertising_data;
    advertising_set->adv_data_len = advertising_data_length;
    // set tasks and start
    advertising_set->tasks |= LE_ADVERTISEMENT_TASKS_SET_ADV_DATA;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_extended_advertising_set_scan_response_data(uint8_t advertising_handle, uint16_t scan_response_data_length, const uint8_t * scan_response_data){
    le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(advertising_handle);
    if (advertising_set == NULL) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    advertising_set->scan_data = scan_response_data;
    advertising_set->scan_data_len = scan_response_data_length;
    // set tasks and start
    advertising_set->tasks |= LE_ADVERTISEMENT_TASKS_SET_SCAN_DATA;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_extended_advertising_start(uint8_t advertising_handle, uint16_t timeout, uint8_t num_extended_advertising_events){
    le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(advertising_handle);
    if (advertising_set == NULL) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    advertising_set->enable_timeout = timeout;
    advertising_set->enable_max_scan_events = num_extended_advertising_events;
    // set tasks and start
    advertising_set->state |= LE_ADVERTISEMENT_STATE_ENABLED;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_extended_advertising_stop(uint8_t advertising_handle){
    le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(advertising_handle);
    if (advertising_set == NULL) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    // set tasks and start
    advertising_set->state &= ~LE_ADVERTISEMENT_STATE_ENABLED;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_extended_advertising_remove(uint8_t advertising_handle){
    le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(advertising_handle);
    if (advertising_set == NULL) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    // set tasks and start
    advertising_set->tasks |= LE_ADVERTISEMENT_TASKS_REMOVE_SET;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

#ifdef ENABLE_LE_PERIODIC_ADVERTISING
uint8_t gap_periodic_advertising_set_params(uint8_t advertising_handle, const le_periodic_advertising_parameters_t * advertising_parameters){
    le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(advertising_handle);
    if (advertising_set == NULL) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    // periodic advertising requires neither connectable, scannable, legacy or anonymous
    if ((advertising_set->extended_params.advertising_event_properties & 0x1f) != 0) return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    memcpy(&advertising_set->periodic_params, advertising_parameters, sizeof(le_periodic_advertising_parameters_t));
    // set tasks and start
    advertising_set->tasks |= LE_ADVERTISEMENT_TASKS_SET_PERIODIC_PARAMS;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_periodic_advertising_get_params(uint8_t advertising_handle, le_periodic_advertising_parameters_t * advertising_parameters){
    le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(advertising_handle);
    if (advertising_set == NULL) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    memcpy(advertising_parameters, &advertising_set->extended_params, sizeof(le_periodic_advertising_parameters_t));
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_periodic_advertising_set_data(uint8_t advertising_handle, uint16_t periodic_data_length, const uint8_t * periodic_data){
    le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(advertising_handle);
    if (advertising_set == NULL) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    advertising_set->periodic_data = periodic_data;
    advertising_set->periodic_data_len = periodic_data_length;
    // set tasks and start
    advertising_set->tasks |= LE_ADVERTISEMENT_TASKS_SET_PERIODIC_DATA;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_periodic_advertising_start(uint8_t advertising_handle, bool include_adi){
    le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(advertising_handle);
    if (advertising_set == NULL) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    // set tasks and start
    advertising_set->periodic_include_adi = include_adi;
    advertising_set->state |= LE_ADVERTISEMENT_STATE_PERIODIC_ENABLED;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_periodic_advertising_stop(uint8_t advertising_handle){
    le_advertising_set_t * advertising_set = hci_advertising_set_for_handle(advertising_handle);
    if (advertising_set == NULL) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    // set tasks and start
    advertising_set->state &= ~LE_ADVERTISEMENT_STATE_PERIODIC_ENABLED;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_periodic_advertising_sync_transfer_set_default_parameters(uint8_t mode, uint16_t skip, uint16_t sync_timeout, uint8_t cte_type){
    hci_stack->le_past_mode = mode;
    hci_stack->le_past_skip = skip;
    hci_stack->le_past_sync_timeout = sync_timeout;
    hci_stack->le_past_cte_type = cte_type;
    hci_stack->le_past_set_default_params = true;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_periodic_advertising_sync_transfer_send(hci_con_handle_t con_handle, uint16_t service_data, hci_con_handle_t sync_handle){
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (hci_connection == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    hci_connection->le_past_sync_handle = sync_handle;
    hci_connection->le_past_service_data = service_data;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

#endif /* ENABLE_LE_PERIODIC_ADVERTISING */

#endif

#endif

void hci_le_set_own_address_type(uint8_t own_address_type){
    log_info("hci_le_set_own_address_type: old %u, new %u", hci_stack->le_own_addr_type, own_address_type);
    if (own_address_type == hci_stack->le_own_addr_type) return;
    hci_stack->le_own_addr_type = own_address_type;

#ifdef ENABLE_LE_PERIPHERAL
    // update advertisement parameters, too
    hci_stack->le_advertisements_todo |= LE_ADVERTISEMENT_TASKS_SET_PARAMS;
    hci_run();
#endif
#ifdef ENABLE_LE_CENTRAL
    // note: we don't update scan parameters or modify ongoing connection attempts
#endif
}

void hci_le_random_address_set(const bd_addr_t random_address){
    memcpy(hci_stack->le_random_address, random_address, 6);
    hci_stack->le_random_address_set = true;
    hci_stack->le_advertisements_todo |= LE_ADVERTISEMENT_TASKS_SET_ADDRESS;
#ifdef ENABLE_LE_EXTENDED_ADVERTISING
    if (hci_extended_advertising_supported()){
        // force advertising set creation for LE Set Advertising Set Random Address
        if ((hci_stack->le_advertisements_state & LE_ADVERTISEMENT_STATE_PARAMS_SET) == 0){
            hci_stack->le_advertisements_todo |= LE_ADVERTISEMENT_TASKS_SET_PARAMS;
        }
        hci_stack->le_advertisements_todo |= LE_ADVERTISEMENT_TASKS_SET_ADDRESS_SET_0;
    }
#endif
    hci_run();
}

#endif

uint8_t gap_disconnect(hci_con_handle_t handle){
    hci_connection_t * conn = hci_connection_for_handle(handle);
    if (!conn){
        hci_emit_disconnection_complete(handle, 0);
        return 0;
    }
    // ignore if already disconnected
    if (conn->state == RECEIVED_DISCONNECTION_COMPLETE){
        return 0;
    }
    conn->state = SEND_DISCONNECT;
    hci_run();
    return 0;
}

int gap_read_rssi(hci_con_handle_t con_handle){
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (hci_connection == NULL) return 0;
    hci_connection->gap_connection_tasks |= GAP_CONNECTION_TASK_READ_RSSI;
    hci_run();
    return 1;
}

/**
 * @brief Get connection type
 * @param con_handle
 * @result connection_type
 */
gap_connection_type_t gap_get_connection_type(hci_con_handle_t connection_handle){
    hci_connection_t * conn = hci_connection_for_handle(connection_handle);
    if (!conn) return GAP_CONNECTION_INVALID;
    switch (conn->address_type){
        case BD_ADDR_TYPE_LE_PUBLIC:
        case BD_ADDR_TYPE_LE_RANDOM:
            return GAP_CONNECTION_LE;
        case BD_ADDR_TYPE_SCO:
            return GAP_CONNECTION_SCO;
        case BD_ADDR_TYPE_ACL:
            return GAP_CONNECTION_ACL;
        default:
            return GAP_CONNECTION_INVALID;
    }
}

hci_role_t gap_get_role(hci_con_handle_t connection_handle){
    hci_connection_t * conn = hci_connection_for_handle(connection_handle);
    if (!conn) return HCI_ROLE_INVALID;
    return (hci_role_t) conn->role;
}


#ifdef ENABLE_CLASSIC
uint8_t gap_request_role(const bd_addr_t addr, hci_role_t role){
    hci_connection_t * conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
    if (!conn) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    conn->request_role = role;
    hci_run();
    return ERROR_CODE_SUCCESS;
}
#endif

#ifdef ENABLE_BLE

uint8_t gap_le_set_phy(hci_con_handle_t con_handle, uint8_t all_phys, uint8_t tx_phys, uint8_t rx_phys, uint8_t phy_options){
    hci_connection_t * conn = hci_connection_for_handle(con_handle);
    if (!conn) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;

    conn->le_phy_update_all_phys    = all_phys;
    conn->le_phy_update_tx_phys     = tx_phys;
    conn->le_phy_update_rx_phys     = rx_phys;
    conn->le_phy_update_phy_options = phy_options;

    hci_run();

    return 0;
}

static uint8_t hci_whitelist_add(bd_addr_type_t address_type, const bd_addr_t address){
    // check if already in list
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->le_whitelist);
    while (btstack_linked_list_iterator_has_next(&it)) {
        whitelist_entry_t *entry = (whitelist_entry_t *) btstack_linked_list_iterator_next(&it);
        if (entry->address_type != address_type) {
            continue;
        }
        if (memcmp(entry->address, address, 6) != 0) {
            continue;
        }

        // if already on controller:
        if ((entry->state & LE_WHITELIST_ON_CONTROLLER) != 0){
            if ((entry->state & LE_WHITELIST_REMOVE_FROM_CONTROLLER) != 0){
                // drop remove request
                entry->state = LE_WHITELIST_ON_CONTROLLER;
                return ERROR_CODE_SUCCESS;
            } else {
                // disallow as already on controller
                return ERROR_CODE_COMMAND_DISALLOWED;
            }
        } 

        // assume scheduled to add
		return ERROR_CODE_COMMAND_DISALLOWED;
    }

    // alloc and add to list
    whitelist_entry_t * entry = btstack_memory_whitelist_entry_get();
    if (!entry) return BTSTACK_MEMORY_ALLOC_FAILED;
    entry->address_type = address_type;
    (void)memcpy(entry->address, address, 6);
    entry->state = LE_WHITELIST_ADD_TO_CONTROLLER;
    btstack_linked_list_add(&hci_stack->le_whitelist, (btstack_linked_item_t*) entry);
    return ERROR_CODE_SUCCESS;
}

static uint8_t hci_whitelist_remove(bd_addr_type_t address_type, const bd_addr_t address){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->le_whitelist);
    while (btstack_linked_list_iterator_has_next(&it)){
        whitelist_entry_t * entry = (whitelist_entry_t*) btstack_linked_list_iterator_next(&it);
        if (entry->address_type != address_type) {
            continue;
        }
        if (memcmp(entry->address, address, 6) != 0) {
            continue;
        }
        if (entry->state & LE_WHITELIST_ON_CONTROLLER){
            // remove from controller if already present
            entry->state |= LE_WHITELIST_REMOVE_FROM_CONTROLLER;
        }  else {
            // directly remove entry from whitelist
            btstack_linked_list_iterator_remove(&it);
            btstack_memory_whitelist_entry_free(entry);
        }
        return ERROR_CODE_SUCCESS;
    }
    return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
}

static void hci_whitelist_clear(void){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->le_whitelist);
    while (btstack_linked_list_iterator_has_next(&it)){
        whitelist_entry_t * entry = (whitelist_entry_t*) btstack_linked_list_iterator_next(&it);
        if (entry->state & LE_WHITELIST_ON_CONTROLLER){
            // remove from controller if already present
            entry->state |= LE_WHITELIST_REMOVE_FROM_CONTROLLER;
            continue;
        }
        // directly remove entry from whitelist
        btstack_linked_list_iterator_remove(&it);
        btstack_memory_whitelist_entry_free(entry);
    }
}

/**
 * @brief Clear Whitelist
 * @return 0 if ok
 */
uint8_t gap_whitelist_clear(void){
    hci_whitelist_clear();
    hci_run();
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Add Device to Whitelist
 * @param address_typ
 * @param address
 * @return 0 if ok
 */
uint8_t gap_whitelist_add(bd_addr_type_t address_type, const bd_addr_t address){
    uint8_t status = hci_whitelist_add(address_type, address);
    if (status){
        return status;
    }
    hci_run();
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Remove Device from Whitelist
 * @param address_typ
 * @param address
 * @return 0 if ok
 */
uint8_t gap_whitelist_remove(bd_addr_type_t address_type, const bd_addr_t address){
    uint8_t status = hci_whitelist_remove(address_type, address);
    if (status){
        return status;
    }
    hci_run();
    return ERROR_CODE_SUCCESS;
}

#ifdef ENABLE_LE_CENTRAL
/**
 * @brief Connect with Whitelist
 * @note Explicit whitelist management and this connect with whitelist replace deprecated gap_auto_connection_* functions
 * @return - if ok
 */
uint8_t gap_connect_with_whitelist(void){
    if (hci_stack->le_connecting_request != LE_CONNECTING_IDLE){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    hci_stack->le_connecting_request = LE_CONNECTING_WHITELIST;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Auto Connection Establishment - Start Connecting to device
 * @param address_typ
 * @param address
 * @return 0 if ok
 */
uint8_t gap_auto_connection_start(bd_addr_type_t address_type, const bd_addr_t address){
    if (hci_stack->le_connecting_request == LE_CONNECTING_DIRECT){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    uint8_t status = hci_whitelist_add(address_type, address);
    if (status == BTSTACK_MEMORY_ALLOC_FAILED) {
        return status;
    }

    hci_stack->le_connecting_request = LE_CONNECTING_WHITELIST;

    hci_run();
    return ERROR_CODE_SUCCESS;
}

/**
 * @brief Auto Connection Establishment - Stop Connecting to device
 * @param address_typ
 * @param address
 * @return 0 if ok
 */
uint8_t gap_auto_connection_stop(bd_addr_type_t address_type, const bd_addr_t address){
    if (hci_stack->le_connecting_request == LE_CONNECTING_DIRECT){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    hci_whitelist_remove(address_type, address);
    if (btstack_linked_list_empty(&hci_stack->le_whitelist)){
        hci_stack->le_connecting_request = LE_CONNECTING_IDLE;
    }
    hci_run();
    return 0;
}

/**
 * @brief Auto Connection Establishment - Stop everything
 * @note  Convenience function to stop all active auto connection attempts
 */
uint8_t gap_auto_connection_stop_all(void){
    if (hci_stack->le_connecting_request == LE_CONNECTING_DIRECT) {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    hci_whitelist_clear();
    hci_stack->le_connecting_request = LE_CONNECTING_IDLE;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint16_t gap_le_connection_interval(hci_con_handle_t con_handle){
    hci_connection_t * conn = hci_connection_for_handle(con_handle);
    if (!conn) return 0;
    return conn->le_connection_interval;
}
#endif
#endif

#ifdef ENABLE_CLASSIC 
/**
 * @brief Set Extended Inquiry Response data
 * @param eir_data size HCI_EXTENDED_INQUIRY_RESPONSE_DATA_LEN (240) bytes, is not copied make sure memory is accessible during stack startup
 * @note has to be done before stack starts up
 */
void gap_set_extended_inquiry_response(const uint8_t * data){
    hci_stack->eir_data = data;
    hci_stack->gap_tasks_classic |= GAP_TASK_SET_EIR_DATA;
    hci_run();
}

/**
 * @brief Start GAP Classic Inquiry
 * @param duration in 1.28s units
 * @return 0 if ok
 * @events: GAP_EVENT_INQUIRY_RESULT, GAP_EVENT_INQUIRY_COMPLETE
 */
int gap_inquiry_start(uint8_t duration_in_1280ms_units){
    if (hci_stack->state != HCI_STATE_WORKING) return ERROR_CODE_COMMAND_DISALLOWED;
    if (hci_stack->inquiry_state != GAP_INQUIRY_STATE_IDLE) return ERROR_CODE_COMMAND_DISALLOWED;
    if ((duration_in_1280ms_units < GAP_INQUIRY_DURATION_MIN) || (duration_in_1280ms_units > GAP_INQUIRY_DURATION_MAX)){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    hci_stack->inquiry_state = duration_in_1280ms_units;
    hci_stack->inquiry_max_period_length = 0;
    hci_stack->inquiry_min_period_length = 0;
    hci_run();
    return 0;
}

uint8_t gap_inquiry_periodic_start(uint8_t duration, uint16_t max_period_length, uint16_t min_period_length){
    if (hci_stack->state != HCI_STATE_WORKING)                return ERROR_CODE_COMMAND_DISALLOWED;
    if (hci_stack->inquiry_state != GAP_INQUIRY_STATE_IDLE)   return ERROR_CODE_COMMAND_DISALLOWED;
    if (duration < GAP_INQUIRY_DURATION_MIN)                  return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    if (duration > GAP_INQUIRY_DURATION_MAX)                  return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    if (max_period_length < GAP_INQUIRY_MAX_PERIODIC_LEN_MIN) return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;;
    if (min_period_length < GAP_INQUIRY_MIN_PERIODIC_LEN_MIN) return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;;

    hci_stack->inquiry_state = duration;
    hci_stack->inquiry_max_period_length = max_period_length;
    hci_stack->inquiry_min_period_length = min_period_length;
    hci_run();
    return 0;
}

/**
 * @brief Stop GAP Classic Inquiry
 * @return 0 if ok
 */
int gap_inquiry_stop(void){
    if ((hci_stack->inquiry_state >= GAP_INQUIRY_DURATION_MIN) && (hci_stack->inquiry_state <= GAP_INQUIRY_DURATION_MAX)) {
        // emit inquiry complete event, before it even started
        uint8_t event[] = { GAP_EVENT_INQUIRY_COMPLETE, 1, 0};
        hci_emit_event(event, sizeof(event), 1);
        return 0;
    }
    switch (hci_stack->inquiry_state){
        case GAP_INQUIRY_STATE_ACTIVE:
            hci_stack->inquiry_state = GAP_INQUIRY_STATE_W2_CANCEL;
            hci_run();
            return ERROR_CODE_SUCCESS;
        case GAP_INQUIRY_STATE_PERIODIC:
            hci_stack->inquiry_state = GAP_INQUIRY_STATE_W2_EXIT_PERIODIC;
            hci_run();
            return ERROR_CODE_SUCCESS;
        default:
            return ERROR_CODE_COMMAND_DISALLOWED;
    }
}

void gap_inquiry_set_lap(uint32_t lap){
    hci_stack->inquiry_lap = lap;
}

void gap_inquiry_set_scan_activity(uint16_t inquiry_scan_interval, uint16_t inquiry_scan_window){
    hci_stack->inquiry_scan_interval = inquiry_scan_interval;
    hci_stack->inquiry_scan_window   = inquiry_scan_window;
    hci_stack->gap_tasks_classic |= GAP_TASK_WRITE_INQUIRY_SCAN_ACTIVITY;
    hci_run();
}


/**
 * @brief Remote Name Request
 * @param addr
 * @param page_scan_repetition_mode
 * @param clock_offset only used when bit 15 is set
 * @events: HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE
 */
int gap_remote_name_request(const bd_addr_t addr, uint8_t page_scan_repetition_mode, uint16_t clock_offset){
    if (hci_stack->remote_name_state != GAP_REMOTE_NAME_STATE_IDLE) return ERROR_CODE_COMMAND_DISALLOWED;
    (void)memcpy(hci_stack->remote_name_addr, addr, 6);
    hci_stack->remote_name_page_scan_repetition_mode = page_scan_repetition_mode;
    hci_stack->remote_name_clock_offset = clock_offset;
    hci_stack->remote_name_state = GAP_REMOTE_NAME_STATE_W2_SEND;
    hci_run();
    return 0;
}

static int gap_pairing_set_state_and_run(const bd_addr_t addr, uint8_t state){
    hci_stack->gap_pairing_state = state;
    (void)memcpy(hci_stack->gap_pairing_addr, addr, 6);
    hci_run();
    return 0;
}

/**
 * @brief Legacy Pairing Pin Code Response for binary data / non-strings
 * @param addr
 * @param pin_data
 * @param pin_len
 * @return 0 if ok
 */
int gap_pin_code_response_binary(const bd_addr_t addr, const uint8_t * pin_data, uint8_t pin_len){
    if (hci_stack->gap_pairing_state != GAP_PAIRING_STATE_IDLE) return ERROR_CODE_COMMAND_DISALLOWED;
    hci_stack->gap_pairing_input.gap_pairing_pin = pin_data;
    hci_stack->gap_pairing_pin_len = pin_len;
    return gap_pairing_set_state_and_run(addr, GAP_PAIRING_STATE_SEND_PIN);
}

/**
 * @brief Legacy Pairing Pin Code Response
 * @param addr
 * @param pin
 * @return 0 if ok
 */
int gap_pin_code_response(const bd_addr_t addr, const char * pin){
    return gap_pin_code_response_binary(addr, (const uint8_t*) pin, (uint8_t) strlen(pin));
}

/**
 * @brief Abort Legacy Pairing
 * @param addr
 * @param pin
 * @return 0 if ok
 */
int gap_pin_code_negative(bd_addr_t addr){
    if (hci_stack->gap_pairing_state != GAP_PAIRING_STATE_IDLE) return ERROR_CODE_COMMAND_DISALLOWED;
    return gap_pairing_set_state_and_run(addr, GAP_PAIRING_STATE_SEND_PIN_NEGATIVE);
}

/**
 * @brief SSP Passkey Response
 * @param addr
 * @param passkey
 * @return 0 if ok
 */
int gap_ssp_passkey_response(const bd_addr_t addr, uint32_t passkey){
    if (hci_stack->gap_pairing_state != GAP_PAIRING_STATE_IDLE) return ERROR_CODE_COMMAND_DISALLOWED;
    hci_stack->gap_pairing_input.gap_pairing_passkey = passkey;
    return gap_pairing_set_state_and_run(addr, GAP_PAIRING_STATE_SEND_PASSKEY);
}

/**
 * @brief Abort SSP Passkey Entry/Pairing
 * @param addr
 * @param pin
 * @return 0 if ok
 */
int gap_ssp_passkey_negative(const bd_addr_t addr){
    if (hci_stack->gap_pairing_state != GAP_PAIRING_STATE_IDLE) return ERROR_CODE_COMMAND_DISALLOWED;
    return gap_pairing_set_state_and_run(addr, GAP_PAIRING_STATE_SEND_PASSKEY_NEGATIVE);
}

/**
 * @brief Accept SSP Numeric Comparison
 * @param addr
 * @param passkey
 * @return 0 if ok
 */
int gap_ssp_confirmation_response(const bd_addr_t addr){
    if (hci_stack->gap_pairing_state != GAP_PAIRING_STATE_IDLE) return ERROR_CODE_COMMAND_DISALLOWED;
    return gap_pairing_set_state_and_run(addr, GAP_PAIRING_STATE_SEND_CONFIRMATION);
}

/**
 * @brief Abort SSP Numeric Comparison/Pairing
 * @param addr
 * @param pin
 * @return 0 if ok
 */
int gap_ssp_confirmation_negative(const bd_addr_t addr){
    if (hci_stack->gap_pairing_state != GAP_PAIRING_STATE_IDLE) return ERROR_CODE_COMMAND_DISALLOWED;
    return gap_pairing_set_state_and_run(addr, GAP_PAIRING_STATE_SEND_CONFIRMATION_NEGATIVE);
}

#if defined(ENABLE_EXPLICIT_IO_CAPABILITIES_REPLY) || defined(ENABLE_EXPLICIT_LINK_KEY_REPLY)
static uint8_t gap_set_auth_flag_and_run(const bd_addr_t addr, hci_authentication_flags_t flag){
    hci_connection_t * conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
    if (!conn) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    connectionSetAuthenticationFlags(conn, flag);
    hci_run();
    return ERROR_CODE_SUCCESS;
}
#endif

#ifdef ENABLE_EXPLICIT_IO_CAPABILITIES_REPLY
uint8_t gap_ssp_io_capabilities_response(const bd_addr_t addr){
    return gap_set_auth_flag_and_run(addr, AUTH_FLAG_SEND_IO_CAPABILITIES_REPLY);
}

uint8_t gap_ssp_io_capabilities_negative(const bd_addr_t addr){
    return gap_set_auth_flag_and_run(addr, AUTH_FLAG_SEND_IO_CAPABILITIES_NEGATIVE_REPLY);
}
#endif

#ifdef ENABLE_CLASSIC_PAIRING_OOB
/**
 * @brief Report Remote OOB Data
 * @param bd_addr
 * @param c_192 Simple Pairing Hash C derived from P-192 public key
 * @param r_192 Simple Pairing Randomizer derived from P-192 public key
 * @param c_256 Simple Pairing Hash C derived from P-256 public key
 * @param r_256 Simple Pairing Randomizer derived from P-256 public key
 */
uint8_t gap_ssp_remote_oob_data(const bd_addr_t addr, const uint8_t * c_192, const uint8_t * r_192, const uint8_t * c_256, const uint8_t * r_256){
    hci_connection_t * connection = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    connection->classic_oob_c_192 = c_192;
    connection->classic_oob_r_192 = r_192;

    // ignore P-256 if not supported by us
    if (hci_stack->secure_connections_active){
        connection->classic_oob_c_256 = c_256;
        connection->classic_oob_r_256 = r_256;
    }

    return ERROR_CODE_SUCCESS;
}
/**
 * @brief Generate new OOB data
 * @note OOB data will be provided in GAP_EVENT_LOCAL_OOB_DATA and be used in future pairing procedures
 */
void gap_ssp_generate_oob_data(void){
    hci_stack->classic_read_local_oob_data = true;
    hci_run();
}

#endif

#ifdef ENABLE_EXPLICIT_LINK_KEY_REPLY
uint8_t gap_send_link_key_response(const bd_addr_t addr, link_key_t link_key, link_key_type_t type){
    hci_connection_t * connection = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL);
    if (connection == NULL) {
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    memcpy(connection->link_key, link_key, sizeof(link_key_t));
    connection->link_key_type = type;

    return gap_set_auth_flag_and_run(addr, AUTH_FLAG_HANDLE_LINK_KEY_REQUEST);
}

#endif // ENABLE_EXPLICIT_LINK_KEY_REPLY
/**
 * @brief Set inquiry mode: standard, with RSSI, with RSSI + Extended Inquiry Results. Has to be called before power on.
 * @param inquiry_mode see bluetooth_defines.h
 */
void hci_set_inquiry_mode(inquiry_mode_t inquiry_mode){
    hci_stack->inquiry_mode = inquiry_mode;
}

/** 
 * @brief Configure Voice Setting for use with SCO data in HSP/HFP
 */
void hci_set_sco_voice_setting(uint16_t voice_setting){
    hci_stack->sco_voice_setting = voice_setting;
}

/**
 * @brief Get SCO Voice Setting
 * @return current voice setting
 */
uint16_t hci_get_sco_voice_setting(void){
    return hci_stack->sco_voice_setting;
}

static int hci_have_usb_transport(void){
    if (!hci_stack->hci_transport) return 0;
    const char * transport_name = hci_stack->hci_transport->name;
    if (!transport_name) return 0;
    return (transport_name[0] == 'H') && (transport_name[1] == '2');
}

/** @brief Get SCO packet length for current SCO Voice setting
 *  @note  Using SCO packets of the exact length is required for USB transfer
 *  @return Length of SCO packets in bytes (not audio frames)
 */
uint16_t hci_get_sco_packet_length(void){
    uint16_t sco_packet_length = 0;

#ifdef ENABLE_SCO_OVER_HCI
    // Transparent = mSBC => 1, CVSD with 16-bit samples requires twice as much bytes
    int multiplier;
    if (((hci_stack->sco_voice_setting_active & 0x03) != 0x03) &&
        ((hci_stack->sco_voice_setting_active & 0x20) == 0x20)) {
        multiplier = 2;
    } else {
        multiplier = 1;
    }


    if (hci_have_usb_transport()){
        // see Core Spec for H2 USB Transfer. 
        // 3 byte SCO header + 24 bytes per connection
        int num_sco_connections = btstack_max(1, hci_number_sco_connections());
        sco_packet_length = 3 + 24 * num_sco_connections * multiplier;
    } else {
        // 3 byte SCO header + SCO packet size over the air (60 bytes)
        sco_packet_length = 3 + 60 * multiplier;
        // assert that it still fits inside an SCO buffer
        if (sco_packet_length > (hci_stack->sco_data_packet_length + 3)){
            sco_packet_length = 3 + 60;
        }
    }
#endif

#ifdef HAVE_SCO_TRANSPORT
    // Transparent = mSBC => 1, CVSD with 16-bit samples requires twice as much bytes
    int multiplier = ((hci_stack->sco_voice_setting_active & 0x03) == 0x03) ? 1 : 2;
    sco_packet_length = 3 + 60 * multiplier;
#endif
    return sco_packet_length;
}

/**
* @brief Sets the master/slave policy
* @param policy (0: attempt to become master, 1: let connecting device decide)
*/
void hci_set_master_slave_policy(uint8_t policy){
    hci_stack->master_slave_policy = policy;
}

#endif

HCI_STATE hci_get_state(void){
    return hci_stack->state;
}

#ifdef ENABLE_CLASSIC
void gap_register_classic_connection_filter(int (*accept_callback)(bd_addr_t addr, hci_link_type_t link_type)){
    hci_stack->gap_classic_accept_callback = accept_callback;
}
#endif

/**
 * @brief Set callback for Bluetooth Hardware Error
 */
void hci_set_hardware_error_callback(void (*fn)(uint8_t error)){
    hci_stack->hardware_error_callback = fn;
}

void hci_disconnect_all(void){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hci_connection_t * con = (hci_connection_t*) btstack_linked_list_iterator_next(&it);
        if (con->state == SENT_DISCONNECT) continue;
        con->state = SEND_DISCONNECT;
    }
    hci_run();
}

uint16_t hci_get_manufacturer(void){
    return hci_stack->manufacturer;
}

#ifdef ENABLE_BLE
static sm_connection_t * sm_get_connection_for_handle(hci_con_handle_t con_handle){
    hci_connection_t * hci_con = hci_connection_for_handle(con_handle);
    if (!hci_con) return NULL;
    return &hci_con->sm_connection;
}

// extracted from sm.c to allow enabling of l2cap le data channels without adding sm.c to the build
// without sm.c default values from create_connection_for_bd_addr_and_type() resulg in non-encrypted, not-authenticated
#endif

uint8_t gap_encryption_key_size(hci_con_handle_t con_handle){
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (hci_connection == NULL) return 0;
    if (hci_is_le_connection(hci_connection)){
#ifdef ENABLE_BLE
        sm_connection_t * sm_conn = &hci_connection->sm_connection;
        if (sm_conn->sm_connection_encrypted) {
            return sm_conn->sm_actual_encryption_key_size;
        }
#endif
    } else {
#ifdef ENABLE_CLASSIC
        if ((hci_connection->authentication_flags & AUTH_FLAG_CONNECTION_ENCRYPTED)){
            return hci_connection->encryption_key_size;
        }
#endif
    }
    return 0;
}

bool gap_authenticated(hci_con_handle_t con_handle){
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (hci_connection == NULL) return false;

    switch (hci_connection->address_type){
#ifdef ENABLE_BLE
        case BD_ADDR_TYPE_LE_PUBLIC:
        case BD_ADDR_TYPE_LE_RANDOM:
            if (hci_connection->sm_connection.sm_connection_encrypted == 0) return 0; // unencrypted connection cannot be authenticated
            return hci_connection->sm_connection.sm_connection_authenticated != 0;
#endif
#ifdef ENABLE_CLASSIC
        case BD_ADDR_TYPE_SCO:
        case BD_ADDR_TYPE_ACL:
            return gap_authenticated_for_link_key_type(hci_connection->link_key_type);
#endif
        default:
            return false;
    }
}

bool gap_secure_connection(hci_con_handle_t con_handle){
    hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
    if (hci_connection == NULL) return 0;

    switch (hci_connection->address_type){
#ifdef ENABLE_BLE
        case BD_ADDR_TYPE_LE_PUBLIC:
        case BD_ADDR_TYPE_LE_RANDOM:
            if (hci_connection->sm_connection.sm_connection_encrypted == 0) return false; // unencrypted connection cannot be authenticated
            return hci_connection->sm_connection.sm_connection_sc != 0;
#endif
#ifdef ENABLE_CLASSIC
        case BD_ADDR_TYPE_SCO:
        case BD_ADDR_TYPE_ACL:
            return gap_secure_connection_for_link_key_type(hci_connection->link_key_type);
#endif
        default:
            return false;
    }
}

bool gap_bonded(hci_con_handle_t con_handle){
	hci_connection_t * hci_connection = hci_connection_for_handle(con_handle);
	if (hci_connection == NULL) return 0;

#ifdef ENABLE_CLASSIC
	link_key_t link_key;
	link_key_type_t link_key_type;
#endif
	switch (hci_connection->address_type){
#ifdef ENABLE_BLE
		case BD_ADDR_TYPE_LE_PUBLIC:
		case BD_ADDR_TYPE_LE_RANDOM:
			return hci_connection->sm_connection.sm_le_db_index >= 0;
#endif
#ifdef ENABLE_CLASSIC
		case BD_ADDR_TYPE_SCO:
		case BD_ADDR_TYPE_ACL:
			return hci_stack->link_key_db && hci_stack->link_key_db->get_link_key(hci_connection->address, link_key, &link_key_type);
#endif
		default:
			return false;
	}
}

#ifdef ENABLE_BLE
authorization_state_t gap_authorization_state(hci_con_handle_t con_handle){
    sm_connection_t * sm_conn = sm_get_connection_for_handle(con_handle);
    if (!sm_conn) return AUTHORIZATION_UNKNOWN;     // wrong connection
    if (!sm_conn->sm_connection_encrypted)               return AUTHORIZATION_UNKNOWN; // unencrypted connection cannot be authorized
    if (!sm_conn->sm_connection_authenticated)           return AUTHORIZATION_UNKNOWN; // unauthenticatd connection cannot be authorized
    return sm_conn->sm_connection_authorization_state;
}
#endif

#ifdef ENABLE_CLASSIC
uint8_t gap_sniff_mode_enter(hci_con_handle_t con_handle, uint16_t sniff_min_interval, uint16_t sniff_max_interval, uint16_t sniff_attempt, uint16_t sniff_timeout){
    hci_connection_t * conn = hci_connection_for_handle(con_handle);
    if (!conn) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    conn->sniff_min_interval = sniff_min_interval;
    conn->sniff_max_interval = sniff_max_interval;
    conn->sniff_attempt = sniff_attempt;
    conn->sniff_timeout = sniff_timeout;
    hci_run();
    return 0;
}

/**
 * @brief Exit Sniff mode
 * @param con_handle
 @ @return 0 if ok
 */
uint8_t gap_sniff_mode_exit(hci_con_handle_t con_handle){
    hci_connection_t * conn = hci_connection_for_handle(con_handle);
    if (!conn) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    conn->sniff_min_interval = 0xffff;
    hci_run();
    return 0;
}

uint8_t gap_sniff_subrating_configure(hci_con_handle_t con_handle, uint16_t max_latency, uint16_t min_remote_timeout, uint16_t min_local_timeout){
    hci_connection_t * conn = hci_connection_for_handle(con_handle);
    if (!conn) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    conn->sniff_subrating_max_latency = max_latency;
    conn->sniff_subrating_min_remote_timeout = min_remote_timeout;
    conn->sniff_subrating_min_local_timeout = min_local_timeout;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_qos_set(hci_con_handle_t con_handle, hci_service_type_t service_type, uint32_t token_rate, uint32_t peak_bandwidth, uint32_t latency, uint32_t delay_variation){
    hci_connection_t * conn = hci_connection_for_handle(con_handle);
    if (!conn) return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    conn->qos_service_type = service_type;
    conn->qos_token_rate = token_rate;
    conn->qos_peak_bandwidth = peak_bandwidth;
    conn->qos_latency = latency;
    conn->qos_delay_variation = delay_variation;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

void gap_set_page_scan_activity(uint16_t page_scan_interval, uint16_t page_scan_window){
    hci_stack->new_page_scan_interval = page_scan_interval;
    hci_stack->new_page_scan_window = page_scan_window;
    hci_stack->gap_tasks_classic |= GAP_TASK_WRITE_PAGE_SCAN_ACTIVITY;
    hci_run();
}

void gap_set_page_scan_type(page_scan_type_t page_scan_type){
    hci_stack->new_page_scan_type = (uint8_t) page_scan_type;
    hci_stack->gap_tasks_classic |= GAP_TASK_WRITE_PAGE_SCAN_TYPE;
    hci_run();
}

void gap_set_page_timeout(uint16_t page_timeout){
    hci_stack->page_timeout = page_timeout;
    hci_stack->gap_tasks_classic |= GAP_TASK_WRITE_PAGE_TIMEOUT;
    hci_run();
}

#endif

#ifdef ENABLE_LE_PRIVACY_ADDRESS_RESOLUTION
void hci_load_le_device_db_entry_into_resolving_list(uint16_t le_device_db_index){
    if (le_device_db_index >= MAX_NUM_RESOLVING_LIST_ENTRIES) return;
    if (le_device_db_index >= le_device_db_max_count()) return;
    uint8_t offset = le_device_db_index >> 3;
    uint8_t mask = 1 << (le_device_db_index & 7);
    hci_stack->le_resolving_list_add_entries[offset] |= mask;
    hci_stack->le_resolving_list_set_privacy_mode[offset] |= mask;
    if (hci_stack->le_resolving_list_state == LE_RESOLVING_LIST_DONE){
    	// note: go back to remove entries, otherwise, a remove + add will skip the add
        hci_stack->le_resolving_list_state = LE_RESOLVING_LIST_UPDATES_ENTRIES;
    }
}

void hci_remove_le_device_db_entry_from_resolving_list(uint16_t le_device_db_index){
	if (le_device_db_index >= MAX_NUM_RESOLVING_LIST_ENTRIES) return;
	if (le_device_db_index >= le_device_db_max_count()) return;
	uint8_t offset = le_device_db_index >> 3;
	uint8_t mask = 1 << (le_device_db_index & 7);
	hci_stack->le_resolving_list_remove_entries[offset] |= mask;
	if (hci_stack->le_resolving_list_state == LE_RESOLVING_LIST_DONE){
		hci_stack->le_resolving_list_state = LE_RESOLVING_LIST_UPDATES_ENTRIES;
	}
}

uint8_t gap_load_resolving_list_from_le_device_db(void){
    if (hci_command_supported(SUPPORTED_HCI_COMMAND_LE_SET_ADDRESS_RESOLUTION_ENABLE) == false){
		return ERROR_CODE_UNSUPPORTED_FEATURE_OR_PARAMETER_VALUE;
	}
	if (hci_stack->le_resolving_list_state != LE_RESOLVING_LIST_SEND_ENABLE_ADDRESS_RESOLUTION){
		// restart le resolving list update
		hci_stack->le_resolving_list_state = LE_RESOLVING_LIST_READ_SIZE;
	}
	return ERROR_CODE_SUCCESS;
}

void gap_set_peer_privacy_mode(le_privacy_mode_t privacy_mode ){
    hci_stack->le_privacy_mode = privacy_mode;
}
#endif

#ifdef ENABLE_BLE
#ifdef ENABLE_LE_CENTRAL
#ifdef ENABLE_LE_EXTENDED_ADVERTISING

static uint8_t hci_periodic_advertiser_list_add(bd_addr_type_t address_type, const bd_addr_t address, uint8_t advertising_sid){
    // check if already in list
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->le_periodic_advertiser_list);
    while (btstack_linked_list_iterator_has_next(&it)) {
        periodic_advertiser_list_entry_t *entry = (periodic_advertiser_list_entry_t *) btstack_linked_list_iterator_next(&it);
        if (entry->sid != advertising_sid) {
            continue;
        }
        if (entry->address_type != address_type) {
            continue;
        }
        if (memcmp(entry->address, address, 6) != 0) {
            continue;
        }
        // disallow if already scheduled to add
        if ((entry->state & LE_PERIODIC_ADVERTISER_LIST_ENTRY_ADD_TO_CONTROLLER) != 0){
            return ERROR_CODE_COMMAND_DISALLOWED;
        }
        // still on controller, but scheduled to remove -> re-add
        entry->state |= LE_PERIODIC_ADVERTISER_LIST_ENTRY_ADD_TO_CONTROLLER;
        return ERROR_CODE_SUCCESS;
    }
    // alloc and add to list
    periodic_advertiser_list_entry_t * entry = btstack_memory_periodic_advertiser_list_entry_get();
    if (!entry) return BTSTACK_MEMORY_ALLOC_FAILED;
    entry->sid = advertising_sid;
    entry->address_type = address_type;
    (void)memcpy(entry->address, address, 6);
    entry->state = LE_PERIODIC_ADVERTISER_LIST_ENTRY_ADD_TO_CONTROLLER;
    btstack_linked_list_add(&hci_stack->le_periodic_advertiser_list, (btstack_linked_item_t*) entry);
    return ERROR_CODE_SUCCESS;
}

static uint8_t hci_periodic_advertiser_list_remove(bd_addr_type_t address_type, const bd_addr_t address, uint8_t advertising_sid){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->le_periodic_advertiser_list);
    while (btstack_linked_list_iterator_has_next(&it)){
        periodic_advertiser_list_entry_t * entry = (periodic_advertiser_list_entry_t*) btstack_linked_list_iterator_next(&it);
        if (entry->sid != advertising_sid) {
            continue;
        }
        if (entry->address_type != address_type) {
            continue;
        }
        if (memcmp(entry->address, address, 6) != 0) {
            continue;
        }
        if (entry->state & LE_PERIODIC_ADVERTISER_LIST_ENTRY_ON_CONTROLLER){
            // remove from controller if already present
            entry->state |= LE_PERIODIC_ADVERTISER_LIST_ENTRY_REMOVE_FROM_CONTROLLER;
        }  else {
            // directly remove entry from whitelist
            btstack_linked_list_iterator_remove(&it);
            btstack_memory_periodic_advertiser_list_entry_free(entry);
        }
        return ERROR_CODE_SUCCESS;
    }
    return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
}

static void hci_periodic_advertiser_list_clear(void){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->le_periodic_advertiser_list);
    while (btstack_linked_list_iterator_has_next(&it)){
        periodic_advertiser_list_entry_t * entry = (periodic_advertiser_list_entry_t*) btstack_linked_list_iterator_next(&it);
        if (entry->state & LE_PERIODIC_ADVERTISER_LIST_ENTRY_ON_CONTROLLER){
            // remove from controller if already present
            entry->state |= LE_PERIODIC_ADVERTISER_LIST_ENTRY_REMOVE_FROM_CONTROLLER;
            continue;
        }
        // directly remove entry from whitelist
        btstack_linked_list_iterator_remove(&it);
        btstack_memory_periodic_advertiser_list_entry_free(entry);
    }
}

uint8_t gap_periodic_advertiser_list_clear(void){
    hci_periodic_advertiser_list_clear();
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_periodic_advertiser_list_add(bd_addr_type_t address_type, const bd_addr_t address, uint8_t advertising_sid){
    uint8_t status = hci_periodic_advertiser_list_add(address_type, address, advertising_sid);
    if (status){
        return status;
    }
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_periodic_advertiser_list_remove(bd_addr_type_t address_type, const bd_addr_t address, uint8_t advertising_sid){
    uint8_t status = hci_periodic_advertiser_list_remove(address_type, address, advertising_sid);
    if (status){
        return status;
    }
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_periodic_advertising_create_sync(uint8_t options, uint8_t advertising_sid, bd_addr_type_t advertiser_address_type,
                                             bd_addr_t advertiser_address, uint16_t skip, uint16_t sync_timeout, uint8_t sync_cte_type){
    // abort if already active
    if (hci_stack->le_periodic_sync_request != LE_CONNECTING_IDLE) {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    // store request
    hci_stack->le_periodic_sync_request = ((options & 0) != 0) ? LE_CONNECTING_WHITELIST : LE_CONNECTING_DIRECT;
    hci_stack->le_periodic_sync_options = options;
    hci_stack->le_periodic_sync_advertising_sid = advertising_sid;
    hci_stack->le_periodic_sync_advertiser_address_type = advertiser_address_type;
    memcpy(hci_stack->le_periodic_sync_advertiser_address, advertiser_address, 6);
    hci_stack->le_periodic_sync_skip = skip;
    hci_stack->le_periodic_sync_timeout = sync_timeout;
    hci_stack->le_periodic_sync_cte_type = sync_cte_type;

    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_periodic_advertising_create_sync_cancel(void){
    // abort if not requested
    if (hci_stack->le_periodic_sync_request == LE_CONNECTING_IDLE) {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    hci_stack->le_periodic_sync_request = LE_CONNECTING_IDLE;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_periodic_advertising_terminate_sync(uint16_t sync_handle){
    if (hci_stack->le_periodic_terminate_sync_handle != HCI_CON_HANDLE_INVALID){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    hci_stack->le_periodic_terminate_sync_handle = sync_handle;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

#endif
#endif
#ifdef ENABLE_LE_ISOCHRONOUS_STREAMS
static hci_iso_stream_t *
hci_iso_stream_create(hci_iso_type_t iso_type, hci_iso_stream_state_t state, uint8_t group_id, uint8_t stream_id) {
    hci_iso_stream_t * iso_stream = btstack_memory_hci_iso_stream_get();
    if (iso_stream != NULL){
        iso_stream->iso_type = iso_type;
        iso_stream->state = state;
        iso_stream->group_id = group_id;
        iso_stream->stream_id = stream_id;
        iso_stream->cis_handle = HCI_CON_HANDLE_INVALID;
        iso_stream->acl_handle = HCI_CON_HANDLE_INVALID;
        btstack_linked_list_add(&hci_stack->iso_streams, (btstack_linked_item_t*) iso_stream);
    }
    return iso_stream;
}

static hci_iso_stream_t * hci_iso_stream_for_con_handle(hci_con_handle_t con_handle){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->iso_streams);
    while (btstack_linked_list_iterator_has_next(&it)){
        hci_iso_stream_t * iso_stream = (hci_iso_stream_t *) btstack_linked_list_iterator_next(&it);
        if (iso_stream->cis_handle == con_handle ) {
            return iso_stream;
        }
    }
    return NULL;
}

static void hci_iso_stream_finalize(hci_iso_stream_t * iso_stream){
    log_info("hci_iso_stream_finalize con_handle 0x%04x, group_id 0x%02x", iso_stream->cis_handle, iso_stream->group_id);
    btstack_linked_list_remove(&hci_stack->iso_streams, (btstack_linked_item_t*) iso_stream);
    btstack_memory_hci_iso_stream_free(iso_stream);
}

static void hci_iso_stream_finalize_by_type_and_group_id(hci_iso_type_t iso_type, uint8_t group_id) {
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->iso_streams);
    while (btstack_linked_list_iterator_has_next(&it)){
        hci_iso_stream_t * iso_stream = (hci_iso_stream_t *) btstack_linked_list_iterator_next(&it);
        if ((iso_stream->group_id == group_id) &&
            (iso_stream->iso_type == iso_type)){
            btstack_linked_list_iterator_remove(&it);
            btstack_memory_hci_iso_stream_free(iso_stream);
        }
    }
}

static void hci_iso_stream_requested_finalize(uint8_t group_id) {
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->iso_streams);
    while (btstack_linked_list_iterator_has_next(&it)){
        hci_iso_stream_t * iso_stream = (hci_iso_stream_t *) btstack_linked_list_iterator_next(&it);
        if ((iso_stream->state == HCI_ISO_STREAM_STATE_REQUESTED ) &&
            (iso_stream->group_id == group_id)){
            btstack_linked_list_iterator_remove(&it);
            btstack_memory_hci_iso_stream_free(iso_stream);
        }
    }
}
static void hci_iso_stream_requested_confirm(uint8_t big_handle){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->iso_streams);
    while (btstack_linked_list_iterator_has_next(&it)){
        hci_iso_stream_t * iso_stream = (hci_iso_stream_t *) btstack_linked_list_iterator_next(&it);
        if ( iso_stream->state == HCI_ISO_STREAM_STATE_REQUESTED ) {
            iso_stream->state = HCI_ISO_STREAM_STATE_W4_ESTABLISHED;
        }
    }
}

static bool hci_iso_sdu_complete(uint8_t * packet, uint16_t size){
    uint8_t  sdu_ts_flag = (packet[1] >> 6) & 1;
    uint16_t sdu_len_offset = 6 + (sdu_ts_flag * 4);
    uint16_t sdu_len = little_endian_read_16(packet, sdu_len_offset) & 0x0fff;
    return (sdu_len_offset + 2 + sdu_len) == size;
}

static void hci_iso_packet_handler(uint8_t * packet, uint16_t size){
    if (hci_stack->iso_packet_handler == NULL) {
        return;
    }
    if (size < 4) {
        return;
    }

    // parse header
    uint16_t conn_handle_and_flags = little_endian_read_16(packet, 0);
    uint16_t iso_data_len = little_endian_read_16(packet, 2);
    hci_con_handle_t cis_handle = (hci_con_handle_t) (conn_handle_and_flags & 0xfff);
    hci_iso_stream_t * iso_stream = hci_iso_stream_for_con_handle(cis_handle);
    uint8_t pb_flag = (conn_handle_and_flags >> 12) & 3;

    // assert packet is complete
    if ((iso_data_len + 4u) != size){
        return;
    }

    if ((pb_flag & 0x01) == 0){
        if (pb_flag == 0x02){
            // The ISO_Data_Load field contains a header and a complete SDU.
            if (hci_iso_sdu_complete(packet, size)) {
                (hci_stack->iso_packet_handler)(HCI_ISO_DATA_PACKET, 0, packet, size);
            }
        } else {
            // The ISO_Data_Load field contains a header and the first fragment of a fragmented SDU.
            if (iso_stream == NULL){
                return;
            }
            if (size > HCI_ISO_PAYLOAD_SIZE){
                return;
            }
            memcpy(iso_stream->reassembly_buffer, packet, size);
            // fix pb_flag
            iso_stream->reassembly_buffer[1] = (iso_stream->reassembly_buffer[1] & 0xcf) | 0x20;
            iso_stream->reassembly_pos = size;
        }
    } else {
        // iso_data_load contains continuation or last fragment of an SDU
        uint8_t  ts_flag = (conn_handle_and_flags >> 14) & 1;
        if (ts_flag != 0){
           return;
        }
        // append fragment
        if (iso_stream == NULL){
            return;
        }
        if (iso_stream->reassembly_pos == 0){
            return;
        }
        if ((iso_stream->reassembly_pos + iso_data_len) > size){
            // reset reassembly buffer
            iso_stream->reassembly_pos = 0;
            return;
        }
        memcpy(&iso_stream->reassembly_buffer[iso_stream->reassembly_pos], &packet[4], iso_data_len);
        iso_stream->reassembly_pos += iso_data_len;

        // deliver if last fragment and SDU complete
        if (pb_flag == 0x03){
            if (hci_iso_sdu_complete(iso_stream->reassembly_buffer, iso_stream->reassembly_pos)){
                (hci_stack->iso_packet_handler)(HCI_ISO_DATA_PACKET, 0, iso_stream->reassembly_buffer, iso_stream->reassembly_pos);
            }
            iso_stream->reassembly_pos = 0;
        }
    }
}

static void hci_emit_big_created(const le_audio_big_t * big, uint8_t status){
    uint8_t event [6 + (MAX_NR_BIS * 2)];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_META_GAP;
    event[pos++] = 4 + (2 * big->num_bis);
    event[pos++] = GAP_SUBEVENT_BIG_CREATED;
    event[pos++] = status;
    event[pos++] = big->big_handle;
    event[pos++] = big->num_bis;
    uint8_t i;
    for (i=0;i<big->num_bis;i++){
        little_endian_store_16(event, pos, big->bis_con_handles[i]);
        pos += 2;
    }
    hci_emit_event(event, pos, 0);
}

static void hci_emit_cig_created(const le_audio_cig_t * cig, uint8_t status){
    uint8_t event [6 + (MAX_NR_CIS * 2)];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_META_GAP;
    event[pos++] = 4 + (2 * cig->num_cis);
    event[pos++] = GAP_SUBEVENT_CIG_CREATED;
    event[pos++] = status;
    event[pos++] = cig->cig_id;
    event[pos++] = cig->num_cis;
    uint8_t i;
    for (i=0;i<cig->num_cis;i++){
        little_endian_store_16(event, pos, cig->cis_con_handles[i]);
        pos += 2;
    }
    hci_emit_event(event, pos, 0);
}

static void hci_emit_cis_created(uint8_t status, uint8_t cig_id, uint8_t cis_id, hci_con_handle_t cis_con_handle,
                     hci_con_handle_t acl_con_handle) {
    uint8_t event [10];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_META_GAP;
    event[pos++] = 8;
    event[pos++] = GAP_SUBEVENT_CIS_CREATED;
    event[pos++] = status;
    event[pos++] = cig_id;
    event[pos++] = cis_id;
    little_endian_store_16(event, pos, cis_con_handle);
    pos += 2;
    little_endian_store_16(event, pos, acl_con_handle);
    pos += 2;
    hci_emit_event(event, pos, 0);
}

// emits GAP_SUBEVENT_CIS_CREATED after calling hci_iso_finalize
static void hci_cis_handle_created(hci_iso_stream_t * iso_stream, uint8_t status){
    // cache data before finalizing struct
    uint8_t cig_id              = iso_stream->group_id;
    uint8_t cis_id              = iso_stream->stream_id;
    hci_con_handle_t cis_handle = iso_stream->cis_handle;
    hci_con_handle_t acl_handle = iso_stream->acl_handle;
    if (status != ERROR_CODE_SUCCESS){
        hci_iso_stream_finalize(iso_stream);
    }
    hci_emit_cis_created(status, cig_id, cis_id, cis_handle, acl_handle);
}

static void hci_emit_big_terminated(const le_audio_big_t * big){
    uint8_t event [4];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_META_GAP;
    event[pos++] = 2;
    event[pos++] = GAP_SUBEVENT_BIG_TERMINATED;
    event[pos++] = big->big_handle;
    hci_emit_event(event, pos, 0);
}

static void hci_emit_big_sync_created(const le_audio_big_sync_t * big_sync, uint8_t status){
    uint8_t event [6 + (MAX_NR_BIS * 2)];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_META_GAP;
    event[pos++] = 4;
    event[pos++] = GAP_SUBEVENT_BIG_SYNC_CREATED;
    event[pos++] = status;
    event[pos++] = big_sync->big_handle;
    event[pos++] = big_sync->num_bis;
    uint8_t i;
    for (i=0;i<big_sync->num_bis;i++){
        little_endian_store_16(event, pos, big_sync->bis_con_handles[i]);
        pos += 2;
    }
    hci_emit_event(event, pos, 0);
}

static void hci_emit_big_sync_stopped(uint8_t big_handle){
    uint8_t event [4];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_META_GAP;
    event[pos++] = 2;
    event[pos++] = GAP_SUBEVENT_BIG_SYNC_STOPPED;
    event[pos++] = big_handle;
    hci_emit_event(event, pos, 0);
}

static void hci_emit_bis_can_send_now(const le_audio_big_t *big, uint8_t bis_index) {
    uint8_t event[6];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_BIS_CAN_SEND_NOW;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = big->big_handle;
    event[pos++] = bis_index;
    little_endian_store_16(event, pos, big->bis_con_handles[bis_index]);
    hci_emit_event(&event[0], sizeof(event), 0);  // don't dump
}

static void hci_emit_cis_can_send_now(hci_con_handle_t cis_con_handle) {
    uint8_t event[4];
    uint16_t pos = 0;
    event[pos++] = HCI_EVENT_CIS_CAN_SEND_NOW;
    event[pos++] = sizeof(event) - 2;
    little_endian_store_16(event, pos, cis_con_handle);
    hci_emit_event(&event[0], sizeof(event), 0);  // don't dump
}

static le_audio_big_t * hci_big_for_handle(uint8_t big_handle){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->le_audio_bigs);
    while (btstack_linked_list_iterator_has_next(&it)){
        le_audio_big_t * big = (le_audio_big_t *) btstack_linked_list_iterator_next(&it);
        if ( big->big_handle == big_handle ) {
            return big;
        }
    }
    return NULL;
}

static le_audio_big_sync_t * hci_big_sync_for_handle(uint8_t big_handle){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->le_audio_big_syncs);
    while (btstack_linked_list_iterator_has_next(&it)){
        le_audio_big_sync_t * big_sync = (le_audio_big_sync_t *) btstack_linked_list_iterator_next(&it);
        if ( big_sync->big_handle == big_handle ) {
            return big_sync;
        }
    }
    return NULL;
}

void hci_set_num_iso_packets_to_queue(uint8_t num_packets){
    hci_stack->iso_packets_to_queue = num_packets;
}

static le_audio_cig_t * hci_cig_for_id(uint8_t cig_id){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->le_audio_cigs);
    while (btstack_linked_list_iterator_has_next(&it)){
        le_audio_cig_t * cig = (le_audio_cig_t *) btstack_linked_list_iterator_next(&it);
        if ( cig->cig_id == cig_id ) {
            return cig;
        }
    }
    return NULL;
}

static void hci_iso_notify_can_send_now(void){

    // BIG

    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->le_audio_bigs);
    while (btstack_linked_list_iterator_has_next(&it)){
        le_audio_big_t * big = (le_audio_big_t *) btstack_linked_list_iterator_next(&it);
        // track number completed packet timestamps
        if (big->num_completed_timestamp_current_valid){
            big->num_completed_timestamp_current_valid = false;
            if (big->num_completed_timestamp_previous_valid){
                // detect delayed sending of all BIS: tolerate up to 50% delayed event handling
                uint32_t iso_interval_missed_threshold_ms = big->params->sdu_interval_us * 3 / 2000;
                int32_t  num_completed_timestamp_delta_ms = btstack_time_delta(big->num_completed_timestamp_current_ms,
                                                                               big->num_completed_timestamp_previous_ms);
                if (num_completed_timestamp_delta_ms > iso_interval_missed_threshold_ms){
                    // to catch up, skip packet on all BIS
                    uint8_t i;
                    for (i=0;i<big->num_bis;i++){
                        hci_iso_stream_t * iso_stream = hci_iso_stream_for_con_handle(big->bis_con_handles[i]);
                        if (iso_stream){
                            iso_stream->num_packets_to_skip++;
                        }
                    }
                }
            }
            big->num_completed_timestamp_previous_valid = true;
            big->num_completed_timestamp_previous_ms = big->num_completed_timestamp_current_ms;
        }

        if (big->can_send_now_requested){
            // check if no outgoing iso packets pending and no can send now have to be emitted
            uint8_t i;
            bool can_send = true;
            uint8_t num_iso_queued_minimum = 0;
            for (i=0;i<big->num_bis;i++){
                hci_iso_stream_t * iso_stream = hci_iso_stream_for_con_handle(big->bis_con_handles[i]);
                if (iso_stream == NULL) continue;
                // handle case where individual ISO packet was sent too late:
                // for each additionally queued packet, a new one needs to get skipped
                if (i==0){
                    num_iso_queued_minimum = iso_stream->num_packets_sent;
                } else if (iso_stream->num_packets_sent > num_iso_queued_minimum){
                    uint8_t num_packets_to_skip = iso_stream->num_packets_sent - num_iso_queued_minimum;
                    iso_stream->num_packets_to_skip += num_packets_to_skip;
                    iso_stream->num_packets_sent    -= num_packets_to_skip;
                }
                // check if we can send now
                if  ((iso_stream->num_packets_sent >= hci_stack->iso_packets_to_queue) || (iso_stream->emit_ready_to_send)){
                    can_send = false;
                    break;
                }
            }
            if (can_send){
                // propagate can send now to individual streams
                big->can_send_now_requested = false;
                for (i=0;i<big->num_bis;i++){
                    hci_iso_stream_t * iso_stream = hci_iso_stream_for_con_handle(big->bis_con_handles[i]);
                    iso_stream->emit_ready_to_send = true;
                }
            }
        }
    }

    if (hci_stack->hci_packet_buffer_reserved) return;

    btstack_linked_list_iterator_init(&it, &hci_stack->le_audio_bigs);
    while (btstack_linked_list_iterator_has_next(&it)){
        le_audio_big_t * big = (le_audio_big_t *) btstack_linked_list_iterator_next(&it);
        // report bis ready
        uint8_t i;
        for (i=0;i<big->num_bis;i++){
            hci_iso_stream_t * iso_stream = hci_iso_stream_for_con_handle(big->bis_con_handles[i]);
            if ((iso_stream != NULL) && iso_stream->emit_ready_to_send){
                iso_stream->emit_ready_to_send = false;
                hci_emit_bis_can_send_now(big, i);
                break;
            }
        }
    }

    // CIS
    btstack_linked_list_iterator_init(&it, &hci_stack->iso_streams);
    while (btstack_linked_list_iterator_has_next(&it)) {
        hci_iso_stream_t *iso_stream = (hci_iso_stream_t *) btstack_linked_list_iterator_next(&it);
        if ((iso_stream->can_send_now_requested) &&
            (iso_stream->num_packets_sent < hci_stack->iso_packets_to_queue)){
            iso_stream->can_send_now_requested = false;
            hci_emit_cis_can_send_now(iso_stream->cis_handle);
        }
    }
}

uint8_t gap_big_create(le_audio_big_t * storage, le_audio_big_params_t * big_params){
    if (hci_big_for_handle(big_params->big_handle) != NULL){
        return ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS;
    }
    if (big_params->num_bis == 0){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if (big_params->num_bis > MAX_NR_BIS){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    // reserve ISO Streams
    uint8_t i;
    uint8_t status = ERROR_CODE_SUCCESS;
    for (i=0;i<big_params->num_bis;i++){
        hci_iso_stream_t * iso_stream = hci_iso_stream_create(HCI_ISO_TYPE_BIS, HCI_ISO_STREAM_STATE_REQUESTED, big_params->big_handle, i);
        if (iso_stream == NULL) {
            status = ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
            break;
        }
    }

    // free structs on error
    if (status != ERROR_CODE_SUCCESS){
        hci_iso_stream_finalize_by_type_and_group_id(HCI_ISO_TYPE_BIS, big_params->big_handle);
        return status;
    }

    le_audio_big_t * big = storage;
    big->big_handle = big_params->big_handle;
    big->params = big_params;
    big->state = LE_AUDIO_BIG_STATE_CREATE;
    big->num_bis = big_params->num_bis;
    btstack_linked_list_add(&hci_stack->le_audio_bigs, (btstack_linked_item_t *) big);

    hci_run();

    return ERROR_CODE_SUCCESS;
}

uint8_t gap_big_sync_create(le_audio_big_sync_t * storage, le_audio_big_sync_params_t * big_sync_params){
    if (hci_big_sync_for_handle(big_sync_params->big_handle) != NULL){
        return ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS;
    }
    if (big_sync_params->num_bis == 0){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if (big_sync_params->num_bis > MAX_NR_BIS){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    le_audio_big_sync_t * big_sync = storage;
    big_sync->big_handle = big_sync_params->big_handle;
    big_sync->params = big_sync_params;
    big_sync->state = LE_AUDIO_BIG_STATE_CREATE;
    big_sync->num_bis = big_sync_params->num_bis;
    btstack_linked_list_add(&hci_stack->le_audio_big_syncs, (btstack_linked_item_t *) big_sync);

    hci_run();

    return ERROR_CODE_SUCCESS;
}

uint8_t gap_big_terminate(uint8_t big_handle){
    le_audio_big_t * big = hci_big_for_handle(big_handle);
    if (big == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    switch (big->state){
        case LE_AUDIO_BIG_STATE_CREATE:
            btstack_linked_list_remove(&hci_stack->le_audio_bigs, (btstack_linked_item_t *) big);
            hci_emit_big_terminated(big);
            break;
        case LE_AUDIO_BIG_STATE_W4_SETUP_ISO_PATH:
            big->state = LE_AUDIO_BIG_STATE_W4_SETUP_ISO_PATH_THEN_TERMINATE;
            break;
        case LE_AUDIO_BIG_STATE_W4_ESTABLISHED:
        case LE_AUDIO_BIG_STATE_SETUP_ISO_PATH:
        case LE_AUDIO_BIG_STATE_ACTIVE:
            big->state = LE_AUDIO_BIG_STATE_TERMINATE;
            hci_run();
            break;
        default:
            return ERROR_CODE_COMMAND_DISALLOWED;
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_big_sync_terminate(uint8_t big_handle){
    le_audio_big_sync_t * big_sync = hci_big_sync_for_handle(big_handle);
    if (big_sync == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    switch (big_sync->state){
        case LE_AUDIO_BIG_STATE_CREATE:
            btstack_linked_list_remove(&hci_stack->le_audio_big_syncs, (btstack_linked_item_t *) big_sync);
            hci_emit_big_sync_stopped(big_handle);
            break;
        case LE_AUDIO_BIG_STATE_W4_SETUP_ISO_PATH:
            big_sync->state = LE_AUDIO_BIG_STATE_W4_SETUP_ISO_PATH_THEN_TERMINATE;
            break;
        case LE_AUDIO_BIG_STATE_W4_ESTABLISHED:
        case LE_AUDIO_BIG_STATE_SETUP_ISO_PATH:
        case LE_AUDIO_BIG_STATE_ACTIVE:
            big_sync->state = LE_AUDIO_BIG_STATE_TERMINATE;
            hci_run();
            break;
        default:
            return ERROR_CODE_COMMAND_DISALLOWED;
    }
    return ERROR_CODE_SUCCESS;
}

uint8_t hci_request_bis_can_send_now_events(uint8_t big_handle){
    le_audio_big_t * big = hci_big_for_handle(big_handle);
    if (big == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if (big->state != LE_AUDIO_BIG_STATE_ACTIVE){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    big->can_send_now_requested = true;
    hci_iso_notify_can_send_now();
    return ERROR_CODE_SUCCESS;
}

uint8_t hci_request_cis_can_send_now_events(hci_con_handle_t cis_con_handle){
    hci_iso_stream_t * iso_stream = hci_iso_stream_for_con_handle(cis_con_handle);
    if (iso_stream == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }
    if ((iso_stream->iso_type != HCI_ISO_TYPE_CIS) && (iso_stream->state != HCI_ISO_STREAM_STATE_ESTABLISHED)) {
        return ERROR_CODE_COMMAND_DISALLOWED;
    }
    iso_stream->can_send_now_requested = true;
    hci_iso_notify_can_send_now();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_cig_create(le_audio_cig_t * storage, le_audio_cig_params_t * cig_params){
    if (hci_cig_for_id(cig_params->cig_id) != NULL){
        return ERROR_CODE_ACL_CONNECTION_ALREADY_EXISTS;
    }
    if (cig_params->num_cis == 0){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }
    if (cig_params->num_cis > MAX_NR_BIS){
        return ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS;
    }

    // reserve ISO Streams
    uint8_t i;
    uint8_t status = ERROR_CODE_SUCCESS;
    for (i=0;i<cig_params->num_cis;i++){
        hci_iso_stream_t * iso_stream = hci_iso_stream_create(HCI_ISO_TYPE_CIS,HCI_ISO_STREAM_STATE_REQUESTED, cig_params->cig_id, i);
        if (iso_stream == NULL) {
            status = ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
            break;
        }
    }

    // free structs on error
    if (status != ERROR_CODE_SUCCESS){
        hci_iso_stream_finalize_by_type_and_group_id(HCI_ISO_TYPE_CIS, cig_params->cig_id);
        return status;
    }

    le_audio_cig_t * cig = storage;
    cig->cig_id = cig_params->cig_id;
    cig->num_cis = cig_params->num_cis;
    cig->params = cig_params;
    cig->state = LE_AUDIO_CIG_STATE_CREATE;
    for (i=0;i<cig->num_cis;i++){
        cig->cis_con_handles[i] = HCI_CON_HANDLE_INVALID;
        cig->acl_con_handles[i] = HCI_CON_HANDLE_INVALID;
        cig->cis_setup_active[i] = false;
        cig->cis_established[i] = false;
    }
    btstack_linked_list_add(&hci_stack->le_audio_cigs, (btstack_linked_item_t *) cig);

    hci_run();

    return ERROR_CODE_SUCCESS;
}

uint8_t gap_cis_create(uint8_t cig_handle, hci_con_handle_t cis_con_handles [], hci_con_handle_t acl_con_handles []){
    le_audio_cig_t * cig = hci_cig_for_id(cig_handle);
    if (cig == NULL){
        return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
    }

    if (cig->state != LE_AUDIO_CIG_STATE_W4_CIS_REQUEST){
        return ERROR_CODE_COMMAND_DISALLOWED;
    }

    // store ACL Connection Handles
    uint8_t i;
    for (i=0;i<cig->num_cis;i++){
        // check that all con handles exist and store
        hci_con_handle_t cis_handle = cis_con_handles[i];
        if (cis_handle == HCI_CON_HANDLE_INVALID){
            return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
        }
        uint8_t j;
        bool found = false;
        for (j=0;j<cig->num_cis;j++){
            if (cig->cis_con_handles[j] == cis_handle){
                cig->acl_con_handles[j] = acl_con_handles[j];
                hci_iso_stream_t * iso_stream = hci_iso_stream_for_con_handle(cis_handle);
                btstack_assert(iso_stream != NULL);
                iso_stream->acl_handle = acl_con_handles[j];
                found = true;
                break;
            }
        }
        if (!found){
            return ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER;
        }
    }

    cig->state = LE_AUDIO_CIG_STATE_CREATE_CIS;
    hci_run();

    return ERROR_CODE_SUCCESS;
}

static uint8_t hci_cis_accept_or_reject(hci_con_handle_t cis_handle, hci_iso_stream_state_t state){
    hci_iso_stream_t * iso_stream = hci_iso_stream_for_con_handle(cis_handle);
    if (iso_stream == NULL){
        // if we got a CIS Request but fail to allocate a hci_iso_stream_t object, we won't find it here
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    // set next state and continue
    iso_stream->state = state;
    hci_run();
    return ERROR_CODE_SUCCESS;
}

uint8_t gap_cis_accept(hci_con_handle_t cis_con_handle){
    return hci_cis_accept_or_reject(cis_con_handle, HCI_ISO_STREAM_W2_ACCEPT);
}

uint8_t gap_cis_reject(hci_con_handle_t cis_con_handle){
    return hci_cis_accept_or_reject(cis_con_handle, HCI_ISO_STREAM_W2_REJECT);
}


#endif
#endif /* ENABLE_BLE */

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
void hci_setup_test_connections_fuzz(void){
    hci_connection_t * conn;

    // default address: 66:55:44:33:00:01
    bd_addr_t addr = { 0x66, 0x55, 0x44, 0x33, 0x00, 0x00};

    // setup Controller info
    hci_stack->num_cmd_packets = 255;
    hci_stack->acl_packets_total_num = 255;

    // setup incoming Classic ACL connection with con handle 0x0001, 66:55:44:33:22:01
    addr[5] = 0x01;
    conn = create_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL, HCI_ROLE_SLAVE);
    conn->con_handle = addr[5];
    conn->state = RECEIVED_CONNECTION_REQUEST;
    conn->sm_connection.sm_role = HCI_ROLE_SLAVE;

    // setup incoming Classic SCO connection with con handle 0x0002
    addr[5] = 0x02;
    conn = create_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_SCO, HCI_ROLE_SLAVE);
    conn->con_handle = addr[5];
    conn->state = RECEIVED_CONNECTION_REQUEST;
    conn->sm_connection.sm_role = HCI_ROLE_SLAVE;

    // setup ready Classic ACL connection with con handle 0x0003
    addr[5] = 0x03;
    conn = create_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_ACL, HCI_ROLE_SLAVE);
    conn->con_handle = addr[5];
    conn->state = OPEN;
    conn->sm_connection.sm_role = HCI_ROLE_SLAVE;

    // setup ready Classic SCO connection with con handle 0x0004
    addr[5] = 0x04;
    conn = create_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_SCO, HCI_ROLE_SLAVE);
    conn->con_handle = addr[5];
    conn->state = OPEN;
    conn->sm_connection.sm_role = HCI_ROLE_SLAVE;

    // setup ready LE ACL connection with con handle 0x005 and public address
    addr[5] = 0x05;
    conn = create_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_LE_PUBLIC, HCI_ROLE_SLAVE);
    conn->con_handle = addr[5];
    conn->state = OPEN;
    conn->sm_connection.sm_role = HCI_ROLE_SLAVE;
    conn->sm_connection.sm_connection_encrypted = 1;
}

void hci_free_connections_fuzz(void){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        hci_connection_t * con = (hci_connection_t*) btstack_linked_list_iterator_next(&it);
        btstack_linked_list_iterator_remove(&it);
        btstack_memory_hci_connection_free(con);
    }
}
void hci_simulate_working_fuzz(void){
    hci_stack->le_scanning_param_update = false;
    hci_init_done();
    hci_stack->num_cmd_packets = 255;
}
#endif
