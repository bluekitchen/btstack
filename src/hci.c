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

/*
 *  hci.c
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#include "btstack_config.h"


#ifdef HAVE_EMBEDDED_TICK
#include "btstack_run_loop_embedded.h"
#endif

#ifdef HAVE_PLATFORM_IPHONE_OS
#include "../port/ios/src/btstack_control_iphone.h"
#endif

#ifdef ENABLE_BLE
#include "gap.h"
#endif

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_linked_list.h"
#include "btstack_memory.h"
#include "gap.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"


#define HCI_CONNECTION_TIMEOUT_MS 10000
#define HCI_RESET_RESEND_TIMEOUT_MS 200

// prototypes
static void hci_update_scan_enable(void);
static gap_security_level_t gap_security_level_for_connection(hci_connection_t * connection);
static void hci_connection_timeout_handler(btstack_timer_source_t *timer);
static void hci_connection_timestamp(hci_connection_t *connection);
static int  hci_power_control_on(void);
static void hci_power_control_off(void);
static void hci_state_reset(void);
static void hci_emit_connection_complete(bd_addr_t address, hci_con_handle_t con_handle, uint8_t status);
static void hci_emit_l2cap_check_timeout(hci_connection_t *conn);
static void hci_emit_disconnection_complete(hci_con_handle_t con_handle, uint8_t reason);
static void hci_emit_nr_connections_changed(void);
static void hci_emit_hci_open_failed(void);
static void hci_emit_discoverable_enabled(uint8_t enabled);
static void hci_emit_security_level(hci_con_handle_t con_handle, gap_security_level_t level);
static void hci_emit_dedicated_bonding_result(bd_addr_t address, uint8_t status);
static void hci_emit_event(uint8_t * event, uint16_t size, int dump);
static void hci_emit_acl_packet(uint8_t * packet, uint16_t size);
static void hci_notify_if_sco_can_send_now(void);
static void hci_run(void);
static int  hci_is_le_connection(hci_connection_t * connection);
static int  hci_number_free_acl_slots_for_connection_type( bd_addr_type_t address_type);
static int  hci_local_ssp_activated(void);
static int  hci_remote_ssp_supported(hci_con_handle_t con_handle);

#ifdef ENABLE_BLE
// called from test/ble_client/advertising_data_parser.c
void le_handle_advertisement_report(uint8_t *packet, int size);
static void hci_remove_from_whitelist(bd_addr_type_t address_type, bd_addr_t address);
#endif

// the STACK is here
#ifndef HAVE_MALLOC
static hci_stack_t   hci_stack_static;
#endif
static hci_stack_t * hci_stack = NULL;

// test helper
static uint8_t disable_l2cap_timeouts = 0;

/**
 * create connection for given address
 *
 * @return connection OR NULL, if no memory left
 */
static hci_connection_t * create_connection_for_bd_addr_and_type(bd_addr_t addr, bd_addr_type_t addr_type){
    log_info("create_connection_for_addr %s, type %x", bd_addr_to_str(addr), addr_type);
    hci_connection_t * conn = btstack_memory_hci_connection_get();
    if (!conn) return NULL;
    memset(conn, 0, sizeof(hci_connection_t));
    bd_addr_copy(conn->address, addr);
    conn->address_type = addr_type;
    conn->con_handle = 0xffff;
    conn->authentication_flags = AUTH_FLAGS_NONE;
    conn->bonding_flags = 0;
    conn->requested_security_level = LEVEL_0;
    btstack_run_loop_set_timer_handler(&conn->timeout, hci_connection_timeout_handler);
    btstack_run_loop_set_timer_context(&conn->timeout, conn);
    hci_connection_timestamp(conn);
    conn->acl_recombination_length = 0;
    conn->acl_recombination_pos = 0;
    conn->num_acl_packets_sent = 0;
    conn->num_sco_packets_sent = 0;
    conn->le_con_parameter_update_state = CON_PARAMETER_UPDATE_NONE;
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
hci_connection_t * hci_connection_for_bd_addr_and_type(bd_addr_t  addr, bd_addr_type_t addr_type){
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

static void hci_connection_timeout_handler(btstack_timer_source_t *timer){
    hci_connection_t * connection = (hci_connection_t *) btstack_run_loop_get_timer_context(timer);
#ifdef HAVE_EMBEDDED_TICK
    if (btstack_run_loop_embedded_get_ticks() > connection->timestamp + btstack_run_loop_embedded_ticks_for_ms(HCI_CONNECTION_TIMEOUT_MS)){
        // connections might be timed out
        hci_emit_l2cap_check_timeout(connection);
    }
#else 
    if (btstack_run_loop_get_time_ms() > connection->timestamp + HCI_CONNECTION_TIMEOUT_MS){
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


inline static void connectionSetAuthenticationFlags(hci_connection_t * conn, hci_authentication_flags_t flags){
    conn->authentication_flags = (hci_authentication_flags_t)(conn->authentication_flags | flags);
}

inline static void connectionClearAuthenticationFlags(hci_connection_t * conn, hci_authentication_flags_t flags){
    conn->authentication_flags = (hci_authentication_flags_t)(conn->authentication_flags & ~flags);
}


/**
 * add authentication flags and reset timer
 * @note: assumes classic connection
 * @note: bd_addr is passed in as litle endian uint8_t * as it is called from parsing packets
 */
static void hci_add_connection_flags_for_flipped_bd_addr(uint8_t *bd_addr, hci_authentication_flags_t flags){
    bd_addr_t addr;
    reverse_bd_addr(bd_addr, addr);
    hci_connection_t * conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_CLASSIC);
    if (conn) {
        connectionSetAuthenticationFlags(conn, flags);
        hci_connection_timestamp(conn);
    }
}

int  hci_authentication_active_for_handle(hci_con_handle_t handle){
    hci_connection_t * conn = hci_connection_for_handle(handle);
    if (!conn) return 0;
    if (conn->authentication_flags & LEGACY_PAIRING_ACTIVE) return 1;
    if (conn->authentication_flags & SSP_PAIRING_ACTIVE) return 1;
    return 0;
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

static int hci_is_le_connection(hci_connection_t * connection){
    return  connection->address_type == BD_ADDR_TYPE_LE_PUBLIC ||
    connection->address_type == BD_ADDR_TYPE_LE_RANDOM;
}

/**
 * count connections
 */
static int nr_hci_connections(void){
    int count = 0;
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) hci_stack->connections; it ; it = it->next, count++);
    return count;
}

static int hci_number_free_acl_slots_for_connection_type(bd_addr_type_t address_type){
    
    unsigned int num_packets_sent_classic = 0;
    unsigned int num_packets_sent_le = 0;

    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) hci_stack->connections; it ; it = it->next){
        hci_connection_t * connection = (hci_connection_t *) it;
        if (connection->address_type == BD_ADDR_TYPE_CLASSIC){
            num_packets_sent_classic += connection->num_acl_packets_sent;
        } else {
            num_packets_sent_le += connection->num_acl_packets_sent;
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

        case BD_ADDR_TYPE_CLASSIC:
            return free_slots_classic;

        default:
           if (hci_stack->le_acl_packets_total_num){
               return free_slots_le;
           }
           return free_slots_classic; 
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

static int hci_number_free_sco_slots(void){
    unsigned int num_sco_packets_sent  = 0;
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) hci_stack->connections; it ; it = it->next){
        hci_connection_t * connection = (hci_connection_t *) it;
        num_sco_packets_sent += connection->num_sco_packets_sent;
    }
    if (num_sco_packets_sent > hci_stack->sco_packets_total_num){
        log_info("hci_number_free_sco_slots:packets (%u) > total packets (%u)", num_sco_packets_sent, hci_stack->sco_packets_total_num);
        return 0;
    }
    // log_info("hci_number_free_sco_slots u", handle, num_sco_packets_sent);
    return hci_stack->sco_packets_total_num - num_sco_packets_sent;
}

// new functions replacing hci_can_send_packet_now[_using_packet_buffer]
int hci_can_send_command_packet_now(void){
    if (hci_stack->hci_packet_buffer_reserved) return 0;

    // check for async hci transport implementations
    if (hci_stack->hci_transport->can_send_packet_now){
        if (!hci_stack->hci_transport->can_send_packet_now(HCI_COMMAND_DATA_PACKET)){
            return 0;
        }
    }

    return hci_stack->num_cmd_packets > 0;
}

static int hci_transport_can_send_prepared_packet_now(uint8_t packet_type){
    // check for async hci transport implementations
    if (!hci_stack->hci_transport->can_send_packet_now) return 1;
    return hci_stack->hci_transport->can_send_packet_now(packet_type);
}

static int hci_can_send_prepared_acl_packet_for_address_type(bd_addr_type_t address_type){
    if (!hci_transport_can_send_prepared_packet_now(HCI_ACL_DATA_PACKET)) return 0;
    return hci_number_free_acl_slots_for_connection_type(address_type) > 0;
}

int hci_can_send_acl_classic_packet_now(void){
    if (hci_stack->hci_packet_buffer_reserved) return 0;
    return hci_can_send_prepared_acl_packet_for_address_type(BD_ADDR_TYPE_CLASSIC);
}

int hci_can_send_acl_le_packet_now(void){
    if (hci_stack->hci_packet_buffer_reserved) return 0;
    return hci_can_send_prepared_acl_packet_for_address_type(BD_ADDR_TYPE_LE_PUBLIC);
}

int hci_can_send_prepared_acl_packet_now(hci_con_handle_t con_handle) {
    if (!hci_transport_can_send_prepared_packet_now(HCI_ACL_DATA_PACKET)) return 0;
    return hci_number_free_acl_slots_for_handle(con_handle) > 0;
}

int hci_can_send_acl_packet_now(hci_con_handle_t con_handle){
    if (hci_stack->hci_packet_buffer_reserved) return 0;
    return hci_can_send_prepared_acl_packet_now(con_handle);
}

int hci_can_send_prepared_sco_packet_now(void){
    if (!hci_transport_can_send_prepared_packet_now(HCI_SCO_DATA_PACKET)) return 0;
    if (!hci_stack->synchronous_flow_control_enabled) return 1;
    return hci_number_free_sco_slots() > 0;    
}

int hci_can_send_sco_packet_now(void){
    if (hci_stack->hci_packet_buffer_reserved) return 0;
    return hci_can_send_prepared_sco_packet_now();
}

void hci_request_sco_can_send_now_event(void){
    hci_stack->sco_waiting_for_can_send_now = 1;
    hci_notify_if_sco_can_send_now();
}

// used for internal checks in l2cap.c
int hci_is_packet_buffer_reserved(void){
    return hci_stack->hci_packet_buffer_reserved;
}

// reserves outgoing packet buffer. @returns 1 if successful
int hci_reserve_packet_buffer(void){
    if (hci_stack->hci_packet_buffer_reserved) {
        log_error("hci_reserve_packet_buffer called but buffer already reserved");
        return 0;
    }
    hci_stack->hci_packet_buffer_reserved = 1;
    return 1;    
}

void hci_release_packet_buffer(void){
    hci_stack->hci_packet_buffer_reserved = 0;
}

// assumption: synchronous implementations don't provide can_send_packet_now as they don't keep the buffer after the call
static int hci_transport_synchronous(void){
    return hci_stack->hci_transport->can_send_packet_now == NULL;
}

static int hci_send_acl_packet_fragments(hci_connection_t *connection){

    // log_info("hci_send_acl_packet_fragments  %u/%u (con 0x%04x)", hci_stack->acl_fragmentation_pos, hci_stack->acl_fragmentation_total_size, connection->con_handle);

    // max ACL data packet length depends on connection type (LE vs. Classic) and available buffers
    uint16_t max_acl_data_packet_length = hci_stack->acl_data_packet_length;
    if (hci_is_le_connection(connection) && hci_stack->le_data_packets_length > 0){
        max_acl_data_packet_length = hci_stack->le_data_packets_length;
    }

    // testing: reduce buffer to minimum
    // max_acl_data_packet_length = 52;

    log_debug("hci_send_acl_packet_fragments entered");

    int err;
    // multiple packets could be send on a synchronous HCI transport
    while (1){

        log_debug("hci_send_acl_packet_fragments loop entered");

        // get current data
        const uint16_t acl_header_pos = hci_stack->acl_fragmentation_pos - 4;
        int current_acl_data_packet_length = hci_stack->acl_fragmentation_total_size - hci_stack->acl_fragmentation_pos;
        int more_fragments = 0;

        // if ACL packet is larger than Bluetooth packet buffer, only send max_acl_data_packet_length
        if (current_acl_data_packet_length > max_acl_data_packet_length){
            more_fragments = 1;
            current_acl_data_packet_length = max_acl_data_packet_length;
        }

        // copy handle_and_flags if not first fragment and update packet boundary flags to be 01 (continuing fragmnent)
        if (acl_header_pos > 0){
            uint16_t handle_and_flags = little_endian_read_16(hci_stack->hci_packet_buffer, 0);
            handle_and_flags = (handle_and_flags & 0xcfff) | (1 << 12);
            little_endian_store_16(hci_stack->hci_packet_buffer, acl_header_pos, handle_and_flags);
        }

        // update header len
        little_endian_store_16(hci_stack->hci_packet_buffer, acl_header_pos + 2, current_acl_data_packet_length);

        // count packet
        connection->num_acl_packets_sent++;
        log_debug("hci_send_acl_packet_fragments loop before send (more fragments %d)", more_fragments);

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
        err = hci_stack->hci_transport->send_packet(HCI_ACL_DATA_PACKET, packet, size);

        log_debug("hci_send_acl_packet_fragments loop after send (more fragments %d)", more_fragments);

        // done yet?
        if (!more_fragments) break;

        // can send more?
        if (!hci_can_send_prepared_acl_packet_now(connection->con_handle)) return err;
    }

    log_debug("hci_send_acl_packet_fragments loop over");

    // release buffer now for synchronous transport
    if (hci_transport_synchronous()){
        hci_release_packet_buffer();
        // notify upper stack that it might be possible to send again
        uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
        hci_emit_event(&event[0], sizeof(event), 0);  // don't dump
    }

    return err;
}

// pre: caller has reserved the packet buffer
int hci_send_acl_packet_buffer(int size){

    // log_info("hci_send_acl_packet_buffer size %u", size);

    if (!hci_stack->hci_packet_buffer_reserved) {
        log_error("hci_send_acl_packet_buffer called without reserving packet buffer");
        return 0;
    }

    uint8_t * packet = hci_stack->hci_packet_buffer;
    hci_con_handle_t con_handle = READ_ACL_CONNECTION_HANDLE(packet);

    // check for free places on Bluetooth module
    if (!hci_can_send_prepared_acl_packet_now(con_handle)) {
        log_error("hci_send_acl_packet_buffer called but no free ACL buffers on controller");
        hci_release_packet_buffer();
        return BTSTACK_ACL_BUFFERS_FULL;
    }

    hci_connection_t *connection = hci_connection_for_handle( con_handle);
    if (!connection) {
        log_error("hci_send_acl_packet_buffer called but no connection for handle 0x%04x", con_handle);
        hci_release_packet_buffer();
        return 0;
    }
    hci_connection_timestamp(connection);
    
    // hci_dump_packet( HCI_ACL_DATA_PACKET, 0, packet, size);

    // setup data
    hci_stack->acl_fragmentation_total_size = size;
    hci_stack->acl_fragmentation_pos = 4;   // start of L2CAP packet

    return hci_send_acl_packet_fragments(connection);
}

// pre: caller has reserved the packet buffer
int hci_send_sco_packet_buffer(int size){

    // log_info("hci_send_acl_packet_buffer size %u", size);

    if (!hci_stack->hci_packet_buffer_reserved) {
        log_error("hci_send_acl_packet_buffer called without reserving packet buffer");
        return 0;
    }

    uint8_t * packet = hci_stack->hci_packet_buffer;

    // skip checks in loopback mode
    if (!hci_stack->loopback_mode){
        hci_con_handle_t con_handle = READ_ACL_CONNECTION_HANDLE(packet);   // same for ACL and SCO

        // check for free places on Bluetooth module
        if (!hci_can_send_prepared_sco_packet_now()) {
            log_error("hci_send_sco_packet_buffer called but no free ACL buffers on controller");
            hci_release_packet_buffer();
            return BTSTACK_ACL_BUFFERS_FULL;
        }

        // track send packet in connection struct
        hci_connection_t *connection = hci_connection_for_handle( con_handle);
        if (!connection) {
            log_error("hci_send_sco_packet_buffer called but no connection for handle 0x%04x", con_handle);
            hci_release_packet_buffer();
            return 0;
        }
        connection->num_sco_packets_sent++;
    }

    hci_dump_packet( HCI_SCO_DATA_PACKET, 0, packet, size);
    int err = hci_stack->hci_transport->send_packet(HCI_SCO_DATA_PACKET, packet, size);

    if (hci_transport_synchronous()){
        hci_release_packet_buffer();
        // notify upper stack that it might be possible to send again
        uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
        hci_emit_event(&event[0], sizeof(event), 0);    // don't dump
    }

    return err;
}

static void acl_handler(uint8_t *packet, int size){

    // log_info("acl_handler: size %u", size);

    // get info
    hci_con_handle_t con_handle = READ_ACL_CONNECTION_HANDLE(packet);
    hci_connection_t *conn      = hci_connection_for_handle(con_handle);
    uint8_t  acl_flags          = READ_ACL_FLAGS(packet);
    uint16_t acl_length         = READ_ACL_LENGTH(packet);

    // ignore non-registered handle
    if (!conn){
        log_error( "hci.c: acl_handler called with non-registered handle %u!" , con_handle);
        return;
    }

    // assert packet is complete    
    if (acl_length + 4 != size){
        log_error("hci.c: acl_handler called with ACL packet of wrong size %d, expected %u => dropping packet", size, acl_length + 4);
        return;
    }

    // update idle timestamp
    hci_connection_timestamp(conn);
    
    // handle different packet types
    switch (acl_flags & 0x03) {
            
        case 0x01: // continuation fragment
            
            // sanity checks
            if (conn->acl_recombination_pos == 0) {
                log_error( "ACL Cont Fragment but no first fragment for handle 0x%02x", con_handle);
                return;
            }
            if (conn->acl_recombination_pos + acl_length > 4 + HCI_ACL_BUFFER_SIZE){
                log_error( "ACL Cont Fragment to large: combined packet %u > buffer size %u for handle 0x%02x",
                    conn->acl_recombination_pos + acl_length, 4 + HCI_ACL_BUFFER_SIZE, con_handle);
                conn->acl_recombination_pos = 0;
                return;
            }

            // append fragment payload (header already stored)
            memcpy(&conn->acl_recombination_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + conn->acl_recombination_pos], &packet[4], acl_length );
            conn->acl_recombination_pos += acl_length;
            
            // log_error( "ACL Cont Fragment: acl_len %u, combined_len %u, l2cap_len %u", acl_length,
            //        conn->acl_recombination_pos, conn->acl_recombination_length);  
            
            // forward complete L2CAP packet if complete. 
            if (conn->acl_recombination_pos >= conn->acl_recombination_length + 4 + 4){ // pos already incl. ACL header
                hci_emit_acl_packet(&conn->acl_recombination_buffer[HCI_INCOMING_PRE_BUFFER_SIZE], conn->acl_recombination_pos);
                // reset recombination buffer
                conn->acl_recombination_length = 0;
                conn->acl_recombination_pos = 0;
            }
            break;
            
        case 0x02: { // first fragment
            
            // sanity check
            if (conn->acl_recombination_pos) {
                log_error( "ACL First Fragment but data in buffer for handle 0x%02x, dropping stale fragments", con_handle);
                conn->acl_recombination_pos = 0;
            }

            // peek into L2CAP packet!
            uint16_t l2cap_length = READ_L2CAP_LENGTH( packet );

            // log_info( "ACL First Fragment: acl_len %u, l2cap_len %u", acl_length, l2cap_length);

            // compare fragment size to L2CAP packet size
            if (acl_length >= l2cap_length + 4){
                // forward fragment as L2CAP packet
                hci_emit_acl_packet(packet, acl_length + 4);
            } else {

                if (acl_length > HCI_ACL_BUFFER_SIZE){
                    log_error( "ACL First Fragment to large: fragment %u > buffer size %u for handle 0x%02x",
                        4 + acl_length, 4 + HCI_ACL_BUFFER_SIZE, con_handle);
                    return;
                }

                // store first fragment and tweak acl length for complete package
                memcpy(&conn->acl_recombination_buffer[HCI_INCOMING_PRE_BUFFER_SIZE], packet, acl_length + 4);
                conn->acl_recombination_pos    = acl_length + 4;
                conn->acl_recombination_length = l2cap_length;
                little_endian_store_16(conn->acl_recombination_buffer, HCI_INCOMING_PRE_BUFFER_SIZE + 2, l2cap_length +4);
            }
            break;
            
        } 
        default:
            log_error( "hci.c: acl_handler called with invalid packet boundary flags %u", acl_flags & 0x03);
            return;
    }
    
    // execute main loop
    hci_run();
}

static void hci_shutdown_connection(hci_connection_t *conn){
    log_info("Connection closed: handle 0x%x, %s", conn->con_handle, bd_addr_to_str(conn->address));

    btstack_run_loop_remove_timer(&conn->timeout);
    
    btstack_linked_list_remove(&hci_stack->connections, (btstack_linked_item_t *) conn);
    btstack_memory_hci_connection_free( conn );
    
    // now it's gone
    hci_emit_nr_connections_changed();
}

static const uint16_t packet_type_sizes[] = {
    0, HCI_ACL_2DH1_SIZE, HCI_ACL_3DH1_SIZE, HCI_ACL_DM1_SIZE,
    HCI_ACL_DH1_SIZE, 0, 0, 0,
    HCI_ACL_2DH3_SIZE, HCI_ACL_3DH3_SIZE, HCI_ACL_DM3_SIZE, HCI_ACL_DH3_SIZE,
    HCI_ACL_2DH5_SIZE, HCI_ACL_3DH5_SIZE, HCI_ACL_DM5_SIZE, HCI_ACL_DH5_SIZE
};
static const uint8_t  packet_type_feature_requirement_bit[] = {
     0, // 3 slot packets
     1, // 5 slot packets
    25, // EDR 2 mpbs
    26, // EDR 3 mbps
    39, // 3 slot EDR packts
    40, // 5 slot EDR packet
};
static const uint16_t packet_type_feature_packet_mask[] = {
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
        if (packet_type_sizes[i] == 0) continue;
        if (packet_type_sizes[i] <= buffer_size){
            packet_types |= 1 << i;
        }
    }
    // disable packet types due to missing local supported features
    for (i=0;i<sizeof(packet_type_feature_requirement_bit);i++){
        unsigned int bit_idx = packet_type_feature_requirement_bit[i];
        int feature_set = (local_supported_features[bit_idx >> 3] & (1<<(bit_idx & 7))) != 0;
        if (feature_set) continue;
        log_info("Features bit %02u is not set, removing packet types 0x%04x", bit_idx, packet_type_feature_packet_mask[i]);
        packet_types &= ~packet_type_feature_packet_mask[i];
    }
    // flip bits for "may not be used"
    packet_types ^= 0x3306;
    return packet_types;
}

uint16_t hci_usable_acl_packet_types(void){
    return hci_stack->packet_types;
}

uint8_t* hci_get_outgoing_packet_buffer(void){
    // hci packet buffer is >= acl data packet length
    return hci_stack->hci_packet_buffer;
}

uint16_t hci_max_acl_data_packet_length(void){
    return hci_stack->acl_data_packet_length;
}

int hci_extended_sco_link_supported(void){
    // No. 31, byte 3, bit 7
    return (hci_stack->local_supported_features[3] & (1 << 7)) != 0;
}

int hci_non_flushable_packet_boundary_flag_supported(void){
    // No. 54, byte 6, bit 6
    return (hci_stack->local_supported_features[6] & (1 << 6)) != 0;
}

static int gap_ssp_supported(void){
    // No. 51, byte 6, bit 3
    return (hci_stack->local_supported_features[6] & (1 << 3)) != 0;
}

static int hci_classic_supported(void){
    // No. 37, byte 4, bit 5, = No BR/EDR Support
    return (hci_stack->local_supported_features[4] & (1 << 5)) == 0;
}

static int hci_le_supported(void){
#ifdef ENABLE_BLE
    // No. 37, byte 4, bit 6 = LE Supported (Controller)
    return (hci_stack->local_supported_features[4] & (1 << 6)) != 0;
#else
    return 0;
#endif    
}

// get addr type and address used in advertisement packets
void gap_advertisements_get_address(uint8_t * addr_type, bd_addr_t  addr){
    *addr_type = hci_stack->adv_addr_type;
    if (hci_stack->adv_addr_type){
        memcpy(addr, hci_stack->adv_address, 6);
    } else {
        memcpy(addr, hci_stack->local_bd_addr, 6);
    }
}

#ifdef ENABLE_BLE
void le_handle_advertisement_report(uint8_t *packet, int size){
    int offset = 3;
    int num_reports = packet[offset];
    offset += 1;

    int i;
    // log_info("HCI: handle adv report with num reports: %d", num_reports);
    uint8_t event[12 + LE_ADVERTISING_DATA_SIZE]; // use upper bound to avoid var size automatic var
    for (i=0; i<num_reports;i++){
        uint8_t data_length = packet[offset + 8];
        uint8_t event_size = 10 + data_length;
        int pos = 0;
        event[pos++] = GAP_EVENT_ADVERTISING_REPORT;
        event[pos++] = event_size;
        memcpy(&event[pos], &packet[offset], 1+1+6); // event type + address type + address
        offset += 8;
        pos += 8;
        event[pos++] = packet[offset + 1 + data_length]; // rssi
        event[pos++] = packet[offset++]; //data_length;
        memcpy(&event[pos], &packet[offset], data_length);
        pos += data_length;
        offset += data_length + 1; // rssi
        hci_emit_event(event, pos, 1);
    }
}
#endif

static uint32_t hci_transport_uart_get_main_baud_rate(void){
    if (!hci_stack->config) return 0;
    uint32_t baud_rate = ((hci_transport_config_uart_t *)hci_stack->config)->baudrate_main;
    // Limit baud rate for Broadcom chipsets to 3 mbps
    if (hci_stack->manufacturer == COMPANY_ID_BROADCOM_CORPORATION && baud_rate > 3000000){
        baud_rate = 3000000;
    }
    return baud_rate;
}

static void hci_initialization_timeout_handler(btstack_timer_source_t * ds){
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
            // NOTE: explicit fallthrough to HCI_INIT_W4_CUSTOM_INIT_CSR_WARM_BOOT
        case HCI_INIT_W4_CUSTOM_INIT_CSR_WARM_BOOT:
            log_info("Resend HCI Reset - CSR Warm Boot");
            hci_stack->substate = HCI_INIT_SEND_RESET_CSR_WARM_BOOT;
            hci_stack->num_cmd_packets = 1;
            hci_run();
            break;
        case HCI_INIT_W4_SEND_BAUD_CHANGE:
            if (hci_stack->hci_transport->set_baudrate){
                uint32_t baud_rate = hci_transport_uart_get_main_baud_rate();
                log_info("Local baud rate change to %"PRIu32"(timeout handler)", baud_rate);
                hci_stack->hci_transport->set_baudrate(baud_rate);
            }
            // For CSR, HCI Reset is sent on new baud rate
            if (hci_stack->manufacturer == COMPANY_ID_CAMBRIDGE_SILICON_RADIO){
                hci_stack->substate = HCI_INIT_SEND_RESET_CSR_WARM_BOOT;
                hci_run();
            }
            break;
        default:
            break;
    }
}

static void hci_initializing_next_state(void){
    hci_stack->substate = (hci_substate_t )( ((int) hci_stack->substate) + 1);
}

// assumption: hci_can_send_command_packet_now() == true
static void hci_initializing_run(void){
    log_debug("hci_initializing_run: substate %u, can send %u", hci_stack->substate, hci_can_send_command_packet_now());
    switch (hci_stack->substate){
        case HCI_INIT_SEND_RESET:
            hci_state_reset();

#ifndef HAVE_PLATFORM_IPHONE_OS
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
        case HCI_INIT_SEND_BAUD_CHANGE: {
            uint32_t baud_rate = hci_transport_uart_get_main_baud_rate();
            hci_stack->chipset->set_baudrate_command(baud_rate, hci_stack->hci_packet_buffer);
            hci_stack->last_cmd_opcode = little_endian_read_16(hci_stack->hci_packet_buffer, 0);
            hci_stack->substate = HCI_INIT_W4_SEND_BAUD_CHANGE;
            hci_send_cmd_packet(hci_stack->hci_packet_buffer, 3 + hci_stack->hci_packet_buffer[2]);
            // STLC25000D: baudrate change happens within 0.5 s after command was send,
            // use timer to update baud rate after 100 ms (knowing exactly, when command was sent is non-trivial)
            if (hci_stack->manufacturer == COMPANY_ID_ST_MICROELECTRONICS){
                btstack_run_loop_set_timer(&hci_stack->timeout, HCI_RESET_RESEND_TIMEOUT_MS);
                btstack_run_loop_add_timer(&hci_stack->timeout);
            }
            break;
        }
        case HCI_INIT_SEND_BAUD_CHANGE_BCM: {
            uint32_t baud_rate = hci_transport_uart_get_main_baud_rate();
            hci_stack->chipset->set_baudrate_command(baud_rate, hci_stack->hci_packet_buffer);
            hci_stack->last_cmd_opcode = little_endian_read_16(hci_stack->hci_packet_buffer, 0);
            hci_stack->substate = HCI_INIT_W4_SEND_BAUD_CHANGE_BCM;
            hci_send_cmd_packet(hci_stack->hci_packet_buffer, 3 + hci_stack->hci_packet_buffer[2]);
            break;
        }
        case HCI_INIT_CUSTOM_INIT:
            // Custom initialization
            if (hci_stack->chipset && hci_stack->chipset->next_command){
                int valid_cmd = (*hci_stack->chipset->next_command)(hci_stack->hci_packet_buffer);
                if (valid_cmd){
                    int size = 3 + hci_stack->hci_packet_buffer[2];
                    hci_stack->last_cmd_opcode = little_endian_read_16(hci_stack->hci_packet_buffer, 0);
                    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, hci_stack->hci_packet_buffer, size);
                    switch (valid_cmd) {
                        case 1:
                        default:
                            hci_stack->substate = HCI_INIT_W4_CUSTOM_INIT;
                            break;
                        case 2: // CSR Warm Boot: Wait a bit, then send HCI Reset until HCI Command Complete
                            log_info("CSR Warm Boot");
                            btstack_run_loop_set_timer(&hci_stack->timeout, HCI_RESET_RESEND_TIMEOUT_MS);
                            btstack_run_loop_set_timer_handler(&hci_stack->timeout, hci_initialization_timeout_handler);
                            btstack_run_loop_add_timer(&hci_stack->timeout);
                            if (hci_stack->manufacturer == COMPANY_ID_CAMBRIDGE_SILICON_RADIO
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
                    }
                    hci_stack->hci_transport->send_packet(HCI_COMMAND_DATA_PACKET, hci_stack->hci_packet_buffer, size);
                    break;
                }
                log_info("Init script done");
            
                // Init script download causes baud rate to reset on Broadcom chipsets, restore UART baud rate if needed
                if (hci_stack->manufacturer == COMPANY_ID_BROADCOM_CORPORATION){
                    int need_baud_change = hci_stack->config
                        && hci_stack->chipset
                        && hci_stack->chipset->set_baudrate_command
                        && hci_stack->hci_transport->set_baudrate
                        && ((hci_transport_config_uart_t *)hci_stack->config)->baudrate_main;
                    if (need_baud_change) {
                        uint32_t baud_rate = ((hci_transport_config_uart_t *)hci_stack->config)->baudrate_init;
                        log_info("Local baud rate change to %"PRIu32" after init script (bcm)", baud_rate);
                        hci_stack->hci_transport->set_baudrate(baud_rate);
                    }
                }
            }
            // otherwise continue
            hci_stack->substate = HCI_INIT_W4_READ_LOCAL_SUPPORTED_COMMANDS;
            hci_send_cmd(&hci_read_local_supported_commands);
            break;            
        case HCI_INIT_READ_LOCAL_SUPPORTED_COMMANDS:
            log_info("Resend hci_read_local_supported_commands after CSR Warm Boot double reset");
            hci_stack->substate = HCI_INIT_W4_READ_LOCAL_SUPPORTED_COMMANDS;
            hci_send_cmd(&hci_read_local_supported_commands);
            break;       
        case HCI_INIT_SET_BD_ADDR:
            log_info("Set Public BD ADDR to %s", bd_addr_to_str(hci_stack->custom_bd_addr));
            hci_stack->chipset->set_bd_addr_command(hci_stack->custom_bd_addr, hci_stack->hci_packet_buffer);
            hci_stack->last_cmd_opcode = little_endian_read_16(hci_stack->hci_packet_buffer, 0);
            hci_stack->substate = HCI_INIT_W4_SET_BD_ADDR;
            hci_send_cmd_packet(hci_stack->hci_packet_buffer, 3 + hci_stack->hci_packet_buffer[2]);
            break;
        case HCI_INIT_READ_BD_ADDR:
            hci_stack->substate = HCI_INIT_W4_READ_BD_ADDR;
            hci_send_cmd(&hci_read_bd_addr);
            break;
        case HCI_INIT_READ_BUFFER_SIZE:
            hci_stack->substate = HCI_INIT_W4_READ_BUFFER_SIZE;
            hci_send_cmd(&hci_read_buffer_size);
            break;
        case HCI_INIT_READ_LOCAL_SUPPORTED_FEATURES:
            hci_stack->substate = HCI_INIT_W4_READ_LOCAL_SUPPORTED_FEATURES;
            hci_send_cmd(&hci_read_local_supported_features);
            break;                
        case HCI_INIT_SET_EVENT_MASK:
            hci_stack->substate = HCI_INIT_W4_SET_EVENT_MASK;
            if (hci_le_supported()){
                hci_send_cmd(&hci_set_event_mask,0xffffffff, 0x3FFFFFFF);
            } else {
                // Kensington Bluetooth 2.1 USB Dongle (CSR Chipset) returns an error for 0xffff... 
                hci_send_cmd(&hci_set_event_mask,0xffffffff, 0x1FFFFFFF);
            }
            break;
        case HCI_INIT_WRITE_SIMPLE_PAIRING_MODE:
            hci_stack->substate = HCI_INIT_W4_WRITE_SIMPLE_PAIRING_MODE;
            hci_send_cmd(&hci_write_simple_pairing_mode, hci_stack->ssp_enable);
            break;
        case HCI_INIT_WRITE_PAGE_TIMEOUT:
            hci_stack->substate = HCI_INIT_W4_WRITE_PAGE_TIMEOUT;
            hci_send_cmd(&hci_write_page_timeout, 0x6000);  // ca. 15 sec
            break;
        case HCI_INIT_WRITE_CLASS_OF_DEVICE:
            hci_stack->substate = HCI_INIT_W4_WRITE_CLASS_OF_DEVICE;
            hci_send_cmd(&hci_write_class_of_device, hci_stack->class_of_device);
            break;
        case HCI_INIT_WRITE_LOCAL_NAME:
            hci_stack->substate = HCI_INIT_W4_WRITE_LOCAL_NAME;
            if (hci_stack->local_name){
                hci_send_cmd(&hci_write_local_name, hci_stack->local_name);
            } else {
                char local_name[8+17+1];
                // BTstack 11:22:33:44:55:66
                memcpy(local_name, "BTstack ", 8);
                memcpy(&local_name[8], bd_addr_to_str(hci_stack->local_bd_addr), 17);   // strlen(bd_addr_to_str(...)) = 17
                local_name[8+17] = '\0';
                log_info("---> Name %s", local_name);
                hci_send_cmd(&hci_write_local_name, local_name);
            }
            break;
        case HCI_INIT_WRITE_EIR_DATA:
            hci_stack->substate = HCI_INIT_W4_WRITE_EIR_DATA;
            hci_send_cmd(&hci_write_extended_inquiry_response, 0, hci_stack->eir_data);                        
            break;
        case HCI_INIT_WRITE_INQUIRY_MODE:
            hci_stack->substate = HCI_INIT_W4_WRITE_INQUIRY_MODE;
            hci_send_cmd(&hci_write_inquiry_mode, (int) hci_stack->inquiry_mode);
            break;
        case HCI_INIT_WRITE_SCAN_ENABLE:
            hci_send_cmd(&hci_write_scan_enable, (hci_stack->connectable << 1) | hci_stack->discoverable); // page scan
            hci_stack->substate = HCI_INIT_W4_WRITE_SCAN_ENABLE;
            break;
        // only sent if ENABLE_SCO_OVER_HCI is defined
        case HCI_INIT_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE:
            hci_stack->substate = HCI_INIT_W4_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE;
            hci_send_cmd(&hci_write_synchronous_flow_control_enable, 1); // SCO tracking enabled
            break;
        case HCI_INIT_WRITE_DEFAULT_ERRONEOUS_DATA_REPORTING:
            hci_stack->substate = HCI_INIT_W4_WRITE_DEFAULT_ERRONEOUS_DATA_REPORTING;
            hci_send_cmd(&hci_write_default_erroneous_data_reporting, 1);
            break;
#ifdef ENABLE_BLE
        // LE INIT
        case HCI_INIT_LE_READ_BUFFER_SIZE:
            hci_stack->substate = HCI_INIT_W4_LE_READ_BUFFER_SIZE;
            hci_send_cmd(&hci_le_read_buffer_size);
            break;
        case HCI_INIT_WRITE_LE_HOST_SUPPORTED:
            // LE Supported Host = 1, Simultaneous Host = 0
            hci_stack->substate = HCI_INIT_W4_WRITE_LE_HOST_SUPPORTED;
            hci_send_cmd(&hci_write_le_host_supported, 1, 0);
            break;
        case HCI_INIT_READ_WHITE_LIST_SIZE:
            hci_stack->substate = HCI_INIT_W4_READ_WHITE_LIST_SIZE;
            hci_send_cmd(&hci_le_read_white_list_size);
            break;
        case HCI_INIT_LE_SET_SCAN_PARAMETERS:
            // LE Scan Parameters: active scanning, 300 ms interval, 30 ms window, public address, accept all advs
            hci_stack->substate = HCI_INIT_W4_LE_SET_SCAN_PARAMETERS;
            hci_send_cmd(&hci_le_set_scan_parameters, 1, 0x1e0, 0x30, 0, 0);
            break;
#endif
        default:
            return;
    }
}

static void hci_init_done(void){
    // done. tell the app
    log_info("hci_init_done -> HCI_STATE_WORKING");
    hci_stack->state = HCI_STATE_WORKING;
    hci_emit_state();
    hci_run();
}

static void hci_initializing_event_handler(uint8_t * packet, uint16_t size){
    uint8_t command_completed = 0;

    if (hci_event_packet_get_type(packet) == HCI_EVENT_COMMAND_COMPLETE){
        uint16_t opcode = little_endian_read_16(packet,3);
        if (opcode == hci_stack->last_cmd_opcode){
            command_completed = 1;
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
                command_completed = 1;
                log_debug("Command status error 0x%02x for expected opcode %04x at substate %u", status, opcode, hci_stack->substate);
            } else {
                log_info("Command status OK for expected opcode %04x, waiting for command complete", opcode);
            }
        } else {
            log_debug("Command status for opcode %04x, expected %04x", opcode, hci_stack->last_cmd_opcode);
        }
    }

    // Vendor == CSR
    if (hci_stack->substate == HCI_INIT_W4_CUSTOM_INIT && hci_event_packet_get_type(packet) == HCI_EVENT_VENDOR_SPECIFIC){
        // TODO: track actual command
        command_completed = 1;
    }

    // Vendor == Toshiba
    if (hci_stack->substate == HCI_INIT_W4_SEND_BAUD_CHANGE && hci_event_packet_get_type(packet) == HCI_EVENT_VENDOR_SPECIFIC){
        // TODO: track actual command
        command_completed = 1;
    }

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
            && hci_event_packet_get_type(packet) == HCI_EVENT_COMMAND_COMPLETE
            && hci_stack->substate == HCI_INIT_W4_SEND_READ_LOCAL_VERSION_INFORMATION){

        uint16_t opcode = little_endian_read_16(packet,3);
        if (opcode == hci_reset.opcode){
            hci_stack->substate = HCI_INIT_SEND_READ_LOCAL_VERSION_INFORMATION;
            return;
        }
    }

    // CSR & H5
    // Fix: Command Complete for HCI Reset in HCI_INIT_W4_SEND_READ_LOCAL_VERSION_INFORMATION trigger resend
    if (!command_completed
            && hci_event_packet_get_type(packet) == HCI_EVENT_COMMAND_COMPLETE
            && hci_stack->substate == HCI_INIT_W4_READ_LOCAL_SUPPORTED_COMMANDS){

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


    if (!command_completed) return;

    int need_baud_change = hci_stack->config
                        && hci_stack->chipset
                        && hci_stack->chipset->set_baudrate_command
                        && hci_stack->hci_transport->set_baudrate
                        && ((hci_transport_config_uart_t *)hci_stack->config)->baudrate_main;

    int need_addr_change = hci_stack->custom_bd_addr_set
                        && hci_stack->chipset
                        && hci_stack->chipset->set_bd_addr_command;

    switch(hci_stack->substate){
        case HCI_INIT_SEND_RESET:
            // on CSR with BCSP/H5, resend triggers resend of HCI Reset and leads to substate == HCI_INIT_SEND_RESET
            // fix: just correct substate and behave as command below
            hci_stack->substate = HCI_INIT_W4_SEND_RESET;
            btstack_run_loop_remove_timer(&hci_stack->timeout);
            break;
        case HCI_INIT_W4_SEND_RESET:
            btstack_run_loop_remove_timer(&hci_stack->timeout);
            break;
        case HCI_INIT_W4_SEND_READ_LOCAL_VERSION_INFORMATION:
            log_info("Received local version info, need baud change %d", need_baud_change);
            if (need_baud_change){
                hci_stack->substate = HCI_INIT_SEND_BAUD_CHANGE;
                return;
            }
            // skip baud change
            hci_stack->substate = HCI_INIT_CUSTOM_INIT;
            return;
        case HCI_INIT_W4_SEND_BAUD_CHANGE:
            // for STLC2500D, baud rate change already happened.
            // for others, baud rate gets changed now
            if ((hci_stack->manufacturer != COMPANY_ID_ST_MICROELECTRONICS) && need_baud_change){
                uint32_t baud_rate = hci_transport_uart_get_main_baud_rate();
                log_info("Local baud rate change to %"PRIu32"(w4_send_baud_change)", baud_rate);
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
        case HCI_INIT_W4_READ_LOCAL_SUPPORTED_COMMANDS:
            if (need_baud_change && hci_stack->manufacturer == COMPANY_ID_BROADCOM_CORPORATION){
                hci_stack->substate = HCI_INIT_SEND_BAUD_CHANGE_BCM;
                return;
            }
            if (need_addr_change){
                hci_stack->substate = HCI_INIT_SET_BD_ADDR;
                return;
            }
            hci_stack->substate = HCI_INIT_READ_BD_ADDR;
            return;
        case HCI_INIT_W4_SEND_BAUD_CHANGE_BCM:
            if (need_baud_change){
                uint32_t baud_rate = hci_transport_uart_get_main_baud_rate();
                log_info("Local baud rate change to %"PRIu32"(w4_send_baud_change_bcm))", baud_rate);
                hci_stack->hci_transport->set_baudrate(baud_rate);
            }
            if (need_addr_change){
                hci_stack->substate = HCI_INIT_SET_BD_ADDR;
                return;
            }
            hci_stack->substate = HCI_INIT_READ_BD_ADDR;
            return;            
        case HCI_INIT_W4_SET_BD_ADDR:
            // for STLC2500D, bd addr change only gets active after sending reset command
            if (hci_stack->manufacturer == COMPANY_ID_ST_MICROELECTRONICS){
                hci_stack->substate = HCI_INIT_SEND_RESET_ST_WARM_BOOT;
                return;
            }
            // skipping st warm boot
            hci_stack->substate = HCI_INIT_READ_BD_ADDR;
            return;
        case HCI_INIT_W4_SEND_RESET_ST_WARM_BOOT:
            hci_stack->substate = HCI_INIT_READ_BD_ADDR;
            return;
        case HCI_INIT_W4_READ_BD_ADDR:
            // only read buffer size if supported
            if (hci_stack->local_supported_commands[0] & 0x01) {
                hci_stack->substate = HCI_INIT_READ_BUFFER_SIZE;
                return;
            }
            // skipping read buffer size
            hci_stack->substate = HCI_INIT_READ_LOCAL_SUPPORTED_FEATURES;
            return;
        case HCI_INIT_W4_SET_EVENT_MASK:
            // skip Classic init commands for LE only chipsets
            if (!hci_classic_supported()){
                if (hci_le_supported()){
                    hci_stack->substate = HCI_INIT_LE_READ_BUFFER_SIZE; // skip all classic command
                    return;
                } else {
                    log_error("Neither BR/EDR nor LE supported");
                    hci_init_done();
                    return;
                }
            }
            if (!gap_ssp_supported()){
                hci_stack->substate = HCI_INIT_WRITE_PAGE_TIMEOUT;
                return;
            }
            break;
        case HCI_INIT_W4_WRITE_PAGE_TIMEOUT:
            break;
        case HCI_INIT_W4_LE_READ_BUFFER_SIZE:
            // skip write le host if not supported (e.g. on LE only EM9301)
            if (hci_stack->local_supported_commands[0] & 0x02) break;
            hci_stack->substate = HCI_INIT_LE_SET_SCAN_PARAMETERS;
            return;
        case HCI_INIT_W4_WRITE_LOCAL_NAME:
            // skip write eir data if no eir data set
            if (hci_stack->eir_data) break;
            hci_stack->substate = HCI_INIT_WRITE_INQUIRY_MODE;
            return;

#ifdef ENABLE_SCO_OVER_HCI
        case HCI_INIT_W4_WRITE_SCAN_ENABLE:
            // skip write synchronous flow control if not supported
            if (hci_stack->local_supported_commands[0] & 0x04) break;
            hci_stack->substate = HCI_INIT_W4_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE;
            // explicit fall through to reduce repetitions

        case HCI_INIT_W4_WRITE_SYNCHRONOUS_FLOW_CONTROL_ENABLE:
            // skip write default erroneous data reporting if not supported
            if (hci_stack->local_supported_commands[0] & 0x08) break;
            hci_stack->substate = HCI_INIT_W4_WRITE_DEFAULT_ERRONEOUS_DATA_REPORTING;
            // explicit fall through to reduce repetitions

        case HCI_INIT_W4_WRITE_DEFAULT_ERRONEOUS_DATA_REPORTING:
            if (!hci_le_supported()){
                // SKIP LE init for Classic only configuration
                hci_init_done();
                return;
            }
            break;
#else
        case HCI_INIT_W4_WRITE_SCAN_ENABLE:
            if (!hci_le_supported()){
                // SKIP LE init for Classic only configuration
                hci_init_done();
                return;
            }
#endif
            break;
        // Response to command before init done state -> init done
        case (HCI_INIT_DONE-1):
            hci_init_done();
            return;

        default:
            break;
    }
    hci_initializing_next_state();
}

static void event_handler(uint8_t *packet, int size){

    uint16_t event_length = packet[1];

    // assert packet is complete
    if (size != event_length + 2){
        log_error("hci.c: event_handler called with event packet of wrong size %d, expected %u => dropping packet", size, event_length + 2);
        return;
    }

    bd_addr_t addr;
    bd_addr_type_t addr_type;
    uint8_t link_type;
    hci_con_handle_t handle;
    hci_connection_t * conn;
    int i;
        
    // log_info("HCI:EVENT:%02x", hci_event_packet_get_type(packet));
    
    switch (hci_event_packet_get_type(packet)) {
                        
        case HCI_EVENT_COMMAND_COMPLETE:
            // get num cmd packets
            // log_info("HCI_EVENT_COMMAND_COMPLETE cmds old %u - new %u", hci_stack->num_cmd_packets, packet[2]);
            hci_stack->num_cmd_packets = packet[2];

            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_read_buffer_size)){
                // from offset 5
                // status 
                // "The HC_ACL_Data_Packet_Length return parameter will be used to determine the size of the L2CAP segments contained in ACL Data Packets"
                hci_stack->acl_data_packet_length = little_endian_read_16(packet, 6);
                hci_stack->sco_data_packet_length = packet[8];
                hci_stack->acl_packets_total_num  = little_endian_read_16(packet, 9);
                hci_stack->sco_packets_total_num  = little_endian_read_16(packet, 11); 

                if (hci_stack->state == HCI_STATE_INITIALIZING){
                    // determine usable ACL payload size
                    if (HCI_ACL_PAYLOAD_SIZE < hci_stack->acl_data_packet_length){
                        hci_stack->acl_data_packet_length = HCI_ACL_PAYLOAD_SIZE;
                    }
                    log_info("hci_read_buffer_size: acl used size %u, count %u / sco size %u, count %u",
                             hci_stack->acl_data_packet_length, hci_stack->acl_packets_total_num,
                             hci_stack->sco_data_packet_length, hci_stack->sco_packets_total_num); 
                }
            }
#ifdef ENABLE_BLE
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_le_read_buffer_size)){
                hci_stack->le_data_packets_length = little_endian_read_16(packet, 6);
                hci_stack->le_acl_packets_total_num  = packet[8];
                    // determine usable ACL payload size
                    if (HCI_ACL_PAYLOAD_SIZE < hci_stack->le_data_packets_length){
                        hci_stack->le_data_packets_length = HCI_ACL_PAYLOAD_SIZE;
                    }
                log_info("hci_le_read_buffer_size: size %u, count %u", hci_stack->le_data_packets_length, hci_stack->le_acl_packets_total_num);
            }         
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_le_read_white_list_size)){
                hci_stack->le_whitelist_capacity = little_endian_read_16(packet, 6);
                log_info("hci_le_read_white_list_size: size %u", hci_stack->le_whitelist_capacity);
            }   
#endif
            // Dump local address
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_read_bd_addr)) {
                reverse_bd_addr(&packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE + 1],
				hci_stack->local_bd_addr);
                log_info("Local Address, Status: 0x%02x: Addr: %s",
                    packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE], bd_addr_to_str(hci_stack->local_bd_addr));
                if (hci_stack->link_key_db){
                    hci_stack->link_key_db->set_local_bd_addr(hci_stack->local_bd_addr);
                }
            }
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_write_scan_enable)){
                hci_emit_discoverable_enabled(hci_stack->discoverable);
            }
            // Note: HCI init checks 
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_read_local_supported_features)){
                memcpy(hci_stack->local_supported_features, &packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE+1], 8);

                // determine usable ACL packet types based on host buffer size and supported features
                hci_stack->packet_types = hci_acl_packet_types_for_buffer_size_and_local_features(HCI_ACL_PAYLOAD_SIZE, &hci_stack->local_supported_features[0]);
                log_info("Packet types %04x, eSCO %u", hci_stack->packet_types, hci_extended_sco_link_supported()); 

                // Classic/LE
                log_info("BR/EDR support %u, LE support %u", hci_classic_supported(), hci_le_supported());
            }
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_read_local_version_information)){
                // hci_stack->hci_version    = little_endian_read_16(packet, 4);
                // hci_stack->hci_revision   = little_endian_read_16(packet, 6);
                // hci_stack->lmp_version    = little_endian_read_16(packet, 8);
                hci_stack->manufacturer   = little_endian_read_16(packet, 10);
                // hci_stack->lmp_subversion = little_endian_read_16(packet, 12);
                log_info("Manufacturer: 0x%04x", hci_stack->manufacturer);
                // notify app
                if (hci_stack->local_version_information_callback){
                    hci_stack->local_version_information_callback(packet);
                }
            }
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_read_local_supported_commands)){
                hci_stack->local_supported_commands[0] =
                    (packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE+1+14] & 0x80) >> 7 |  // bit 0 = Octet 14, bit 7
                    (packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE+1+24] & 0x40) >> 5 |  // bit 1 = Octet 24, bit 6
                    (packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE+1+10] & 0x10) >> 2 |  // bit 2 = Octet 10, bit 4
                    (packet[OFFSET_OF_DATA_IN_COMMAND_COMPLETE+1+18] & 0x08);        // bit 3 = Octet 18, bit 3
                    log_info("Local supported commands summary 0x%02x", hci_stack->local_supported_commands[0]); 
            }
            if (HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_write_synchronous_flow_control_enable)){
                if (packet[5] == 0){
                    hci_stack->synchronous_flow_control_enabled = 1;
                }
            } 
            break;
            
        case HCI_EVENT_COMMAND_STATUS:
            // get num cmd packets
            // log_info("HCI_EVENT_COMMAND_STATUS cmds - old %u - new %u", hci_stack->num_cmd_packets, packet[3]);
            hci_stack->num_cmd_packets = packet[3];
            break;
            
        case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS:{
            int offset = 3;
            for (i=0; i<packet[2];i++){
                handle = little_endian_read_16(packet, offset);
                offset += 2;
                uint16_t num_packets = little_endian_read_16(packet, offset);
                offset += 2;
                
                conn = hci_connection_for_handle(handle);
                if (!conn){
                    log_error("hci_number_completed_packet lists unused con handle %u", handle);
                    continue;
                }
                
                if (conn->address_type == BD_ADDR_TYPE_SCO){
                    if (conn->num_sco_packets_sent >= num_packets){
                        conn->num_sco_packets_sent -= num_packets;
                    } else {
                        log_error("hci_number_completed_packets, more sco slots freed then sent.");
                        conn->num_sco_packets_sent = 0;
                    }
                    hci_notify_if_sco_can_send_now();
                } else {
                    if (conn->num_acl_packets_sent >= num_packets){
                        conn->num_acl_packets_sent -= num_packets;
                    } else {
                        log_error("hci_number_completed_packets, more acl slots freed then sent.");
                        conn->num_acl_packets_sent = 0;
                    }
                }
                // log_info("hci_number_completed_packet %u processed for handle %u, outstanding %u", num_packets, handle, conn->num_acl_packets_sent);
            }
            break;
        }
        case HCI_EVENT_CONNECTION_REQUEST:
            reverse_bd_addr(&packet[2], addr);
            // TODO: eval COD 8-10
            link_type = packet[11];
            log_info("Connection_incoming: %s, type %u", bd_addr_to_str(addr), link_type);
            addr_type = link_type == 1 ? BD_ADDR_TYPE_CLASSIC : BD_ADDR_TYPE_SCO;
            conn = hci_connection_for_bd_addr_and_type(addr, addr_type);
            if (!conn) {
                conn = create_connection_for_bd_addr_and_type(addr, addr_type);
            }
            if (!conn) {
                // CONNECTION REJECTED DUE TO LIMITED RESOURCES (0X0D)
                hci_stack->decline_reason = 0x0d;
                bd_addr_copy(hci_stack->decline_addr, addr);
                break;
            }
            conn->role  = HCI_ROLE_SLAVE;
            conn->state = RECEIVED_CONNECTION_REQUEST;
            // store info about eSCO
            if (link_type == 0x02){
                conn->remote_supported_feature_eSCO = 1;
            }
            hci_run();
            break;
            
        case HCI_EVENT_CONNECTION_COMPLETE:
            // Connection management
            reverse_bd_addr(&packet[5], addr);
            log_info("Connection_complete (status=%u) %s", packet[2], bd_addr_to_str(addr));
            addr_type = BD_ADDR_TYPE_CLASSIC;
            conn = hci_connection_for_bd_addr_and_type(addr, addr_type);
            if (conn) {
                if (!packet[2]){
                    conn->state = OPEN;
                    conn->con_handle = little_endian_read_16(packet, 3);
                    conn->bonding_flags |= BONDING_REQUEST_REMOTE_FEATURES;

                    // restart timer
                    btstack_run_loop_set_timer(&conn->timeout, HCI_CONNECTION_TIMEOUT_MS);
                    btstack_run_loop_add_timer(&conn->timeout);
                    
                    log_info("New connection: handle %u, %s", conn->con_handle, bd_addr_to_str(conn->address));
                    
                    hci_emit_nr_connections_changed();
                } else {
                    int notify_dedicated_bonding_failed = conn->bonding_flags & BONDING_DEDICATED;
                    uint8_t status = packet[2];
                    bd_addr_t bd_address;
                    memcpy(&bd_address, conn->address, 6);

                    // connection failed, remove entry
                    btstack_linked_list_remove(&hci_stack->connections, (btstack_linked_item_t *) conn);
                    btstack_memory_hci_connection_free( conn );
                    
                    // notify client if dedicated bonding
                    if (notify_dedicated_bonding_failed){
                        log_info("hci notify_dedicated_bonding_failed");
                        hci_emit_dedicated_bonding_result(bd_address, status);                        
                    }

                    // if authentication error, also delete link key
                    if (packet[2] == 0x05) {
                        gap_drop_link_key_for_bd_addr(addr);
                    }
                }
            }
            break;

        case HCI_EVENT_SYNCHRONOUS_CONNECTION_COMPLETE:
            reverse_bd_addr(&packet[5], addr);
            log_info("Synchronous Connection Complete (status=%u) %s", packet[2], bd_addr_to_str(addr));
            if (packet[2]){
                // connection failed
                break;
            }
            conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_SCO);
            if (!conn) {
                conn = create_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_SCO);
            }
            if (!conn) {
                break;
            }
            conn->state = OPEN;
            conn->con_handle = little_endian_read_16(packet, 3);            
            break;

        case HCI_EVENT_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE:
            handle = little_endian_read_16(packet, 3);
            conn = hci_connection_for_handle(handle);
            if (!conn) break;
            if (!packet[2]){
                uint8_t * features = &packet[5];
                if (features[6] & (1 << 3)){
                    conn->bonding_flags |= BONDING_REMOTE_SUPPORTS_SSP;
                }
                if (features[3] & (1<<7)){
                    conn->remote_supported_feature_eSCO = 1;
                }
            }
            conn->bonding_flags |= BONDING_RECEIVED_REMOTE_FEATURES;
            log_info("HCI_EVENT_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE, bonding flags %x, eSCO %u", conn->bonding_flags, conn->remote_supported_feature_eSCO);
            if (conn->bonding_flags & BONDING_DEDICATED){
                conn->bonding_flags |= BONDING_SEND_AUTHENTICATE_REQUEST;
            }
            break;

        case HCI_EVENT_LINK_KEY_REQUEST:
            log_info("HCI_EVENT_LINK_KEY_REQUEST");
            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], RECV_LINK_KEY_REQUEST);
            // non-bondable mode: link key negative reply will be sent by HANDLE_LINK_KEY_REQUEST
            if (hci_stack->bondable && !hci_stack->link_key_db) break;
            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], HANDLE_LINK_KEY_REQUEST);
            hci_run();
            // request handled by hci_run() as HANDLE_LINK_KEY_REQUEST gets set
            return;
            
        case HCI_EVENT_LINK_KEY_NOTIFICATION: {
            reverse_bd_addr(&packet[2], addr);
            conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_CLASSIC);
            if (!conn) break;
            conn->authentication_flags |= RECV_LINK_KEY_NOTIFICATION;
            link_key_type_t link_key_type = (link_key_type_t)packet[24];
            // Change Connection Encryption keeps link key type
            if (link_key_type != CHANGED_COMBINATION_KEY){
                conn->link_key_type = link_key_type;
            }
            gap_store_link_key_for_bd_addr(addr, &packet[8], conn->link_key_type);
            // still forward event to allow dismiss of pairing dialog
            break;
        }

        case HCI_EVENT_PIN_CODE_REQUEST:
            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], LEGACY_PAIRING_ACTIVE);
            // non-bondable mode: pin code negative reply will be sent
            if (!hci_stack->bondable){
                hci_add_connection_flags_for_flipped_bd_addr(&packet[2], DENY_PIN_CODE_REQUEST);
                hci_run();
                return;
            }
            // PIN CODE REQUEST means the link key request didn't succee -> delete stored link key
            if (!hci_stack->link_key_db) break;
            hci_event_pin_code_request_get_bd_addr(packet, addr);
            hci_stack->link_key_db->delete_link_key(addr);
            break;
            
        case HCI_EVENT_IO_CAPABILITY_REQUEST:
            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], RECV_IO_CAPABILITIES_REQUEST);
            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], SEND_IO_CAPABILITIES_REPLY);
            break;
        
        case HCI_EVENT_USER_CONFIRMATION_REQUEST:
            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], SSP_PAIRING_ACTIVE);
            if (!hci_stack->ssp_auto_accept) break;
            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], SEND_USER_CONFIRM_REPLY);
            break;

        case HCI_EVENT_USER_PASSKEY_REQUEST:
            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], SSP_PAIRING_ACTIVE);
            if (!hci_stack->ssp_auto_accept) break;
            hci_add_connection_flags_for_flipped_bd_addr(&packet[2], SEND_USER_PASSKEY_REPLY);
            break;

        case HCI_EVENT_ENCRYPTION_CHANGE:
            handle = little_endian_read_16(packet, 3);
            conn = hci_connection_for_handle(handle);
            if (!conn) break;
            if (packet[2] == 0) {
                if (packet[5]){
                    conn->authentication_flags |= CONNECTION_ENCRYPTED;
                } else {
                    conn->authentication_flags &= ~CONNECTION_ENCRYPTED;
                }
            }
            hci_emit_security_level(handle, gap_security_level_for_connection(conn));
            break;

        case HCI_EVENT_AUTHENTICATION_COMPLETE_EVENT:
            handle = little_endian_read_16(packet, 3);
            conn = hci_connection_for_handle(handle);
            if (!conn) break;

            // dedicated bonding: send result and disconnect
            if (conn->bonding_flags & BONDING_DEDICATED){
                conn->bonding_flags &= ~BONDING_DEDICATED;
                conn->bonding_flags |= BONDING_DISCONNECT_DEDICATED_DONE;
                conn->bonding_status = packet[2];
                break;
            }

            if (packet[2] == 0 && gap_security_level_for_link_key_type(conn->link_key_type) >= conn->requested_security_level){
                // link key sufficient for requested security
                conn->bonding_flags |= BONDING_SEND_ENCRYPTION_REQUEST;
                break;
            }
            // not enough
            hci_emit_security_level(handle, gap_security_level_for_connection(conn));
            break;

        // HCI_EVENT_DISCONNECTION_COMPLETE
        // has been split, to first notify stack before shutting connection down
        // see end of function, too.
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if (packet[2]) break;   // status != 0
            handle = little_endian_read_16(packet, 3);
            // drop outgoing ACL fragments if it is for closed connection
            if (hci_stack->acl_fragmentation_total_size > 0) {
                if (handle == READ_ACL_CONNECTION_HANDLE(hci_stack->hci_packet_buffer)){
                    log_info("hci: drop fragmented ACL data for closed connection");
                     hci_stack->acl_fragmentation_total_size = 0;
                     hci_stack->acl_fragmentation_pos = 0;
                }
            }
            // re-enable advertisements for le connections if active
            conn = hci_connection_for_handle(handle);
            if (!conn) break; 
            if (hci_is_le_connection(conn) && hci_stack->le_advertisements_enabled){
                hci_stack->le_advertisements_todo |= LE_ADVERTISEMENT_TASKS_ENABLE;
            }
            conn->state = RECEIVED_DISCONNECTION_COMPLETE;
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

        case HCI_EVENT_ROLE_CHANGE:
            if (packet[2]) break;   // status != 0
            handle = little_endian_read_16(packet, 3);
            conn = hci_connection_for_handle(handle);
            if (!conn) break;       // no conn
            conn->role = packet[9];
            break;

        case HCI_EVENT_TRANSPORT_PACKET_SENT:
            // release packet buffer only for asynchronous transport and if there are not further fragements
            if (hci_transport_synchronous()) {
                log_error("Synchronous HCI Transport shouldn't send HCI_EVENT_TRANSPORT_PACKET_SENT");
                return; // instead of break: to avoid re-entering hci_run()
            }
            if (hci_stack->acl_fragmentation_total_size) break;
            hci_release_packet_buffer();
            
            // L2CAP receives this event via the hci_emit_event below

            // For SCO, we do the can_send_now_check here
            hci_notify_if_sco_can_send_now();
            break;

        case HCI_EVENT_SCO_CAN_SEND_NOW:
            // For SCO, we do the can_send_now_check here
            hci_notify_if_sco_can_send_now();
            return;

#ifdef ENABLE_BLE
        case HCI_EVENT_LE_META:
            switch (packet[2]){
                case HCI_SUBEVENT_LE_ADVERTISING_REPORT:
                    // log_info("advertising report received");
                    if (hci_stack->le_scanning_state != LE_SCANNING) break;
                    le_handle_advertisement_report(packet, size);
                    break;
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                    // Connection management
                    reverse_bd_addr(&packet[8], addr);
                    addr_type = (bd_addr_type_t)packet[7];
                    log_info("LE Connection_complete (status=%u) type %u, %s", packet[3], addr_type, bd_addr_to_str(addr));
                    conn = hci_connection_for_bd_addr_and_type(addr, addr_type);
                    // if auto-connect, remove from whitelist in both roles
                    if (hci_stack->le_connecting_state == LE_CONNECTING_WHITELIST){
                        hci_remove_from_whitelist(addr_type, addr);  
                    }
                    // handle error: error is reported only to the initiator -> outgoing connection
                    if (packet[3]){
                        // outgoing connection establishment is done
                        hci_stack->le_connecting_state = LE_CONNECTING_IDLE;
                        // remove entry
                        if (conn){
                            btstack_linked_list_remove(&hci_stack->connections, (btstack_linked_item_t *) conn);
                            btstack_memory_hci_connection_free( conn );
                        }
                        break;
                    }
                    // on success, both hosts receive connection complete event
                    if (packet[6] == HCI_ROLE_MASTER){
                        // if we're master, it was an outgoing connection and we're done with it
                        hci_stack->le_connecting_state = LE_CONNECTING_IDLE;
                    } else {
                        // if we're slave, it was an incoming connection, advertisements have stopped
                        hci_stack->le_advertisements_active = 0;
                    }
                    // LE connections are auto-accepted, so just create a connection if there isn't one already
                    if (!conn){
                        conn = create_connection_for_bd_addr_and_type(addr, addr_type);
                    }
                    // no memory, sorry.
                    if (!conn){
                        break;
                    }
                    
                    conn->state = OPEN;
                    conn->role  = packet[6];
                    conn->con_handle = little_endian_read_16(packet, 4);
                    
                    // TODO: store - role, peer address type, conn_interval, conn_latency, supervision timeout, master clock

                    // restart timer
                    // btstack_run_loop_set_timer(&conn->timeout, HCI_CONNECTION_TIMEOUT_MS);
                    // btstack_run_loop_add_timer(&conn->timeout);
                    
                    log_info("New connection: handle %u, %s", conn->con_handle, bd_addr_to_str(conn->address));
                    
                    hci_emit_nr_connections_changed();
                    break;

            // log_info("LE buffer size: %u, count %u", little_endian_read_16(packet,6), packet[8]);
                    
                default:
                    break;
            }
            break;
#endif            
        default:
            break;
    }

    // handle BT initialization
    if (hci_stack->state == HCI_STATE_INITIALIZING){
        hci_initializing_event_handler(packet, size);
    }
    
    // help with BT sleep
    if (hci_stack->state == HCI_STATE_FALLING_ASLEEP
        && hci_stack->substate == HCI_FALLING_ASLEEP_W4_WRITE_SCAN_ENABLE
        && HCI_EVENT_IS_COMMAND_COMPLETE(packet, hci_write_scan_enable)){
        hci_initializing_next_state();
    }
    
    // notify upper stack
	hci_emit_event(packet, size, 0);   // don't dump, already happened in packet handler

    // moved here to give upper stack a chance to close down everything with hci_connection_t intact
    if (hci_event_packet_get_type(packet) == HCI_EVENT_DISCONNECTION_COMPLETE){
        if (!packet[2]){
            handle = little_endian_read_16(packet, 3);
            hci_connection_t * aConn = hci_connection_for_handle(handle);
            if (aConn) {
                uint8_t status = aConn->bonding_status;
                uint16_t flags = aConn->bonding_flags;
                bd_addr_t bd_address;
                memcpy(&bd_address, aConn->address, 6);
                hci_shutdown_connection(aConn);
                // connection struct is gone, don't access anymore
                if (flags & BONDING_EMIT_COMPLETE_ON_DISCONNECT){
                    hci_emit_dedicated_bonding_result(bd_address, status);
                }
            }
        }
    }

	// execute main loop
	hci_run();
}

static void sco_handler(uint8_t * packet, uint16_t size){
    if (!hci_stack->sco_packet_handler) return;
    hci_stack->sco_packet_handler(HCI_SCO_DATA_PACKET, 0, packet, size);
}

static void packet_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
    hci_dump_packet(packet_type, 1, packet, size);
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            event_handler(packet, size);
            break;
        case HCI_ACL_DATA_PACKET:
            acl_handler(packet, size);
            break;
        case HCI_SCO_DATA_PACKET:
            sco_handler(packet, size);
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


/** Register HCI packet handlers */
void hci_register_acl_packet_handler(btstack_packet_handler_t handler){
    hci_stack->acl_packet_handler = handler;
}

/**
 * @brief Registers a packet handler for SCO data. Used for HSP and HFP profiles.
 */
void hci_register_sco_packet_handler(btstack_packet_handler_t handler){
    hci_stack->sco_packet_handler = handler;    
}

static void hci_state_reset(void){
    // no connections yet
    hci_stack->connections = NULL;

    // keep discoverable/connectable as this has been requested by the client(s)
    // hci_stack->discoverable = 0;
    // hci_stack->connectable = 0;
    // hci_stack->bondable = 1;

    // buffer is free
    hci_stack->hci_packet_buffer_reserved = 0;

    // no pending cmds
    hci_stack->decline_reason = 0;
    hci_stack->new_scan_enable_value = 0xff;
    
    // LE
    hci_stack->adv_addr_type = 0;
    memset(hci_stack->adv_address, 0, 6);
    hci_stack->le_scanning_state = LE_SCAN_IDLE;
    hci_stack->le_scan_type = 0xff; 
    hci_stack->le_connecting_state = LE_CONNECTING_IDLE;
    hci_stack->le_whitelist = 0;
    hci_stack->le_whitelist_capacity = 0;
    hci_stack->le_connection_parameter_range.le_conn_interval_min =          6; 
    hci_stack->le_connection_parameter_range.le_conn_interval_max =       3200;
    hci_stack->le_connection_parameter_range.le_conn_latency_min =           0;
    hci_stack->le_connection_parameter_range.le_conn_latency_max =         500;
    hci_stack->le_connection_parameter_range.le_supervision_timeout_min =   10;
    hci_stack->le_connection_parameter_range.le_supervision_timeout_max = 3200;
}

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

    // Secure Simple Pairing default: enable, no I/O capabilities, general bonding, mitm not required, auto accept 
    hci_stack->ssp_enable = 1;
    hci_stack->ssp_io_capability = SSP_IO_CAPABILITY_NO_INPUT_NO_OUTPUT;
    hci_stack->ssp_authentication_requirement = SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_GENERAL_BONDING;
    hci_stack->ssp_auto_accept = 1;

    // voice setting - signed 8 bit pcm data with CVSD over the air
    hci_stack->sco_voice_setting = 0x40;

    hci_state_reset();
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

/**
 * @brief Configure Bluetooth hardware control. Has to be called after hci_init() but before power on.
 */
void hci_set_control(const btstack_control_t *hardware_control){
    // references to used control implementation
    hci_stack->control = hardware_control;
    // init with transport config
    hardware_control->init(hci_stack->config);
}

void hci_close(void){
    // close remote device db
    if (hci_stack->link_key_db) {
        hci_stack->link_key_db->close();
    }

    btstack_linked_list_iterator_t lit;
    btstack_linked_list_iterator_init(&lit, &hci_stack->connections);
    while (btstack_linked_list_iterator_has_next(&lit)){
        // cancel all l2cap connections by emitting dicsconnection complete before shutdown (free) connection
        hci_connection_t * connection = (hci_connection_t*) btstack_linked_list_iterator_next(&lit);
        hci_emit_disconnection_complete(connection->con_handle, 0x16); // terminated by local host
        hci_shutdown_connection(connection);
    }

    hci_power_control(HCI_POWER_OFF);
    
#ifdef HAVE_MALLOC
    free(hci_stack);
#endif
    hci_stack = NULL;
}

void gap_set_class_of_device(uint32_t class_of_device){
    hci_stack->class_of_device = class_of_device;
}

// Set Public BD ADDR - passed on to Bluetooth chipset if supported in bt_control_h
void hci_set_bd_addr(bd_addr_t addr){
    memcpy(hci_stack->custom_bd_addr, addr, 6);
    hci_stack->custom_bd_addr_set = 1;
}

void hci_disable_l2cap_timeout_check(void){
    disable_l2cap_timeouts = 1;
}
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

static void hci_power_transition_to_initializing(void){
    // set up state machine
    hci_stack->num_cmd_packets = 1; // assume that one cmd can be sent
    hci_stack->hci_packet_buffer_reserved = 0;
    hci_stack->state = HCI_STATE_INITIALIZING;
    hci_stack->substate = HCI_INIT_SEND_RESET;
}

int hci_power_control(HCI_POWER_MODE power_mode){
    
    log_info("hci_power_control: %d, current mode %u", power_mode, hci_stack->state);
    
    int err = 0;
    switch (hci_stack->state){
            
        case HCI_STATE_OFF:
            switch (power_mode){
                case HCI_POWER_ON:
                    err = hci_power_control_on();
                    if (err) {
                        log_error("hci_power_control_on() error %d", err);
                        return err;
                    }
                    hci_power_transition_to_initializing();
                    break;
                case HCI_POWER_OFF:
                    // do nothing
                    break;  
                case HCI_POWER_SLEEP:
                    // do nothing (with SLEEP == OFF)
                    break;
            }
            break;
            
        case HCI_STATE_INITIALIZING:
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
            }
            break;
            
        case HCI_STATE_WORKING:
            switch (power_mode){
                case HCI_POWER_ON:
                    // do nothing
                    break;
                case HCI_POWER_OFF:
                    // see hci_run
                    hci_stack->state = HCI_STATE_HALTING;
                    break;  
                case HCI_POWER_SLEEP:
                    // see hci_run
                    hci_stack->state = HCI_STATE_FALLING_ASLEEP;
                    hci_stack->substate = HCI_FALLING_ASLEEP_DISCONNECT;
                    break;
            }
            break;
            
        case HCI_STATE_HALTING:
            switch (power_mode){
                case HCI_POWER_ON:
                    hci_power_transition_to_initializing();
                    break;
                case HCI_POWER_OFF:
                    // do nothing
                    break;  
                case HCI_POWER_SLEEP:
                    // see hci_run
                    hci_stack->state = HCI_STATE_FALLING_ASLEEP;
                    hci_stack->substate = HCI_FALLING_ASLEEP_DISCONNECT;
                    break;
            }
            break;
            
        case HCI_STATE_FALLING_ASLEEP:
            switch (power_mode){
                case HCI_POWER_ON:

#ifdef HAVE_PLATFORM_IPHONE_OS
                    // nothing to do, if H4 supports power management
                    if (btstack_control_iphone_power_management_enabled()){
                        hci_stack->state = HCI_STATE_INITIALIZING;
                        hci_stack->substate = HCI_INIT_WRITE_SCAN_ENABLE;   // init after sleep
                        break;
                    }
#endif
                    hci_power_transition_to_initializing();
                    break;
                case HCI_POWER_OFF:
                    // see hci_run
                    hci_stack->state = HCI_STATE_HALTING;
                    break;  
                case HCI_POWER_SLEEP:
                    // do nothing
                    break;
            }
            break;
            
        case HCI_STATE_SLEEPING:
            switch (power_mode){
                case HCI_POWER_ON:
                    
#ifdef HAVE_PLATFORM_IPHONE_OS
                    // nothing to do, if H4 supports power management
                    if (btstack_control_iphone_power_management_enabled()){
                        hci_stack->state = HCI_STATE_INITIALIZING;
                        hci_stack->substate = HCI_INIT_AFTER_SLEEP;
                        hci_update_scan_enable();
                        break;
                    }
#endif
                    err = hci_power_control_wake();
                    if (err) return err;
                    hci_power_transition_to_initializing();
                    break;
                case HCI_POWER_OFF:
                    hci_stack->state = HCI_STATE_HALTING;
                    break;  
                case HCI_POWER_SLEEP:
                    // do nothing
                    break;
            }
            break;
    }

    // create internal event
	hci_emit_state();
    
	// trigger next/first action
	hci_run();
	
    return 0;
}

static void hci_update_scan_enable(void){
    // 2 = page scan, 1 = inq scan
    hci_stack->new_scan_enable_value  = hci_stack->connectable << 1 | hci_stack->discoverable;
    hci_run();
}

void gap_discoverable_control(uint8_t enable){
    if (enable) enable = 1; // normalize argument
    
    if (hci_stack->discoverable == enable){
        hci_emit_discoverable_enabled(hci_stack->discoverable);
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

void gap_local_bd_addr(bd_addr_t address_buffer){
    memcpy(address_buffer, hci_stack->local_bd_addr, 6);
}

static void hci_run(void){
    
    // log_info("hci_run: entered");
    btstack_linked_item_t * it;

    // send continuation fragments first, as they block the prepared packet buffer
    if (hci_stack->acl_fragmentation_total_size > 0) {
        hci_con_handle_t con_handle = READ_ACL_CONNECTION_HANDLE(hci_stack->hci_packet_buffer);
        hci_connection_t *connection = hci_connection_for_handle(con_handle);
        if (connection) {
            if (hci_can_send_prepared_acl_packet_now(con_handle)){
                hci_send_acl_packet_fragments(connection);
                return;
            }
        } else {
            // connection gone -> discard further fragments
            log_info("hci_run: fragmented ACL packet no connection -> discard fragment");
            hci_stack->acl_fragmentation_total_size = 0;
            hci_stack->acl_fragmentation_pos = 0;
        }
    }

    if (!hci_can_send_command_packet_now()) return;

    // global/non-connection oriented commands
    
    // decline incoming connections
    if (hci_stack->decline_reason){
        uint8_t reason = hci_stack->decline_reason;
        hci_stack->decline_reason = 0;
        hci_send_cmd(&hci_reject_connection_request, hci_stack->decline_addr, reason);
        return;
    }

    // send scan enable
    if (hci_stack->state == HCI_STATE_WORKING && hci_stack->new_scan_enable_value != 0xff && hci_classic_supported()){
        hci_send_cmd(&hci_write_scan_enable, hci_stack->new_scan_enable_value);
        hci_stack->new_scan_enable_value = 0xff;
        return;
    }
    
#ifdef ENABLE_BLE
    if (hci_stack->state == HCI_STATE_WORKING){
        // handle le scan
        switch(hci_stack->le_scanning_state){
            case LE_START_SCAN:
                hci_stack->le_scanning_state = LE_SCANNING;
                hci_send_cmd(&hci_le_set_scan_enable, 1, 0);
                return;
                
            case LE_STOP_SCAN:
                hci_stack->le_scanning_state = LE_SCAN_IDLE;
                hci_send_cmd(&hci_le_set_scan_enable, 0, 0);
                return;
            default:
                break;
        }
        if (hci_stack->le_scan_type != 0xff){
            // defaults: active scanning, accept all advertisement packets
            int scan_type = hci_stack->le_scan_type;
            hci_stack->le_scan_type = 0xff;
            hci_send_cmd(&hci_le_set_scan_parameters, scan_type, hci_stack->le_scan_interval, hci_stack->le_scan_window, hci_stack->adv_addr_type, 0);
            return;
        }
        // le advertisement control
        if (hci_stack->le_advertisements_todo){
            log_info("hci_run: gap_le: adv todo: %x", hci_stack->le_advertisements_todo );
        }
        if (hci_stack->le_advertisements_todo & LE_ADVERTISEMENT_TASKS_DISABLE){
            hci_stack->le_advertisements_todo &= ~LE_ADVERTISEMENT_TASKS_DISABLE;
            hci_send_cmd(&hci_le_set_advertise_enable, 0);
            return;
        }
        if (hci_stack->le_advertisements_todo & LE_ADVERTISEMENT_TASKS_SET_PARAMS){
            hci_stack->le_advertisements_todo &= ~LE_ADVERTISEMENT_TASKS_SET_PARAMS;
            hci_send_cmd(&hci_le_set_advertising_parameters,
                 hci_stack->le_advertisements_interval_min,
                 hci_stack->le_advertisements_interval_max,
                 hci_stack->le_advertisements_type,
                 hci_stack->le_advertisements_own_address_type,
                 hci_stack->le_advertisements_direct_address_type,
                 hci_stack->le_advertisements_direct_address,
                 hci_stack->le_advertisements_channel_map,
                 hci_stack->le_advertisements_filter_policy);
            return;
        }
        if (hci_stack->le_advertisements_todo & LE_ADVERTISEMENT_TASKS_SET_ADV_DATA){
            hci_stack->le_advertisements_todo &= ~LE_ADVERTISEMENT_TASKS_SET_ADV_DATA;
            hci_send_cmd(&hci_le_set_advertising_data, hci_stack->le_advertisements_data_len,
                hci_stack->le_advertisements_data);
            return;
        }
        if (hci_stack->le_advertisements_todo & LE_ADVERTISEMENT_TASKS_SET_SCAN_DATA){
            hci_stack->le_advertisements_todo &= ~LE_ADVERTISEMENT_TASKS_SET_SCAN_DATA;
            hci_send_cmd(&hci_le_set_scan_response_data, hci_stack->le_scan_response_data_len,
                hci_stack->le_scan_response_data);
            return;
        }
        if (hci_stack->le_advertisements_todo & LE_ADVERTISEMENT_TASKS_ENABLE){
            hci_stack->le_advertisements_todo &= ~LE_ADVERTISEMENT_TASKS_ENABLE;
            hci_send_cmd(&hci_le_set_advertise_enable, 1);
            return;
        }

        //
        // LE Whitelist Management
        //

        // check if whitelist needs modification
        btstack_linked_list_iterator_t lit;
        int modification_pending = 0;
        btstack_linked_list_iterator_init(&lit, &hci_stack->le_whitelist);
        while (btstack_linked_list_iterator_has_next(&lit)){
            whitelist_entry_t * entry = (whitelist_entry_t*) btstack_linked_list_iterator_next(&lit);
            if (entry->state & (LE_WHITELIST_REMOVE_FROM_CONTROLLER | LE_WHITELIST_ADD_TO_CONTROLLER)){
                modification_pending = 1;
                break;
            }
        }

        if (modification_pending){
            // stop connnecting if modification pending
            if (hci_stack->le_connecting_state != LE_CONNECTING_IDLE){
                hci_send_cmd(&hci_le_create_connection_cancel);
                return;
            }

            // add/remove entries
            btstack_linked_list_iterator_init(&lit, &hci_stack->le_whitelist);
            while (btstack_linked_list_iterator_has_next(&lit)){
                whitelist_entry_t * entry = (whitelist_entry_t*) btstack_linked_list_iterator_next(&lit);
                if (entry->state & LE_WHITELIST_ADD_TO_CONTROLLER){
                    entry->state = LE_WHITELIST_ON_CONTROLLER;
                    hci_send_cmd(&hci_le_add_device_to_white_list, entry->address_type, entry->address);
                    return;

                }
                if (entry->state & LE_WHITELIST_REMOVE_FROM_CONTROLLER){
                    bd_addr_t address;
                    bd_addr_type_t address_type = entry->address_type;                    
                    memcpy(address, entry->address, 6);
                    btstack_linked_list_remove(&hci_stack->le_whitelist, (btstack_linked_item_t *) entry);
                    btstack_memory_whitelist_entry_free(entry);
                    hci_send_cmd(&hci_le_remove_device_from_white_list, address_type, address);
                    return;
                }
            }
        }

        // start connecting
        if ( hci_stack->le_connecting_state == LE_CONNECTING_IDLE && 
            !btstack_linked_list_empty(&hci_stack->le_whitelist)){
            bd_addr_t null_addr;
            memset(null_addr, 0, 6);
            hci_send_cmd(&hci_le_create_connection,
                 0x0060,    // scan interval: 60 ms
                 0x0030,    // scan interval: 30 ms
                 1,         // use whitelist
                 0,         // peer address type
                 null_addr,      // peer bd addr
                 hci_stack->adv_addr_type, // our addr type:
                 0x0008,    // conn interval min
                 0x0018,    // conn interval max
                 0,         // conn latency
                 0x0048,    // supervision timeout
                 0x0001,    // min ce length
                 0x0001     // max ce length
                 );
            return;
        }
    }
#endif
    
    // send pending HCI commands
    for (it = (btstack_linked_item_t *) hci_stack->connections; it ; it = it->next){
        hci_connection_t * connection = (hci_connection_t *) it;
        
        switch(connection->state){
            case SEND_CREATE_CONNECTION:
                switch(connection->address_type){
                    case BD_ADDR_TYPE_CLASSIC:
                        log_info("sending hci_create_connection");
                        hci_send_cmd(&hci_create_connection, connection->address, hci_usable_acl_packet_types(), 0, 0, 0, 1);
                        break;
                    default:
#ifdef ENABLE_BLE
                        log_info("sending hci_le_create_connection");
                        hci_send_cmd(&hci_le_create_connection,
                                     0x0060,    // scan interval: 60 ms
                                     0x0030,    // scan interval: 30 ms
                                     0,         // don't use whitelist
                                     connection->address_type, // peer address type
                                     connection->address,      // peer bd addr
                                     hci_stack->adv_addr_type, // our addr type:
                                     0x0008,    // conn interval min
                                     0x0018,    // conn interval max
                                     0,         // conn latency
                                     0x0048,    // supervision timeout
                                     0x0001,    // min ce length
                                     0x0001     // max ce length
                                     );
                        
                        connection->state = SENT_CREATE_CONNECTION;
#endif
                        break;
                }
                return;
               
            case RECEIVED_CONNECTION_REQUEST:
                log_info("sending hci_accept_connection_request, remote eSCO %u", connection->remote_supported_feature_eSCO);
                connection->state = ACCEPTED_CONNECTION_REQUEST;
                connection->role  = HCI_ROLE_SLAVE;
                if (connection->address_type == BD_ADDR_TYPE_CLASSIC){
                    hci_send_cmd(&hci_accept_connection_request, connection->address, 1);
                } 
                return;

#ifdef ENABLE_BLE
            case SEND_CANCEL_CONNECTION:
                connection->state = SENT_CANCEL_CONNECTION;
                hci_send_cmd(&hci_le_create_connection_cancel);
                return;
#endif                
            case SEND_DISCONNECT:
                connection->state = SENT_DISCONNECT;
                hci_send_cmd(&hci_disconnect, connection->con_handle, 0x13); // remote closed connection
                return;
                
            default:
                break;
        }
        
        if (connection->authentication_flags & HANDLE_LINK_KEY_REQUEST){
            log_info("responding to link key request");
            connectionClearAuthenticationFlags(connection, HANDLE_LINK_KEY_REQUEST);
            link_key_t link_key;
            link_key_type_t link_key_type;
            if ( hci_stack->link_key_db
              && hci_stack->link_key_db->get_link_key(connection->address, link_key, &link_key_type)
              && gap_security_level_for_link_key_type(link_key_type) >= connection->requested_security_level){
               connection->link_key_type = link_key_type;
               hci_send_cmd(&hci_link_key_request_reply, connection->address, &link_key);
            } else {
               hci_send_cmd(&hci_link_key_request_negative_reply, connection->address);
            }
            return;
        }

        if (connection->authentication_flags & DENY_PIN_CODE_REQUEST){
            log_info("denying to pin request");
            connectionClearAuthenticationFlags(connection, DENY_PIN_CODE_REQUEST);
            hci_send_cmd(&hci_pin_code_request_negative_reply, connection->address);
            return;
        }

        if (connection->authentication_flags & SEND_IO_CAPABILITIES_REPLY){
            connectionClearAuthenticationFlags(connection, SEND_IO_CAPABILITIES_REPLY);
            log_info("IO Capability Request received, stack bondable %u, io cap %u", hci_stack->bondable, hci_stack->ssp_io_capability);
            if (hci_stack->bondable && (hci_stack->ssp_io_capability != SSP_IO_CAPABILITY_UNKNOWN)){
                // tweak authentication requirements
                uint8_t authreq = hci_stack->ssp_authentication_requirement;
                if (connection->bonding_flags & BONDING_DEDICATED){
                    authreq = SSP_IO_AUTHREQ_MITM_PROTECTION_NOT_REQUIRED_DEDICATED_BONDING;
                }
                if (gap_mitm_protection_required_for_security_level(connection->requested_security_level)){
                    authreq |= 1;
                } 
                hci_send_cmd(&hci_io_capability_request_reply, &connection->address, hci_stack->ssp_io_capability, NULL, authreq);
            } else {
                hci_send_cmd(&hci_io_capability_request_negative_reply, &connection->address, ERROR_CODE_PAIRING_NOT_ALLOWED);
            }
            return;
        }
        
        if (connection->authentication_flags & SEND_USER_CONFIRM_REPLY){
            connectionClearAuthenticationFlags(connection, SEND_USER_CONFIRM_REPLY);
            hci_send_cmd(&hci_user_confirmation_request_reply, &connection->address);
            return;
        }

        if (connection->authentication_flags & SEND_USER_PASSKEY_REPLY){
            connectionClearAuthenticationFlags(connection, SEND_USER_PASSKEY_REPLY);
            hci_send_cmd(&hci_user_passkey_request_reply, &connection->address, 000000);
            return;
        }

        if (connection->bonding_flags & BONDING_REQUEST_REMOTE_FEATURES){
            connection->bonding_flags &= ~BONDING_REQUEST_REMOTE_FEATURES;
            hci_send_cmd(&hci_read_remote_supported_features_command, connection->con_handle);
            return;
        }

        if (connection->bonding_flags & BONDING_DISCONNECT_SECURITY_BLOCK){
            connection->bonding_flags &= ~BONDING_DISCONNECT_SECURITY_BLOCK;
            hci_send_cmd(&hci_disconnect, connection->con_handle, 0x0005);  // authentication failure
            return;
        }
        if (connection->bonding_flags & BONDING_DISCONNECT_DEDICATED_DONE){
            connection->bonding_flags &= ~BONDING_DISCONNECT_DEDICATED_DONE;
            connection->bonding_flags |= BONDING_EMIT_COMPLETE_ON_DISCONNECT;
            hci_send_cmd(&hci_disconnect, connection->con_handle, 0x13);  // authentication done
            return;
        }
        if (connection->bonding_flags & BONDING_SEND_AUTHENTICATE_REQUEST){
            connection->bonding_flags &= ~BONDING_SEND_AUTHENTICATE_REQUEST;
            hci_send_cmd(&hci_authentication_requested, connection->con_handle);
            return;
        }
        if (connection->bonding_flags & BONDING_SEND_ENCRYPTION_REQUEST){
            connection->bonding_flags &= ~BONDING_SEND_ENCRYPTION_REQUEST;
            hci_send_cmd(&hci_set_connection_encryption, connection->con_handle, 1);
            return;
        }

#ifdef ENABLE_BLE
        if (connection->le_con_parameter_update_state == CON_PARAMETER_UPDATE_CHANGE_HCI_CON_PARAMETERS){
            connection->le_con_parameter_update_state = CON_PARAMETER_UPDATE_NONE; 
            
            uint16_t connection_interval_min = connection->le_conn_interval_min;
            connection->le_conn_interval_min = 0;
            hci_send_cmd(&hci_le_connection_update, connection->con_handle, connection_interval_min,
                connection->le_conn_interval_max, connection->le_conn_latency, connection->le_supervision_timeout,
                0x0000, 0xffff);
        }
#endif
    }
    
    hci_connection_t * connection;
    switch (hci_stack->state){
        case HCI_STATE_INITIALIZING:
            hci_initializing_run();
            break;
            
        case HCI_STATE_HALTING:

            log_info("HCI_STATE_HALTING");

            // free whitelist entries
#ifdef ENABLE_BLE
            {
                btstack_linked_list_iterator_t lit;
                btstack_linked_list_iterator_init(&lit, &hci_stack->le_whitelist);
                while (btstack_linked_list_iterator_has_next(&lit)){
                    whitelist_entry_t * entry = (whitelist_entry_t*) btstack_linked_list_iterator_next(&lit);
                    btstack_linked_list_remove(&hci_stack->le_whitelist, (btstack_linked_item_t *) entry);
                    btstack_memory_whitelist_entry_free(entry);
                }
            }
#endif
            // close all open connections
            connection =  (hci_connection_t *) hci_stack->connections;
            if (connection){
                hci_con_handle_t con_handle = (uint16_t) connection->con_handle;
                if (!hci_can_send_command_packet_now()) return;

                log_info("HCI_STATE_HALTING, connection %p, handle %u", connection, con_handle);

                // cancel all l2cap connections right away instead of waiting for disconnection complete event ...
                hci_emit_disconnection_complete(con_handle, 0x16); // terminated by local host

                // ... which would be ignored anyway as we shutdown (free) the connection now
                hci_shutdown_connection(connection);

                // finally, send the disconnect command
                hci_send_cmd(&hci_disconnect, con_handle, 0x13);  // remote closed connection
                return;
            }
            log_info("HCI_STATE_HALTING, calling off");
            
            // switch mode
            hci_power_control_off();
            
            log_info("HCI_STATE_HALTING, emitting state");
            hci_emit_state();
            log_info("HCI_STATE_HALTING, done");
            break;
            
        case HCI_STATE_FALLING_ASLEEP:
            switch(hci_stack->substate) {
                case HCI_FALLING_ASLEEP_DISCONNECT:
                    log_info("HCI_STATE_FALLING_ASLEEP");
                    // close all open connections
                    connection =  (hci_connection_t *) hci_stack->connections;

#ifdef HAVE_PLATFORM_IPHONE_OS
                    // don't close connections, if H4 supports power management
                    if (btstack_control_iphone_power_management_enabled()){
                        connection = NULL;
                    }
#endif
                    if (connection){
                        
                        // send disconnect
                        if (!hci_can_send_command_packet_now()) return;

                        log_info("HCI_STATE_FALLING_ASLEEP, connection %p, handle %u", connection, (uint16_t)connection->con_handle);
                        hci_send_cmd(&hci_disconnect, connection->con_handle, 0x13);  // remote closed connection
                        
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
                    // fall through for ble-only chips

                case HCI_FALLING_ASLEEP_COMPLETE:
                    log_info("HCI_STATE_HALTING, calling sleep");
#ifdef HAVE_PLATFORM_IPHONE_OS
                    // don't actually go to sleep, if H4 supports power management
                    if (btstack_control_iphone_power_management_enabled()){
                        // SLEEP MODE reached
                        hci_stack->state = HCI_STATE_SLEEPING; 
                        hci_emit_state();
                        break;
                    }
#endif
                    // switch mode
                    hci_power_control_sleep();  // changes hci_stack->state to SLEEP
                    hci_emit_state();
                    break;
                    
                default:
                    break;
            }
            break;
            
        default:
            break;
    }
}

int hci_send_cmd_packet(uint8_t *packet, int size){
    bd_addr_t addr;
    hci_connection_t * conn;
    // house-keeping
    
    // create_connection?
    if (IS_COMMAND(packet, hci_create_connection)){
        reverse_bd_addr(&packet[3], addr);
        log_info("Create_connection to %s", bd_addr_to_str(addr));

        conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_CLASSIC);
        if (!conn){
            conn = create_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_CLASSIC);
            if (!conn){
                // notify client that alloc failed
                hci_emit_connection_complete(addr, 0, BTSTACK_MEMORY_ALLOC_FAILED);
                return 0; // don't sent packet to controller
            }
            conn->state = SEND_CREATE_CONNECTION;
        }
        log_info("conn state %u", conn->state);
        switch (conn->state){
            // if connection active exists
            case OPEN:
                // and OPEN, emit connection complete command, don't send to controller
                hci_emit_connection_complete(addr, conn->con_handle, 0);
                return 0;
            case SEND_CREATE_CONNECTION:
                // connection created by hci, e.g. dedicated bonding
                break;
            default:
                // otherwise, just ignore as it is already in the open process
                return 0;
        }
        conn->state = SENT_CREATE_CONNECTION;
    }
    if (IS_COMMAND(packet, hci_link_key_request_reply)){
        hci_add_connection_flags_for_flipped_bd_addr(&packet[3], SENT_LINK_KEY_REPLY);
    }
    if (IS_COMMAND(packet, hci_link_key_request_negative_reply)){
        hci_add_connection_flags_for_flipped_bd_addr(&packet[3], SENT_LINK_KEY_NEGATIVE_REQUEST);
    }
    
    if (IS_COMMAND(packet, hci_delete_stored_link_key)){
        if (hci_stack->link_key_db){
            reverse_bd_addr(&packet[3], addr);
            hci_stack->link_key_db->delete_link_key(addr);
        }
    }

    if (IS_COMMAND(packet, hci_pin_code_request_negative_reply)
    ||  IS_COMMAND(packet, hci_pin_code_request_reply)){
        reverse_bd_addr(&packet[3], addr);
        conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_CLASSIC);
        if (conn){
            connectionClearAuthenticationFlags(conn, LEGACY_PAIRING_ACTIVE);
        }
    }

    if (IS_COMMAND(packet, hci_user_confirmation_request_negative_reply)
    ||  IS_COMMAND(packet, hci_user_confirmation_request_reply)
    ||  IS_COMMAND(packet, hci_user_passkey_request_negative_reply)
    ||  IS_COMMAND(packet, hci_user_passkey_request_reply)) {
        reverse_bd_addr(&packet[3], addr);
        conn = hci_connection_for_bd_addr_and_type(addr, BD_ADDR_TYPE_CLASSIC);
        if (conn){
            connectionClearAuthenticationFlags(conn, SSP_PAIRING_ACTIVE);
        }
    }

    if (IS_COMMAND(packet, hci_write_loopback_mode)){
        hci_stack->loopback_mode = packet[3];
    }

#ifdef ENABLE_BLE
    if (IS_COMMAND(packet, hci_le_set_advertising_parameters)){
        hci_stack->adv_addr_type = packet[8];
    }
    if (IS_COMMAND(packet, hci_le_set_random_address)){
        reverse_bd_addr(&packet[3], hci_stack->adv_address);
    }
    if (IS_COMMAND(packet, hci_le_set_advertise_enable)){
        hci_stack->le_advertisements_active = packet[3];
    }
    if (IS_COMMAND(packet, hci_le_create_connection)){
        // white list used?
        uint8_t initiator_filter_policy = packet[7];
        switch (initiator_filter_policy){
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
    }
    if (IS_COMMAND(packet, hci_le_create_connection_cancel)){
        hci_stack->le_connecting_state = LE_CONNECTING_IDLE;
    }
#endif

    hci_stack->num_cmd_packets--;

    hci_dump_packet(HCI_COMMAND_DATA_PACKET, 0, packet, size);
    int err = hci_stack->hci_transport->send_packet(HCI_COMMAND_DATA_PACKET, packet, size);

    // release packet buffer for synchronous transport implementations    
    if (hci_transport_synchronous() && (packet == hci_stack->hci_packet_buffer)){
        hci_stack->hci_packet_buffer_reserved = 0;
    }

    return err;
}

// disconnect because of security block
void hci_disconnect_security_block(hci_con_handle_t con_handle){
    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection) return;
    connection->bonding_flags |= BONDING_DISCONNECT_SECURITY_BLOCK;
}


// Configure Secure Simple Pairing

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

/**
 * pre: numcmds >= 0 - it's allowed to send a command to the controller
 */
int hci_send_cmd(const hci_cmd_t *cmd, ...){

    if (!hci_can_send_command_packet_now()){ 
        log_error("hci_send_cmd called but cannot send packet now");
        return 0;
    }

    // for HCI INITIALIZATION
    // log_info("hci_send_cmd: opcode %04x", cmd->opcode);
    hci_stack->last_cmd_opcode = cmd->opcode;

    hci_reserve_packet_buffer();
    uint8_t * packet = hci_stack->hci_packet_buffer;

    va_list argptr;
    va_start(argptr, cmd);
    uint16_t size = hci_cmd_create_from_template(packet, cmd, argptr);
    va_end(argptr);

    return hci_send_cmd_packet(packet, size);
}

// Create various non-HCI events. 
// TODO: generalize, use table similar to hci_create_command

static void hci_emit_event(uint8_t * event, uint16_t size, int dump){
    // dump packet
    if (dump) {
        hci_dump_packet( HCI_EVENT_PACKET, 0, event, size);
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

void hci_emit_state(void){
    log_info("BTSTACK_EVENT_STATE %u", hci_stack->state);
    uint8_t event[3];
    event[0] = BTSTACK_EVENT_STATE;
    event[1] = sizeof(event) - 2;
    event[2] = hci_stack->state;
    hci_emit_event(event, sizeof(event), 1);
}

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

static void hci_emit_le_connection_complete(uint8_t address_type, bd_addr_t address, hci_con_handle_t con_handle, uint8_t status){
    uint8_t event[21];
    event[0] = HCI_EVENT_LE_META;
    event[1] = sizeof(event) - 2;
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

static void hci_emit_disconnection_complete(hci_con_handle_t con_handle, uint8_t reason){
    uint8_t event[6];
    event[0] = HCI_EVENT_DISCONNECTION_COMPLETE;
    event[1] = sizeof(event) - 2;
    event[2] = 0; // status = OK
    little_endian_store_16(event, 3, con_handle);
    event[5] = reason;
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

static void hci_emit_nr_connections_changed(void){
    log_info("BTSTACK_EVENT_NR_CONNECTIONS_CHANGED %u", nr_hci_connections());
    uint8_t event[3];
    event[0] = BTSTACK_EVENT_NR_CONNECTIONS_CHANGED;
    event[1] = sizeof(event) - 2;
    event[2] = nr_hci_connections();
    hci_emit_event(event, sizeof(event), 1);
}

static void hci_emit_hci_open_failed(void){
    log_info("BTSTACK_EVENT_POWERON_FAILED");
    uint8_t event[2];
    event[0] = BTSTACK_EVENT_POWERON_FAILED;
    event[1] = sizeof(event) - 2;
    hci_emit_event(event, sizeof(event), 1);
}

static void hci_emit_discoverable_enabled(uint8_t enabled){
    log_info("BTSTACK_EVENT_DISCOVERABLE_ENABLED %u", enabled);
    uint8_t event[3];
    event[0] = BTSTACK_EVENT_DISCOVERABLE_ENABLED;
    event[1] = sizeof(event) - 2;
    event[2] = enabled;
    hci_emit_event(event, sizeof(event), 1);
}

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

static void hci_emit_dedicated_bonding_result(bd_addr_t address, uint8_t status){
    log_info("hci_emit_dedicated_bonding_result %u ", status);
    uint8_t event[9];
    int pos = 0;
    event[pos++] = GAP_EVENT_DEDICATED_BONDING_COMPLETED;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = status;
    reverse_bd_addr(address, &event[pos]);
    hci_emit_event(event, sizeof(event), 1);
}

// query if remote side supports eSCO
int hci_remote_esco_supported(hci_con_handle_t con_handle){
    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection) return 0;
    return connection->remote_supported_feature_eSCO;
}

// query if remote side supports SSP
int hci_remote_ssp_supported(hci_con_handle_t con_handle){
    hci_connection_t * connection = hci_connection_for_handle(con_handle);
    if (!connection) return 0;
    return (connection->bonding_flags & BONDING_REMOTE_SUPPORTS_SSP) ? 1 : 0;
}

int gap_ssp_supported_on_both_sides(hci_con_handle_t handle){
    return hci_local_ssp_activated() && hci_remote_ssp_supported(handle);
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

static gap_security_level_t gap_security_level_for_connection(hci_connection_t * connection){
    if (!connection) return LEVEL_0;
    if ((connection->authentication_flags & CONNECTION_ENCRYPTED) == 0) return LEVEL_0;
    return gap_security_level_for_link_key_type(connection->link_key_type);
}    


int gap_mitm_protection_required_for_security_level(gap_security_level_t level){
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
    gap_security_level_t current_level = gap_security_level(con_handle);
    log_info("gap_request_security_level %u, current level %u", requested_level, current_level);
    if (current_level >= requested_level){
        hci_emit_security_level(con_handle, current_level);
        return;
    }

    connection->requested_security_level = requested_level;

#if 0
    // sending encryption request without a link key results in an error. 
    // TODO: figure out how to use it properly

    // would enabling ecnryption suffice (>= LEVEL_2)?
    if (hci_stack->link_key_db){
        link_key_type_t link_key_type;
        link_key_t      link_key;
        if (hci_stack->link_key_db->get_link_key( &connection->address, &link_key, &link_key_type)){
            if (gap_security_level_for_link_key_type(link_key_type) >= requested_level){
                connection->bonding_flags |= BONDING_SEND_ENCRYPTION_REQUEST;
                return;
            }
        }
    }
#endif

    // try to authenticate connection
    connection->bonding_flags |= BONDING_SEND_AUTHENTICATE_REQUEST;
    hci_run();
}

/**
 * @brief start dedicated bonding with device. disconnect after bonding
 * @param device
 * @param request MITM protection
 * @result GAP_DEDICATED_BONDING_COMPLETE
 */
int gap_dedicated_bonding(bd_addr_t device, int mitm_protection_required){

    // create connection state machine
    hci_connection_t * connection = create_connection_for_bd_addr_and_type(device, BD_ADDR_TYPE_CLASSIC);

    if (!connection){
        return BTSTACK_MEMORY_ALLOC_FAILED;
    }

    // delete linkn key
    gap_drop_link_key_for_bd_addr(device);

    // configure LEVEL_2/3, dedicated bonding
    connection->state = SEND_CREATE_CONNECTION;    
    connection->requested_security_level = mitm_protection_required ? LEVEL_3 : LEVEL_2;
    log_info("gap_dedicated_bonding, mitm %d -> level %u", mitm_protection_required, connection->requested_security_level);
    connection->bonding_flags = BONDING_DEDICATED;

    // wait for GAP Security Result and send GAP Dedicated Bonding complete

    // handle: connnection failure (connection complete != ok)
    // handle: authentication failure
    // handle: disconnect on done

    hci_run();

    return 0;
}

void gap_set_local_name(const char * local_name){
    hci_stack->local_name = local_name;
}

void gap_start_scan(void){
    if (hci_stack->le_scanning_state == LE_SCANNING) return;
    hci_stack->le_scanning_state = LE_START_SCAN;
    hci_run();
}

void gap_stop_scan(void){
    if ( hci_stack->le_scanning_state == LE_SCAN_IDLE) return;
    hci_stack->le_scanning_state = LE_STOP_SCAN;
    hci_run();
}

void gap_set_scan_parameters(uint8_t scan_type, uint16_t scan_interval, uint16_t scan_window){
    hci_stack->le_scan_type     = scan_type;
    hci_stack->le_scan_interval = scan_interval;
    hci_stack->le_scan_window   = scan_window;
    hci_run();
}

uint8_t gap_connect(bd_addr_t addr, bd_addr_type_t addr_type){
    hci_connection_t * conn = hci_connection_for_bd_addr_and_type(addr, addr_type);
    if (!conn){
        log_info("gap_connect: no connection exists yet, creating context");
        conn = create_connection_for_bd_addr_and_type(addr, addr_type);
        if (!conn){
            // notify client that alloc failed
            hci_emit_le_connection_complete(addr_type, addr, 0, BTSTACK_MEMORY_ALLOC_FAILED);
            log_info("gap_connect: failed to alloc hci_connection_t");
            return GATT_CLIENT_NOT_CONNECTED; // don't sent packet to controller
        }
        conn->state = SEND_CREATE_CONNECTION;
        log_info("gap_connect: send create connection next");
        hci_run();
        return 0;
    }
    
    if (!hci_is_le_connection(conn) ||
        conn->state == SEND_CREATE_CONNECTION ||
        conn->state == SENT_CREATE_CONNECTION) {
        hci_emit_le_connection_complete(conn->address_type, conn->address, 0, ERROR_CODE_COMMAND_DISALLOWED);
        log_error("gap_connect: classic connection or connect is already being created");
        return GATT_CLIENT_IN_WRONG_STATE;
    }
    
    log_info("gap_connect: context exists with state %u", conn->state);
    hci_emit_le_connection_complete(conn->address_type, conn->address, conn->con_handle, 0);
    hci_run();
    return 0;
}

// @assumption: only a single outgoing LE Connection exists
static hci_connection_t * gap_get_outgoing_connection(void){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) hci_stack->connections; it ; it = it->next){
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
    hci_connection_t * conn = gap_get_outgoing_connection();
    if (!conn) return 0;
    switch (conn->state){
        case SEND_CREATE_CONNECTION:
            // skip sending create connection and emit event instead
            hci_emit_le_connection_complete(conn->address_type, conn->address, 0, ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER);
            btstack_linked_list_remove(&hci_stack->connections, (btstack_linked_item_t *) conn);
            btstack_memory_hci_connection_free( conn );
            break;            
        case SENT_CREATE_CONNECTION:
            // request to send cancel connection
            conn->state = SEND_CANCEL_CONNECTION;
            hci_run();
            break;
        default:
            break;
    }
    return 0;
}

/**
 * @brief Updates the connection parameters for a given LE connection
 * @param handle
 * @param conn_interval_min (unit: 1.25ms)
 * @param conn_interval_max (unit: 1.25ms)
 * @param conn_latency
 * @param supervision_timeout (unit: 10ms)
 * @returns 0 if ok
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
 * @returns 0 if ok
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
    hci_run();
    return 0;
}

static void gap_advertisments_changed(void){
    // disable advertisements before updating adv, scan data, or adv params
    if (hci_stack->le_advertisements_active){
        hci_stack->le_advertisements_todo |= LE_ADVERTISEMENT_TASKS_DISABLE | LE_ADVERTISEMENT_TASKS_ENABLE;
    }
    hci_run();
}

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
    gap_advertisments_changed();
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
    gap_advertisments_changed();
}

/**
 * @brief Set Advertisement Parameters
 * @param adv_int_min
 * @param adv_int_max
 * @param adv_type
 * @param own_address_type
 * @param direct_address_type
 * @param direct_address
 * @param channel_map
 * @param filter_policy
 *
 * @note internal use. use gap_advertisements_set_params from gap_le.h instead.
 */
 void hci_le_advertisements_set_params(uint16_t adv_int_min, uint16_t adv_int_max, uint8_t adv_type,
    uint8_t own_address_type, uint8_t direct_address_typ, bd_addr_t direct_address,
    uint8_t channel_map, uint8_t filter_policy) {

    hci_stack->le_advertisements_interval_min = adv_int_min;
    hci_stack->le_advertisements_interval_max = adv_int_max;
    hci_stack->le_advertisements_type = adv_type;
    hci_stack->le_advertisements_own_address_type = own_address_type;
    hci_stack->le_advertisements_direct_address_type = direct_address_typ;
    hci_stack->le_advertisements_channel_map = channel_map;
    hci_stack->le_advertisements_filter_policy = filter_policy;
    memcpy(hci_stack->le_advertisements_direct_address, direct_address, 6);

    hci_stack->le_advertisements_todo |= LE_ADVERTISEMENT_TASKS_SET_PARAMS;
    gap_advertisments_changed();
 }

/**
 * @brief Enable/Disable Advertisements
 * @param enabled
 */
void gap_advertisements_enable(int enabled){
    hci_stack->le_advertisements_enabled = enabled;
    if (enabled && !hci_stack->le_advertisements_active){
        hci_stack->le_advertisements_todo |= LE_ADVERTISEMENT_TASKS_ENABLE;
    }
    if (!enabled && hci_stack->le_advertisements_active){
        hci_stack->le_advertisements_todo |= LE_ADVERTISEMENT_TASKS_DISABLE;
    }
    hci_run();
}


uint8_t gap_disconnect(hci_con_handle_t handle){
    hci_connection_t * conn = hci_connection_for_handle(handle);
    if (!conn){
        hci_emit_disconnection_complete(handle, 0);
        return 0;
    }
    conn->state = SEND_DISCONNECT;
    hci_run();
    return 0;
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
        case BD_ADDR_TYPE_CLASSIC:
            return GAP_CONNECTION_ACL;
        default:
            return GAP_CONNECTION_INVALID;
    }
}

#ifdef ENABLE_BLE

/**
 * @brief Auto Connection Establishment - Start Connecting to device
 * @param address_typ
 * @param address
 * @returns 0 if ok
 */
int gap_auto_connection_start(bd_addr_type_t address_type, bd_addr_t address){
    // check capacity
    int num_entries = btstack_linked_list_count(&hci_stack->le_whitelist);
    if (num_entries >= hci_stack->le_whitelist_capacity) return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    whitelist_entry_t * entry = btstack_memory_whitelist_entry_get();
    if (!entry) return BTSTACK_MEMORY_ALLOC_FAILED;
    entry->address_type = address_type;
    memcpy(entry->address, address, 6);
    entry->state = LE_WHITELIST_ADD_TO_CONTROLLER;
    btstack_linked_list_add(&hci_stack->le_whitelist, (btstack_linked_item_t*) entry);
    hci_run();
    return 0;
}

static void hci_remove_from_whitelist(bd_addr_type_t address_type, bd_addr_t address){
    btstack_linked_list_iterator_t it;
    btstack_linked_list_iterator_init(&it, &hci_stack->le_whitelist);
    while (btstack_linked_list_iterator_has_next(&it)){
        whitelist_entry_t * entry = (whitelist_entry_t*) btstack_linked_list_iterator_next(&it);
        if (entry->address_type != address_type) continue;
        if (memcmp(entry->address, address, 6) != 0) continue;
        if (entry->state & LE_WHITELIST_ON_CONTROLLER){
            // remove from controller if already present
            entry->state |= LE_WHITELIST_REMOVE_FROM_CONTROLLER;
            continue;
        }
        // direclty remove entry from whitelist
        btstack_linked_list_iterator_remove(&it);
        btstack_memory_whitelist_entry_free(entry);
    }
}

/**
 * @brief Auto Connection Establishment - Stop Connecting to device
 * @param address_typ
 * @param address
 * @returns 0 if ok
 */
int gap_auto_connection_stop(bd_addr_type_t address_type, bd_addr_t address){
    hci_remove_from_whitelist(address_type, address);
    hci_run();
    return 0;
}

/**
 * @brief Auto Connection Establishment - Stop everything
 * @note  Convenience function to stop all active auto connection attempts
 */
void gap_auto_connection_stop_all(void){
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
    hci_run();
}

#endif

/**
 * @brief Set Extended Inquiry Response data
 * @param eir_data size 240 bytes, is not copied make sure memory is accessible during stack startup
 * @note has to be done before stack starts up
 */
void gap_set_extended_inquiry_response(const uint8_t * data){
    hci_stack->eir_data = data;
}

/**
 * @brief Set inquiry mode: standard, with RSSI, with RSSI + Extended Inquiry Results. Has to be called before power on.
 * @param inquriy_mode see bluetooth_defines.h
 */
void hci_set_inquiry_mode(inquiry_mode_t mode){
    hci_stack->inquiry_mode = mode;
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

/** @brief Get SCO packet length for current SCO Voice setting
 *  @note  Using SCO packets of the exact length is required for USB transfer
 *  @return Length of SCO packets in bytes (not audio frames)
 */
int hci_get_sco_packet_length(void){
    // see Core Spec for H2 USB Transfer. 
    if (hci_stack->sco_voice_setting & 0x0020) return 51;
    return 27;
}

/**
 * @brief Set callback for Bluetooth Hardware Error
 */
void hci_set_hardware_error_callback(void (*fn)(uint8_t error)){
    hci_stack->hardware_error_callback = fn;
}

/**
 * @brief Set callback for local information from Bluetooth controller right after HCI Reset
 * @note Can be used to select chipset driver dynamically during startup
 */
void hci_set_local_version_information_callback(void (*fn)(uint8_t * local_version_information)){
    hci_stack->local_version_information_callback = fn;
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
