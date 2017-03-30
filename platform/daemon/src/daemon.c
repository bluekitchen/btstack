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

#define __BTSTACK_FILE__ "daemon.c"

/*
 *  daemon.c
 *
 *  Created by Matthias Ringwald on 7/1/09.
 *
 *  BTstack background daemon
 *
 */

#include "btstack_config.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include <getopt.h>

#include "btstack.h"
#include "btstack_client.h"
#include "btstack_debug.h"
#include "btstack_device_name_db.h"
#include "btstack_event.h"
#include "btstack_linked_list.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_posix.h"
#include "btstack_version.h"
#include "classic/btstack_link_key_db.h"
#include "classic/rfcomm.h"
#include "classic/sdp_server.h"
#include "classic/sdp_client.h"
#include "classic/sdp_client_rfcomm.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "hci_transport.h"
#include "l2cap.h"
#include "rfcomm_service_db.h"
#include "socket_connection.h"

#ifdef ENABLE_BLE
#include "ble/gatt_client.h"
#include "ble/att_server.h"
#include "ble/att_db.h"
#include "ble/le_device_db.h"
#include "ble/sm.h"
#endif

#ifdef HAVE_PLATFORM_IPHONE_OS
#include <CoreFoundation/CoreFoundation.h>
#include <notify.h>
#include "../port/ios/src/btstack_control_iphone.h"
#include "../port/ios/src/platform_iphone.h"
// support for "enforece wake device" in h4 - used by iOS power management
extern void hci_transport_h4_iphone_set_enforce_wake_device(char *path);
#endif

// copy of prototypes
const btstack_device_name_db_t * btstack_device_name_db_corefoundation_instance(void);
const btstack_device_name_db_t * btstack_device_name_db_fs_instance(void);
const btstack_link_key_db_t * btstack_link_key_db_corefoundation_instance(void);
const btstack_link_key_db_t * btstack_link_key_db_fs_instance(void);

#ifndef BTSTACK_LOG_FILE
#define BTSTACK_LOG_FILE "/tmp/hci_dump.pklg"
#endif

// use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
#ifndef BTSTACK_LOG_TYPE
#define BTSTACK_LOG_TYPE HCI_DUMP_PACKETLOGGER 
#endif

#define DAEMON_NO_ACTIVE_CLIENT_TIMEOUT 10000

#define ATT_MAX_LONG_ATTRIBUTE_SIZE 512


#define SERVICE_LENGTH                      20
#define CHARACTERISTIC_LENGTH               24
#define CHARACTERISTIC_DESCRIPTOR_LENGTH    18

// ATT_MTU - 1
#define ATT_MAX_ATTRIBUTE_SIZE 22

// HCI CMD OGF/OCF
#define READ_CMD_OGF(buffer) (buffer[1] >> 2)
#define READ_CMD_OCF(buffer) ((buffer[1] & 0x03) << 8 | buffer[0])

typedef struct {
    // linked list - assert: first field
    btstack_linked_item_t    item;
    
    // connection
    connection_t * connection;

    btstack_linked_list_t rfcomm_cids;
    btstack_linked_list_t rfcomm_services;
    btstack_linked_list_t l2cap_cids;
    btstack_linked_list_t l2cap_psms;
    btstack_linked_list_t sdp_record_handles;
    btstack_linked_list_t gatt_con_handles;
    // power mode
    HCI_POWER_MODE power_mode;
    
    // discoverable
    uint8_t        discoverable;
    
} client_state_t;

typedef struct btstack_linked_list_uint32 {
    btstack_linked_item_t   item;
    uint32_t        value;
} btstack_linked_list_uint32_t;

typedef struct btstack_linked_list_connection {
    btstack_linked_item_t   item;
    connection_t  * connection;
} btstack_linked_list_connection_t;

typedef struct btstack_linked_list_gatt_client_helper{
    btstack_linked_item_t item;
    hci_con_handle_t con_handle;
    connection_t * active_connection;   // the one that started the current query
    btstack_linked_list_t  all_connections;     // list of all connections that ever used this helper
    uint16_t characteristic_length;
    uint16_t characteristic_handle;
    uint8_t  characteristic_buffer[10 + ATT_MAX_LONG_ATTRIBUTE_SIZE];   // header for sending event right away
    uint8_t  long_query_type;
} btstack_linked_list_gatt_client_helper_t;

// MARK: prototypes
static void handle_sdp_rfcomm_service_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
#ifdef ENABLE_BLE
static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size);
#endif
static void dummy_bluetooth_status_handler(BLUETOOTH_STATE state);
static client_state_t * client_for_connection(connection_t *connection);
static int              clients_require_power_on(void);
static int              clients_require_discoverable(void);
static void              clients_clear_power_request(void);
static void start_power_off_timer(void);
static void stop_power_off_timer(void);
static client_state_t * client_for_connection(connection_t *connection);
static void hci_emit_system_bluetooth_enabled(uint8_t enabled);
static void rfcomm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size);


// MARK: globals

#ifdef HAVE_TRANSPORT_H4
static hci_transport_config_uart_t hci_transport_config_uart;
#endif

static const hci_transport_t * transport;
static btstack_timer_source_t timeout;
static uint8_t timeout_active = 0;
static int power_management_sleep = 0;
static btstack_linked_list_t clients = NULL;        // list of connected clients `
#ifdef ENABLE_BLE
static btstack_linked_list_t gatt_client_helpers = NULL;   // list of used gatt client (helpers)
#endif

static void (*bluetooth_status_handler)(BLUETOOTH_STATE state) = dummy_bluetooth_status_handler;

static btstack_packet_callback_registration_t hci_event_callback_registration;

static int global_enable = 0;

static btstack_link_key_db_t    const * btstack_link_key_db = NULL;
static btstack_device_name_db_t const * btstack_device_name_db = NULL;
// static int rfcomm_channel_generator = 1;

static uint8_t   attribute_value[1000];
static const int attribute_value_buffer_size = sizeof(attribute_value);
static uint8_t serviceSearchPattern[200];
static uint8_t attributeIDList[50];
static void * sdp_client_query_connection;
    
static int loggingEnabled;

// stashed code from l2cap.c and rfcomm.c -- needed for new implementation
#if 0
static void l2cap_emit_credits(l2cap_channel_t *channel, uint8_t credits) {
    
    log_info("DAEMON_EVENT_L2CAP_CREDITS local_cid 0x%x credits %u", channel->local_cid, credits);
    
    uint8_t event[5];
    event[0] = DAEMON_EVENT_L2CAP_CREDITS;
    event[1] = sizeof(event) - 2;
    little_endian_store_16(event, 2, channel->local_cid);
    event[4] = credits;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    l2cap_dispatch(channel, HCI_EVENT_PACKET, event, sizeof(event));
}

static void l2cap_hand_out_credits(void){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, &l2cap_channels);
    while (btstack_linked_list_iterator_has_next(&it)){
        l2cap_channel_t * channel = (l2cap_channel_t *) btstack_linked_list_iterator_next(&it);
        if (channel->state != L2CAP_STATE_OPEN) continue;
        if (!hci_number_free_acl_slots_for_handle(channel->handle)) return;
        l2cap_emit_credits(channel, 1);
    }
}
static void rfcomm_emit_credits(rfcomm_channel_t * channel, uint8_t credits) {
    log_info("DAEMON_EVENT_RFCOMM_CREDITS cid 0x%02x credits %u", channel->rfcomm_cid, credits);
    uint8_t event[5];
    event[0] = DAEMON_EVENT_RFCOMM_CREDITS;
    event[1] = sizeof(event) - 2;
    little_endian_store_16(event, 2, channel->rfcomm_cid);
    event[4] = credits;
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
    (*app_packet_handler)(HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
}
static void rfcomm_hand_out_credits(void){
    btstack_linked_item_t * it;
    for (it = (btstack_linked_item_t *) rfcomm_channels; it ; it = it->next){
        rfcomm_channel_t * channel = (rfcomm_channel_t *) it;
        if (channel->state != RFCOMM_CHANNEL_OPEN) {
            // log_info("DAEMON_EVENT_RFCOMM_CREDITS: multiplexer not open");
            continue;
        }
        if (!channel->credits_outgoing) {
            // log_info("DAEMON_EVENT_RFCOMM_CREDITS: no outgoing credits");
            continue;
        }
        // channel open, multiplexer has l2cap credits and we didn't hand out credit before -> go!
        // log_info("DAEMON_EVENT_RFCOMM_CREDITS: 1");
        rfcomm_emit_credits(channel, 1);
    }        
}

#endif

static void dummy_bluetooth_status_handler(BLUETOOTH_STATE state){
    log_info("Bluetooth status: %u\n", state);
};

static void daemon_no_connections_timeout(struct btstack_timer_source *ts){
    if (clients_require_power_on()) return;    // false alarm :)
    log_info("No active client connection for %u seconds -> POWER OFF\n", DAEMON_NO_ACTIVE_CLIENT_TIMEOUT/1000);
    hci_power_control(HCI_POWER_OFF);
}


static void add_uint32_to_list(btstack_linked_list_t *list, uint32_t value){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, list);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_uint32_t * item = (btstack_linked_list_uint32_t*) btstack_linked_list_iterator_next(&it);
        if ( item->value == value) return; // already in list
    } 

    btstack_linked_list_uint32_t * item = malloc(sizeof(btstack_linked_list_uint32_t));
    if (!item) return; 
    item->value = value;
    btstack_linked_list_add(list, (btstack_linked_item_t *) item);
}

static void remove_and_free_uint32_from_list(btstack_linked_list_t *list, uint32_t value){
    btstack_linked_list_iterator_t it;    
    btstack_linked_list_iterator_init(&it, list);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_uint32_t * item = (btstack_linked_list_uint32_t*) btstack_linked_list_iterator_next(&it);
        if ( item->value != value) continue;
        btstack_linked_list_remove(list, (btstack_linked_item_t *) item);
        free(item);
    } 
}

static void daemon_add_client_rfcomm_service(connection_t * connection, uint16_t service_channel){
    client_state_t * client_state = client_for_connection(connection);
    if (!client_state) return;
    add_uint32_to_list(&client_state->rfcomm_services, service_channel);    
}

static void daemon_remove_client_rfcomm_service(connection_t * connection, uint16_t service_channel){
    client_state_t * client_state = client_for_connection(connection);
    if (!client_state) return;
    remove_and_free_uint32_from_list(&client_state->rfcomm_services, service_channel);    
}

static void daemon_add_client_rfcomm_channel(connection_t * connection, uint16_t cid){
    client_state_t * client_state = client_for_connection(connection);
    if (!client_state) return;
    add_uint32_to_list(&client_state->rfcomm_cids, cid);
}

static void daemon_remove_client_rfcomm_channel(connection_t * connection, uint16_t cid){
    client_state_t * client_state = client_for_connection(connection);
    if (!client_state) return;
    remove_and_free_uint32_from_list(&client_state->rfcomm_cids, cid);
}

static void daemon_add_client_l2cap_service(connection_t * connection, uint16_t psm){
    client_state_t * client_state = client_for_connection(connection);
    if (!client_state) return;
    add_uint32_to_list(&client_state->l2cap_psms, psm);
}

static void daemon_remove_client_l2cap_service(connection_t * connection, uint16_t psm){
    client_state_t * client_state = client_for_connection(connection);
    if (!client_state) return;
    remove_and_free_uint32_from_list(&client_state->l2cap_psms, psm);
}

static void daemon_add_client_l2cap_channel(connection_t * connection, uint16_t cid){
    client_state_t * client_state = client_for_connection(connection);
    if (!client_state) return;
    add_uint32_to_list(&client_state->l2cap_cids, cid);
}

static void daemon_remove_client_l2cap_channel(connection_t * connection, uint16_t cid){
    client_state_t * client_state = client_for_connection(connection);
    if (!client_state) return;
    remove_and_free_uint32_from_list(&client_state->l2cap_cids, cid);
}

static void daemon_add_client_sdp_service_record_handle(connection_t * connection, uint32_t handle){
    client_state_t * client_state = client_for_connection(connection);
    if (!client_state) return;
    add_uint32_to_list(&client_state->sdp_record_handles, handle);    
}

static void daemon_remove_client_sdp_service_record_handle(connection_t * connection, uint32_t handle){
    client_state_t * client_state = client_for_connection(connection);
    if (!client_state) return;
    remove_and_free_uint32_from_list(&client_state->sdp_record_handles, handle);    
}

#ifdef ENABLE_BLE
static void daemon_add_gatt_client_handle(connection_t * connection, uint32_t handle){
    client_state_t * client_state = client_for_connection(connection);
    if (!client_state) return;
    
    // check if handle already exists in the gatt_con_handles list
    btstack_linked_list_iterator_t it;
    int handle_found = 0;
    btstack_linked_list_iterator_init(&it, &client_state->gatt_con_handles);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_uint32_t * item = (btstack_linked_list_uint32_t*) btstack_linked_list_iterator_next(&it);
        if (item->value == handle){ 
            handle_found = 1;
            break;
        }
    }
    // if handle doesn't exist add it to gatt_con_handles
    if (!handle_found){
        add_uint32_to_list(&client_state->gatt_con_handles, handle);
    }
    
    // check if there is a helper with given handle
    btstack_linked_list_gatt_client_helper_t * gatt_helper = NULL;
    btstack_linked_list_iterator_init(&it, &gatt_client_helpers);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_gatt_client_helper_t * item = (btstack_linked_list_gatt_client_helper_t*) btstack_linked_list_iterator_next(&it);
        if (item->con_handle == handle){
            gatt_helper = item;
            break;
        }
    }

    // if gatt_helper doesn't exist, create it and add it to gatt_client_helpers list
    if (!gatt_helper){
        gatt_helper = calloc(sizeof(btstack_linked_list_gatt_client_helper_t), 1);
        if (!gatt_helper) return; 
        gatt_helper->con_handle = handle;
        btstack_linked_list_add(&gatt_client_helpers, (btstack_linked_item_t *) gatt_helper);
    }

    // check if connection exists
    int connection_found = 0;
    btstack_linked_list_iterator_init(&it, &gatt_helper->all_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_connection_t * item = (btstack_linked_list_connection_t*) btstack_linked_list_iterator_next(&it);
        if (item->connection == connection){
            connection_found = 1;
            break;
        }
    }

    // if connection is not found, add it to the all_connections, and set it as active connection
    if (!connection_found){
        btstack_linked_list_connection_t * con = calloc(sizeof(btstack_linked_list_connection_t), 1);
        if (!con) return;
        con->connection = connection;
        btstack_linked_list_add(&gatt_helper->all_connections, (btstack_linked_item_t *)con);
    }
}


static void daemon_remove_gatt_client_handle(connection_t * connection, uint32_t handle){
    // PART 1 - uses connection & handle
    // might be extracted or vanish totally
    client_state_t * client_state = client_for_connection(connection);
    if (!client_state) return;
    
    btstack_linked_list_iterator_t it;    
    // remove handle from gatt_con_handles list
    btstack_linked_list_iterator_init(&it, &client_state->gatt_con_handles);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_uint32_t * item = (btstack_linked_list_uint32_t*) btstack_linked_list_iterator_next(&it);
        if (item->value == handle){
            btstack_linked_list_remove(&client_state->gatt_con_handles, (btstack_linked_item_t *) item);
            free(item);
        }
    }

    // PART 2 - only uses handle

    // find helper with given handle
    btstack_linked_list_gatt_client_helper_t * helper = NULL;
    btstack_linked_list_iterator_init(&it, &gatt_client_helpers);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_gatt_client_helper_t * item = (btstack_linked_list_gatt_client_helper_t*) btstack_linked_list_iterator_next(&it);
        if (item->con_handle == handle){
            helper = item;
            break;
        }
    }

    if (!helper) return;
    // remove connection from helper
    btstack_linked_list_iterator_init(&it, &helper->all_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_connection_t * item = (btstack_linked_list_connection_t*) btstack_linked_list_iterator_next(&it);
        if (item->connection == connection){
            btstack_linked_list_remove(&helper->all_connections, (btstack_linked_item_t *) item);
            free(item);
            break;
        }
    }

    if (helper->active_connection == connection){
        helper->active_connection = NULL;
    }
    // if helper has no more connections, call disconnect
    if (helper->all_connections == NULL){
        gap_disconnect((hci_con_handle_t) helper->con_handle);
    }
}


static void daemon_remove_gatt_client_helper(uint32_t con_handle){
    btstack_linked_list_iterator_t it, cl;    
    // find helper with given handle
    btstack_linked_list_gatt_client_helper_t * helper = NULL;
    btstack_linked_list_iterator_init(&it, &gatt_client_helpers);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_gatt_client_helper_t * item = (btstack_linked_list_gatt_client_helper_t*) btstack_linked_list_iterator_next(&it);
        if (item->con_handle == con_handle){
            helper = item;
            break;
        }
    }

    if (!helper) return;

    // remove all connection from helper
    btstack_linked_list_iterator_init(&it, &helper->all_connections);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_connection_t * item = (btstack_linked_list_connection_t*) btstack_linked_list_iterator_next(&it);
        btstack_linked_list_remove(&helper->all_connections, (btstack_linked_item_t *) item);
        free(item);
    }

    btstack_linked_list_remove(&gatt_client_helpers, (btstack_linked_item_t *) helper);
    free(helper);
    
    btstack_linked_list_iterator_init(&cl, &clients);
    while (btstack_linked_list_iterator_has_next(&cl)){
        client_state_t * client_state = (client_state_t *) btstack_linked_list_iterator_next(&cl);
        btstack_linked_list_iterator_init(&it, &client_state->gatt_con_handles);
        while (btstack_linked_list_iterator_has_next(&it)){
            btstack_linked_list_uint32_t * item = (btstack_linked_list_uint32_t*) btstack_linked_list_iterator_next(&it);
            if (item->value == con_handle){
                btstack_linked_list_remove(&client_state->gatt_con_handles, (btstack_linked_item_t *) item);
                free(item);
            }
        }
    }
}
#endif

static void daemon_rfcomm_close_connection(client_state_t * daemon_client){
    btstack_linked_list_iterator_t it;  
    btstack_linked_list_t *rfcomm_services = &daemon_client->rfcomm_services;
    btstack_linked_list_t *rfcomm_cids = &daemon_client->rfcomm_cids;
    
    btstack_linked_list_iterator_init(&it, rfcomm_services);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_uint32_t * item = (btstack_linked_list_uint32_t*) btstack_linked_list_iterator_next(&it);
        rfcomm_unregister_service(item->value);
        btstack_linked_list_remove(rfcomm_services, (btstack_linked_item_t *) item);
        free(item);
    }

    btstack_linked_list_iterator_init(&it, rfcomm_cids);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_uint32_t * item = (btstack_linked_list_uint32_t*) btstack_linked_list_iterator_next(&it);
        rfcomm_disconnect(item->value);
        btstack_linked_list_remove(rfcomm_cids, (btstack_linked_item_t *) item);
        free(item);
    }
}


static void daemon_l2cap_close_connection(client_state_t * daemon_client){
    btstack_linked_list_iterator_t it;  
    btstack_linked_list_t *l2cap_psms = &daemon_client->l2cap_psms;
    btstack_linked_list_t *l2cap_cids = &daemon_client->l2cap_cids;
    
    btstack_linked_list_iterator_init(&it, l2cap_psms);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_uint32_t * item = (btstack_linked_list_uint32_t*) btstack_linked_list_iterator_next(&it);
        l2cap_unregister_service(item->value);
        btstack_linked_list_remove(l2cap_psms, (btstack_linked_item_t *) item);
        free(item);
    }

    btstack_linked_list_iterator_init(&it, l2cap_cids);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_uint32_t * item = (btstack_linked_list_uint32_t*) btstack_linked_list_iterator_next(&it);
        l2cap_disconnect(item->value, 0); // note: reason isn't used
        btstack_linked_list_remove(l2cap_cids, (btstack_linked_item_t *) item);
        free(item);
    }
}

static void daemon_sdp_close_connection(client_state_t * daemon_client){
    btstack_linked_list_t * list = &daemon_client->sdp_record_handles;
    btstack_linked_list_iterator_t it;  
    btstack_linked_list_iterator_init(&it, list);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_uint32_t * item = (btstack_linked_list_uint32_t*) btstack_linked_list_iterator_next(&it);
        sdp_unregister_service(item->value);
        btstack_linked_list_remove(list, (btstack_linked_item_t *) item);
        free(item);
    }
}

static connection_t * connection_for_l2cap_cid(uint16_t cid){
    btstack_linked_list_iterator_t cl;
    btstack_linked_list_iterator_init(&cl, &clients);
    while (btstack_linked_list_iterator_has_next(&cl)){
        client_state_t * client_state = (client_state_t *) btstack_linked_list_iterator_next(&cl);
        btstack_linked_list_iterator_t it;
        btstack_linked_list_iterator_init(&it, &client_state->l2cap_cids);
        while (btstack_linked_list_iterator_has_next(&it)){
            btstack_linked_list_uint32_t * item = (btstack_linked_list_uint32_t*) btstack_linked_list_iterator_next(&it);
            if (item->value == cid){
                return client_state->connection;
            }
        }
    }
    return NULL;
}

static const uint8_t removeServiceRecordHandleAttributeIDList[] = { 0x36, 0x00, 0x05, 0x0A, 0x00, 0x01, 0xFF, 0xFF };

// register a service record
// pre: AttributeIDs are in ascending order
// pre: ServiceRecordHandle is first attribute and is not already registered in database
// @returns status
static uint32_t daemon_sdp_create_and_register_service(uint8_t * record){
    
    // create new handle
    uint32_t record_handle = sdp_create_service_record_handle();
    
    // calculate size of new service record: DES (2 byte len) 
    // + ServiceRecordHandle attribute (UINT16 UINT32) + size of existing attributes
    uint16_t recordSize =  3 + (3 + 5) + de_get_data_size(record);
        
    // alloc memory for new service record
    uint8_t * newRecord = malloc(recordSize);
    if (!newRecord) return 0;
    
    // create DES for new record
    de_create_sequence(newRecord);
    
    // set service record handle
    de_add_number(newRecord, DE_UINT, DE_SIZE_16, 0);
    de_add_number(newRecord, DE_UINT, DE_SIZE_32, record_handle);
    
    // add other attributes
    sdp_append_attributes_in_attributeIDList(record, (uint8_t *) removeServiceRecordHandleAttributeIDList, 0, recordSize, newRecord);
    
    uint8_t status = sdp_register_service(newRecord);

    if (status) {
        free(newRecord);
        return 0;
    }

    return record_handle;
}

static connection_t * connection_for_rfcomm_cid(uint16_t cid){
    btstack_linked_list_iterator_t cl;
    btstack_linked_list_iterator_init(&cl, &clients);
    while (btstack_linked_list_iterator_has_next(&cl)){
        client_state_t * client_state = (client_state_t *) btstack_linked_list_iterator_next(&cl);
        btstack_linked_list_iterator_t it;
        btstack_linked_list_iterator_init(&it, &client_state->rfcomm_cids);
        while (btstack_linked_list_iterator_has_next(&it)){
            btstack_linked_list_uint32_t * item = (btstack_linked_list_uint32_t*) btstack_linked_list_iterator_next(&it);
            if (item->value == cid){
                return client_state->connection;
            }
        }
    }
    return NULL;
}

#ifdef ENABLE_BLE
static void daemon_gatt_client_close_connection(connection_t * connection){
    client_state_t * client = client_for_connection(connection);
    if (!client) return;

    btstack_linked_list_iterator_t it; 
    btstack_linked_list_iterator_init(&it, &client->gatt_con_handles);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_uint32_t * item = (btstack_linked_list_uint32_t*) btstack_linked_list_iterator_next(&it);
        daemon_remove_gatt_client_handle(connection, item->value);
    }
}
#endif

static void daemon_disconnect_client(connection_t * connection){
    log_info("Daemon disconnect client %p\n",connection);

    client_state_t * client = client_for_connection(connection);
    if (!client) return;

    daemon_sdp_close_connection(client);
    daemon_rfcomm_close_connection(client);
    daemon_l2cap_close_connection(client);
#ifdef ENABLE_BLE
    // NOTE: experimental - disconnect all LE connections where GATT Client was used
    // gatt_client_disconnect_connection(connection);
    daemon_gatt_client_close_connection(connection);
#endif

    btstack_linked_list_remove(&clients, (btstack_linked_item_t *) client);
    free(client); 
}

static void hci_emit_btstack_version(void){
    log_info("DAEMON_EVENT_VERSION %u.%u", BTSTACK_MAJOR, BTSTACK_MINOR);
    uint8_t event[6];
    event[0] = DAEMON_EVENT_VERSION;
    event[1] = sizeof(event) - 2;
    event[2] = BTSTACK_MAJOR;
    event[3] = BTSTACK_MINOR;
    little_endian_store_16(event, 4, 3257);    // last SVN commit on Google Code + 1
    socket_connection_send_packet_all(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void hci_emit_system_bluetooth_enabled(uint8_t enabled){
    log_info("DAEMON_EVENT_SYSTEM_BLUETOOTH_ENABLED %u", enabled);
    uint8_t event[3];
    event[0] = DAEMON_EVENT_SYSTEM_BLUETOOTH_ENABLED;
    event[1] = sizeof(event) - 2;
    event[2] = enabled;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    socket_connection_send_packet_all(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void send_l2cap_connection_open_failed(connection_t * connection, bd_addr_t address, uint16_t psm, uint8_t status){
    // emit error - see l2cap.c:l2cap_emit_channel_opened(..)
    uint8_t event[23];
    memset(event, 0, sizeof(event));
    event[0] = L2CAP_EVENT_CHANNEL_OPENED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    reverse_bd_addr(address, &event[3]);
    // little_endian_store_16(event,  9, channel->handle);
    little_endian_store_16(event, 11, psm);
    // little_endian_store_16(event, 13, channel->local_cid);
    // little_endian_store_16(event, 15, channel->remote_cid);
    // little_endian_store_16(event, 17, channel->local_mtu);
    // little_endian_store_16(event, 19, channel->remote_mtu); 
    // little_endian_store_16(event, 21, channel->flush_timeout); 
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    socket_connection_send_packet(connection, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void l2cap_emit_service_registered(void *connection, uint8_t status, uint16_t psm){
    uint8_t event[5];
    event[0] = DAEMON_EVENT_L2CAP_SERVICE_REGISTERED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    little_endian_store_16(event, 3, psm);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    socket_connection_send_packet(connection, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void rfcomm_emit_service_registered(void *connection, uint8_t status, uint8_t channel){
    uint8_t event[4];
    event[0] = DAEMON_EVENT_RFCOMM_SERVICE_REGISTERED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    event[3] = channel;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, sizeof(event));
    socket_connection_send_packet(connection, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void send_rfcomm_create_channel_failed(void * connection, bd_addr_t addr, uint8_t server_channel, uint8_t status){
    // emit error - see rfcom.c:rfcomm_emit_channel_open_failed_outgoing_memory(..)
    uint8_t event[16];
    memset(event, 0, sizeof(event));
    uint8_t pos = 0;
    event[pos++] = RFCOMM_EVENT_CHANNEL_OPENED;
    event[pos++] = sizeof(event) - 2;
    event[pos++] = status;
    reverse_bd_addr(addr, &event[pos]); pos += 6;
    little_endian_store_16(event,  pos, 0);   pos += 2;
    event[pos++] = server_channel;
    little_endian_store_16(event, pos, 0); pos += 2;   // channel ID
    little_endian_store_16(event, pos, 0); pos += 2;   // max frame size
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
    socket_connection_send_packet(connection, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

// data: event(8), len(8), status(8), service_record_handle(32)
static void sdp_emit_service_registered(void *connection, uint32_t handle, uint8_t status) {
    uint8_t event[7];
    event[0] = DAEMON_EVENT_SDP_SERVICE_REGISTERED;
    event[1] = sizeof(event) - 2;
    event[2] = status;
    little_endian_store_32(event, 3, handle);
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
    socket_connection_send_packet(connection, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

#ifdef ENABLE_BLE

btstack_linked_list_gatt_client_helper_t * daemon_get_gatt_client_helper(hci_con_handle_t con_handle) {
    btstack_linked_list_iterator_t it;  
    if (!gatt_client_helpers) return NULL;
    log_info("daemon_get_gatt_client_helper for handle 0x%02x", con_handle);
    
    btstack_linked_list_iterator_init(&it, &gatt_client_helpers);
    while (btstack_linked_list_iterator_has_next(&it)){
        btstack_linked_list_gatt_client_helper_t * item = (btstack_linked_list_gatt_client_helper_t*) btstack_linked_list_iterator_next(&it);
        if (!item ) {
            log_info("daemon_get_gatt_client_helper gatt_client_helpers null item");
            break;
        } 
        if (item->con_handle == con_handle){
            return item;
        }
    }
    log_info("daemon_get_gatt_client_helper for handle 0x%02x is NULL.", con_handle);
    return NULL;
}

static void send_gatt_query_complete(connection_t * connection, hci_con_handle_t con_handle, uint8_t status){
    // @format H1
    uint8_t event[5];
    event[0] = GATT_EVENT_QUERY_COMPLETE;
    event[1] = 3;
    little_endian_store_16(event, 2, con_handle);
    event[4] = status;
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
    socket_connection_send_packet(connection, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void send_gatt_mtu_event(connection_t * connection, hci_con_handle_t con_handle, uint16_t mtu){
    uint8_t event[6];
    int pos = 0;
    event[pos++] = GATT_EVENT_MTU;
    event[pos++] = sizeof(event) - 2;
    little_endian_store_16(event, pos, con_handle);
    pos += 2;
    little_endian_store_16(event, pos, mtu);
    pos += 2;
    hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
    socket_connection_send_packet(connection, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

btstack_linked_list_gatt_client_helper_t * daemon_setup_gatt_client_request(connection_t *connection, uint8_t *packet, int track_active_connection) {
    hci_con_handle_t con_handle = little_endian_read_16(packet, 3);    
    log_info("daemon_setup_gatt_client_request for handle 0x%02x", con_handle);
    hci_connection_t * hci_con = hci_connection_for_handle(con_handle);
    if ((hci_con == NULL) || (hci_con->state != OPEN)){
        send_gatt_query_complete(connection, con_handle, GATT_CLIENT_NOT_CONNECTED);
        return NULL;
    }

    btstack_linked_list_gatt_client_helper_t * helper = daemon_get_gatt_client_helper(con_handle);

    if (!helper){
        log_info("helper does not exist");
        helper = calloc(sizeof(btstack_linked_list_gatt_client_helper_t), 1);
        if (!helper) return NULL; 
        helper->con_handle = con_handle;
        btstack_linked_list_add(&gatt_client_helpers, (btstack_linked_item_t *) helper);
    } 
    
    if (track_active_connection && helper->active_connection){
        send_gatt_query_complete(connection, con_handle, GATT_CLIENT_BUSY);
        return NULL;
    }

    daemon_add_gatt_client_handle(connection, con_handle);
    
    if (track_active_connection){
        // remember connection responsible for this request
        helper->active_connection = connection;
    }

    return helper;
}

// (de)serialize structs from/to HCI commands/events

void daemon_gatt_serialize_service(gatt_client_service_t * service, uint8_t * event, int offset){
    little_endian_store_16(event, offset, service->start_group_handle);
    little_endian_store_16(event, offset+2, service->end_group_handle);
    reverse_128(service->uuid128, &event[offset + 4]);
}

void daemon_gatt_serialize_characteristic(gatt_client_characteristic_t * characteristic, uint8_t * event, int offset){
    little_endian_store_16(event, offset, characteristic->start_handle);
    little_endian_store_16(event, offset+2, characteristic->value_handle);
    little_endian_store_16(event, offset+4, characteristic->end_handle);
    little_endian_store_16(event, offset+6, characteristic->properties);
    reverse_128(characteristic->uuid128, &event[offset+8]);
}

void daemon_gatt_serialize_characteristic_descriptor(gatt_client_characteristic_descriptor_t * characteristic_descriptor, uint8_t * event, int offset){
    little_endian_store_16(event, offset, characteristic_descriptor->handle);
    reverse_128(characteristic_descriptor->uuid128, &event[offset+2]);
}

#endif

static int btstack_command_handler(connection_t *connection, uint8_t *packet, uint16_t size){
    
    bd_addr_t addr;
#ifdef ENABLE_BLE
    bd_addr_type_t addr_type;
    hci_con_handle_t handle;
#endif
    uint16_t cid;
    uint16_t psm;
    uint16_t service_channel;
    uint16_t mtu;
    uint8_t  reason;
    uint8_t  rfcomm_channel;
    uint8_t  rfcomm_credits;
    uint32_t service_record_handle;
    client_state_t *client;
    uint8_t status;
    uint8_t  * data;
#if defined(HAVE_MALLOC) && defined(ENABLE_BLE)
    uint8_t uuid128[16];
    gatt_client_service_t service;
    gatt_client_characteristic_t characteristic;
    gatt_client_characteristic_descriptor_t descriptor;
    uint16_t data_length;
    btstack_linked_list_gatt_client_helper_t * gatt_helper;
#endif

    uint16_t serviceSearchPatternLen;
    uint16_t attributeIDListLen;

    // verbose log info before other info to allow for better tracking
    hci_dump_packet( HCI_COMMAND_DATA_PACKET, 1, packet, size);

    // BTstack internal commands - 16 Bit OpCode, 8 Bit ParamLen, Params...
    switch (READ_CMD_OCF(packet)){
        case BTSTACK_GET_STATE:
            log_info("BTSTACK_GET_STATE");
            hci_emit_state();
            break;
        case BTSTACK_SET_POWER_MODE:
            log_info("BTSTACK_SET_POWER_MODE %u", packet[3]);
            // track client power requests
            client = client_for_connection(connection);
            if (!client) break;
            client->power_mode = packet[3];
            // handle merged state
            if (!clients_require_power_on()){
                start_power_off_timer();
            } else if (!power_management_sleep) {
                stop_power_off_timer();
                hci_power_control(HCI_POWER_ON);
            }
            break;
        case BTSTACK_GET_VERSION:
            log_info("BTSTACK_GET_VERSION");
            hci_emit_btstack_version();
            break;   
#ifdef HAVE_PLATFORM_IPHONE_OS
        case BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED:
            log_info("BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED %u", packet[3]);
            btstack_control_iphone_bt_set_enabled(packet[3]);
            hci_emit_system_bluetooth_enabled(btstack_control_iphone_bt_enabled());
            break;
            
        case BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED:
            log_info("BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED");
            hci_emit_system_bluetooth_enabled(btstack_control_iphone_bt_enabled());
            break;
#else
        case BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED:
        case BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED:
            hci_emit_system_bluetooth_enabled(0);
            break;
#endif
        case BTSTACK_SET_DISCOVERABLE:
            log_info("BTSTACK_SET_DISCOVERABLE discoverable %u)", packet[3]);
            // track client discoverable requests
            client = client_for_connection(connection);
            if (!client) break;
            client->discoverable = packet[3];
            // merge state
            gap_discoverable_control(clients_require_discoverable());
            break;
        case BTSTACK_SET_BLUETOOTH_ENABLED:
            log_info("BTSTACK_SET_BLUETOOTH_ENABLED: %u\n", packet[3]);
            if (packet[3]) {
                // global enable
                global_enable = 1;
                hci_power_control(HCI_POWER_ON);
            } else {
                global_enable = 0;
                clients_clear_power_request();
                hci_power_control(HCI_POWER_OFF);
            }
            break;
        case L2CAP_CREATE_CHANNEL_MTU:
            reverse_bd_addr(&packet[3], addr);
            psm = little_endian_read_16(packet, 9);
            mtu = little_endian_read_16(packet, 11);
            status = l2cap_create_channel(NULL, addr, psm, mtu, &cid);
            if (status){
                send_l2cap_connection_open_failed(connection, addr, psm, status);
            } else {
                daemon_add_client_l2cap_channel(connection, cid);
            }
            break;
        case L2CAP_CREATE_CHANNEL:
            reverse_bd_addr(&packet[3], addr);
            psm = little_endian_read_16(packet, 9);
            mtu = 150; // until r865
            status = l2cap_create_channel(NULL, addr, psm, mtu, &cid);
            if (status){
                send_l2cap_connection_open_failed(connection, addr, psm, status);
            } else {
                daemon_add_client_l2cap_channel(connection, cid);
            }
            break;
        case L2CAP_DISCONNECT:
            cid = little_endian_read_16(packet, 3);
            reason = packet[5];
            l2cap_disconnect(cid, reason);
            break;
        case L2CAP_REGISTER_SERVICE:
            psm = little_endian_read_16(packet, 3);
            mtu = little_endian_read_16(packet, 5);
            status = l2cap_register_service(NULL, psm, mtu, LEVEL_0);
            daemon_add_client_l2cap_service(connection, little_endian_read_16(packet, 3));
            l2cap_emit_service_registered(connection, status, psm);
            break;
        case L2CAP_UNREGISTER_SERVICE:
            psm = little_endian_read_16(packet, 3);
            daemon_remove_client_l2cap_service(connection, psm);
            l2cap_unregister_service(psm);
            break;
        case L2CAP_ACCEPT_CONNECTION:
            cid    = little_endian_read_16(packet, 3);
            l2cap_accept_connection(cid);
            break;
        case L2CAP_DECLINE_CONNECTION:
            cid    = little_endian_read_16(packet, 3);
            reason = packet[7];
            l2cap_decline_connection(cid);
            break;
        case RFCOMM_CREATE_CHANNEL:
            reverse_bd_addr(&packet[3], addr);
            rfcomm_channel = packet[9];
            status = rfcomm_create_channel(&rfcomm_packet_handler, addr, rfcomm_channel, &cid);
            if (status){
                send_rfcomm_create_channel_failed(connection, addr, rfcomm_channel, status);
            } else {
                daemon_add_client_rfcomm_channel(connection, cid);
            }
            break;
        case RFCOMM_CREATE_CHANNEL_WITH_CREDITS:
            reverse_bd_addr(&packet[3], addr);
            rfcomm_channel = packet[9];
            rfcomm_credits = packet[10];
            status = rfcomm_create_channel_with_initial_credits(&rfcomm_packet_handler, addr, rfcomm_channel, rfcomm_credits, &cid );
            if (status){
                send_rfcomm_create_channel_failed(connection, addr, rfcomm_channel, status);
            } else {
                daemon_add_client_rfcomm_channel(connection, cid);
            }
            break;
        case RFCOMM_DISCONNECT:
            cid = little_endian_read_16(packet, 3);
            reason = packet[5];
            rfcomm_disconnect(cid);
            break;
        case RFCOMM_REGISTER_SERVICE:
            rfcomm_channel = packet[3];
            mtu = little_endian_read_16(packet, 4);
            status = rfcomm_register_service(&rfcomm_packet_handler, rfcomm_channel, mtu);
            rfcomm_emit_service_registered(connection, status, rfcomm_channel);
            break;
        case RFCOMM_REGISTER_SERVICE_WITH_CREDITS:
            rfcomm_channel = packet[3];
            mtu = little_endian_read_16(packet, 4);
            rfcomm_credits = packet[6];
            status = rfcomm_register_service_with_initial_credits(&rfcomm_packet_handler, rfcomm_channel, mtu, rfcomm_credits);
            rfcomm_emit_service_registered(connection, status, rfcomm_channel);
            break;
        case RFCOMM_UNREGISTER_SERVICE:
            service_channel = little_endian_read_16(packet, 3);
            daemon_remove_client_rfcomm_service(connection, service_channel);
            rfcomm_unregister_service(service_channel);
            break;
        case RFCOMM_ACCEPT_CONNECTION:
            cid    = little_endian_read_16(packet, 3);
            rfcomm_accept_connection(cid);
            break;
        case RFCOMM_DECLINE_CONNECTION:
            cid    = little_endian_read_16(packet, 3);
            reason = packet[7];
            rfcomm_decline_connection(cid);
            break;            
        case RFCOMM_GRANT_CREDITS:
            cid    = little_endian_read_16(packet, 3);
            rfcomm_credits = packet[5];
            rfcomm_grant_credits(cid, rfcomm_credits);
            break;
        case RFCOMM_PERSISTENT_CHANNEL: {
            // enforce \0
            packet[3+248] = 0;
            rfcomm_channel = rfcomm_service_db_channel_for_service((char*)&packet[3]);
            log_info("DAEMON_EVENT_RFCOMM_PERSISTENT_CHANNEL %u", rfcomm_channel);
            uint8_t event[4];
            event[0] = DAEMON_EVENT_RFCOMM_PERSISTENT_CHANNEL;
            event[1] = sizeof(event) - 2;
            event[2] = 0;
            event[3] = rfcomm_channel;
            hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
            socket_connection_send_packet(connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
            break;
        }
        case SDP_REGISTER_SERVICE_RECORD:
            log_info("SDP_REGISTER_SERVICE_RECORD size %u\n", size);
            service_record_handle = daemon_sdp_create_and_register_service(&packet[3]);
            if (service_record_handle){
                daemon_add_client_sdp_service_record_handle(connection, service_record_handle);
                sdp_emit_service_registered(connection, service_record_handle, 0);
            } else {
               sdp_emit_service_registered(connection, 0, BTSTACK_MEMORY_ALLOC_FAILED);
            }
            break;
        case SDP_UNREGISTER_SERVICE_RECORD:
            service_record_handle = little_endian_read_32(packet, 3);
            log_info("SDP_UNREGISTER_SERVICE_RECORD handle 0x%x ", service_record_handle);
            data = sdp_get_record_for_handle(service_record_handle);
            sdp_unregister_service(service_record_handle);
            daemon_remove_client_sdp_service_record_handle(connection, service_record_handle);
            if (data){
                free(data);
            }
            break;
        case SDP_CLIENT_QUERY_RFCOMM_SERVICES: 
            reverse_bd_addr(&packet[3], addr);

            serviceSearchPatternLen = de_get_len(&packet[9]);
            memcpy(serviceSearchPattern, &packet[9], serviceSearchPatternLen);

            sdp_client_query_connection = connection;
            sdp_client_query_rfcomm_channel_and_name_for_search_pattern(&handle_sdp_rfcomm_service_result, addr, serviceSearchPattern);

            break;
        case SDP_CLIENT_QUERY_SERVICES:
            reverse_bd_addr(&packet[3], addr);
            sdp_client_query_connection = connection;

            serviceSearchPatternLen = de_get_len(&packet[9]);
            memcpy(serviceSearchPattern, &packet[9], serviceSearchPatternLen);
            
            attributeIDListLen = de_get_len(&packet[9+serviceSearchPatternLen]); 
            memcpy(attributeIDList, &packet[9+serviceSearchPatternLen], attributeIDListLen);
            
            sdp_client_query(&handle_sdp_client_query_result, addr, (uint8_t*)&serviceSearchPattern[0], (uint8_t*)&attributeIDList[0]);
            break;
#ifdef ENABLE_BLE
        case GAP_LE_SCAN_START:
            gap_start_scan();
            break;
        case GAP_LE_SCAN_STOP:
            gap_stop_scan();
            break;
        case GAP_LE_SET_SCAN_PARAMETERS:
            gap_set_scan_parameters(packet[3], little_endian_read_16(packet, 4), little_endian_read_16(packet, 6));
            break;
        case GAP_LE_CONNECT:
            reverse_bd_addr(&packet[4], addr);
            addr_type = packet[3];
            gap_connect(addr, addr_type);
            break;
        case GAP_LE_CONNECT_CANCEL:
            gap_connect_cancel();
            break;
        case GAP_DISCONNECT:
            handle = little_endian_read_16(packet, 3);
            gap_disconnect(handle);
            break;
#endif
#if defined(HAVE_MALLOC) && defined(ENABLE_BLE)
        case GATT_DISCOVER_ALL_PRIMARY_SERVICES:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            gatt_client_discover_primary_services(&handle_gatt_client_event, gatt_helper->con_handle);
            break;
        case GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID16:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            gatt_client_discover_primary_services_by_uuid16(&handle_gatt_client_event, gatt_helper->con_handle, little_endian_read_16(packet, 5));
            break;
        case GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID128:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            reverse_128(&packet[5], uuid128);
            gatt_client_discover_primary_services_by_uuid128(&handle_gatt_client_event, gatt_helper->con_handle, uuid128);
            break;
        case GATT_FIND_INCLUDED_SERVICES_FOR_SERVICE:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            gatt_client_deserialize_service(packet, 5, &service);
            gatt_client_find_included_services_for_service(&handle_gatt_client_event, gatt_helper->con_handle, &service);
            break;
        
        case GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            gatt_client_deserialize_service(packet, 5, &service);
            gatt_client_discover_characteristics_for_service(&handle_gatt_client_event, gatt_helper->con_handle, &service);
            break;
        case GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID128:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            gatt_client_deserialize_service(packet, 5, &service);
            reverse_128(&packet[5 + SERVICE_LENGTH], uuid128);
            gatt_client_discover_characteristics_for_service_by_uuid128(&handle_gatt_client_event, gatt_helper->con_handle, &service, uuid128);
            break;
        case GATT_DISCOVER_CHARACTERISTIC_DESCRIPTORS:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            gatt_client_deserialize_characteristic(packet, 5, &characteristic);
            gatt_client_discover_characteristic_descriptors(&handle_gatt_client_event, gatt_helper->con_handle, &characteristic);
            break;
        
        case GATT_READ_VALUE_OF_CHARACTERISTIC:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            gatt_client_deserialize_characteristic(packet, 5, &characteristic);
            gatt_client_read_value_of_characteristic(&handle_gatt_client_event, gatt_helper->con_handle, &characteristic);
            break;
        case GATT_READ_LONG_VALUE_OF_CHARACTERISTIC:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            gatt_client_deserialize_characteristic(packet, 5, &characteristic);
            gatt_client_read_long_value_of_characteristic(&handle_gatt_client_event, gatt_helper->con_handle, &characteristic);
            break;
        
        case GATT_WRITE_VALUE_OF_CHARACTERISTIC_WITHOUT_RESPONSE:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 0);  // note: don't track active connection
            if (!gatt_helper) break;
            gatt_client_deserialize_characteristic(packet, 5, &characteristic);
            data_length = little_endian_read_16(packet, 5 + CHARACTERISTIC_LENGTH);
            data = gatt_helper->characteristic_buffer;
            memcpy(data, &packet[7 + CHARACTERISTIC_LENGTH], data_length);
            gatt_client_write_value_of_characteristic_without_response(gatt_helper->con_handle, characteristic.value_handle, data_length, data);
            break;
        case GATT_WRITE_VALUE_OF_CHARACTERISTIC:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            gatt_client_deserialize_characteristic(packet, 5, &characteristic);
            data_length = little_endian_read_16(packet, 5 + CHARACTERISTIC_LENGTH);
            data = gatt_helper->characteristic_buffer;
            memcpy(data, &packet[7 + CHARACTERISTIC_LENGTH], data_length);
            gatt_client_write_value_of_characteristic(&handle_gatt_client_event, gatt_helper->con_handle, characteristic.value_handle, data_length, data);
            break;
        case GATT_WRITE_LONG_VALUE_OF_CHARACTERISTIC:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            gatt_client_deserialize_characteristic(packet, 5, &characteristic);
            data_length = little_endian_read_16(packet, 5 + CHARACTERISTIC_LENGTH);
            data = gatt_helper->characteristic_buffer;
            memcpy(data, &packet[7 + CHARACTERISTIC_LENGTH], data_length);
            gatt_client_write_long_value_of_characteristic(&handle_gatt_client_event, gatt_helper->con_handle, characteristic.value_handle, data_length, data);
            break;
        case GATT_RELIABLE_WRITE_LONG_VALUE_OF_CHARACTERISTIC:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            gatt_client_deserialize_characteristic(packet, 5, &characteristic);
            data_length = little_endian_read_16(packet, 5 + CHARACTERISTIC_LENGTH);
            data = gatt_helper->characteristic_buffer;
            memcpy(data, &packet[7 + CHARACTERISTIC_LENGTH], data_length);
            gatt_client_write_long_value_of_characteristic(&handle_gatt_client_event, gatt_helper->con_handle, characteristic.value_handle, data_length, data);
            break;
        case GATT_READ_CHARACTERISTIC_DESCRIPTOR:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            handle = little_endian_read_16(packet, 3);
            gatt_client_deserialize_characteristic_descriptor(packet, 5, &descriptor);
            gatt_client_read_characteristic_descriptor(&handle_gatt_client_event, gatt_helper->con_handle, &descriptor);
            break;
        case GATT_READ_LONG_CHARACTERISTIC_DESCRIPTOR:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            gatt_client_deserialize_characteristic_descriptor(packet, 5, &descriptor);
            gatt_client_read_long_characteristic_descriptor(&handle_gatt_client_event, gatt_helper->con_handle, &descriptor);
            break;
            
        case GATT_WRITE_CHARACTERISTIC_DESCRIPTOR:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            gatt_client_deserialize_characteristic_descriptor(packet, 5, &descriptor);
            data = gatt_helper->characteristic_buffer;
            data_length = little_endian_read_16(packet, 5 + CHARACTERISTIC_DESCRIPTOR_LENGTH);
            gatt_client_write_characteristic_descriptor(&handle_gatt_client_event, gatt_helper->con_handle, &descriptor, data_length, data);
            break;
        case GATT_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR:
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            gatt_client_deserialize_characteristic_descriptor(packet, 5, &descriptor);
            data = gatt_helper->characteristic_buffer;
            data_length = little_endian_read_16(packet, 5 + CHARACTERISTIC_DESCRIPTOR_LENGTH);
            gatt_client_write_long_characteristic_descriptor(&handle_gatt_client_event, gatt_helper->con_handle, &descriptor, data_length, data);
            break;
        case GATT_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION:{
            uint16_t configuration = little_endian_read_16(packet, 5 + CHARACTERISTIC_LENGTH);
            gatt_helper = daemon_setup_gatt_client_request(connection, packet, 1);
            if (!gatt_helper) break;
            data = gatt_helper->characteristic_buffer;
            gatt_client_deserialize_characteristic(packet, 5, &characteristic);
            gatt_client_write_client_characteristic_configuration(&handle_gatt_client_event, gatt_helper->con_handle, &characteristic, configuration);
            break;
        case GATT_GET_MTU:
            handle = little_endian_read_16(packet, 3);
            gatt_client_get_mtu(handle, &mtu);
            send_gatt_mtu_event(connection, handle, mtu);
            break;
        }
#endif
    default:
            log_error("Error: command %u not implemented:", READ_CMD_OCF(packet));
            break;
    }
    
    return 0;
}

static int daemon_client_handler(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t length){
    
    int err = 0;
    client_state_t * client;
    
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            if (READ_CMD_OGF(data) != OGF_BTSTACK) { 
                // HCI Command
                hci_send_cmd_packet(data, length);
            } else {
                // BTstack command
                btstack_command_handler(connection, data, length);
            }
            break;
        case L2CAP_DATA_PACKET:
            // process l2cap packet...
            err = l2cap_send(channel, data, length);
            break;
        case RFCOMM_DATA_PACKET:
            // process l2cap packet...
            err = rfcomm_send(channel, data, length);
            break;
        case DAEMON_EVENT_PACKET:
            switch (data[0]) {
                case DAEMON_EVENT_CONNECTION_OPENED:
                    log_info("DAEMON_EVENT_CONNECTION_OPENED %p\n",connection);

                    client = calloc(sizeof(client_state_t), 1);
                    if (!client) break; // fail
                    client->connection   = connection;
                    client->power_mode   = HCI_POWER_OFF;
                    client->discoverable = 0;
                    btstack_linked_list_add(&clients, (btstack_linked_item_t *) client);
                    break;
                case DAEMON_EVENT_CONNECTION_CLOSED:
                    log_info("DAEMON_EVENT_CONNECTION_CLOSED %p\n",connection);
                    daemon_disconnect_client(connection);
                    // no clients -> no HCI connections
                    if (!clients){
                        hci_disconnect_all();
                    }

                    // update discoverable mode
                    gap_discoverable_control(clients_require_discoverable());
                    // start power off, if last active client
                    if (!clients_require_power_on()){
                        start_power_off_timer();
                    }
                    break;
                default:
                    break;
            }
            break;
    }
    if (err) {
        log_info("Daemon Handler: err %d\n", err);
    }
    return err;
}


static void daemon_set_logging_enabled(int enabled){
    if (enabled && !loggingEnabled){
        hci_dump_open(BTSTACK_LOG_FILE, BTSTACK_LOG_TYPE);
    }
    if (!enabled && loggingEnabled){
        hci_dump_close();
    }
    loggingEnabled = enabled;
}

// local cache used to manage UI status
static HCI_STATE hci_state = HCI_STATE_OFF;
static int num_connections = 0;
static void update_ui_status(void){
    if (hci_state != HCI_STATE_WORKING) {
        bluetooth_status_handler(BLUETOOTH_OFF);
    } else {
        if (num_connections) {
            bluetooth_status_handler(BLUETOOTH_ACTIVE);
        } else {
            bluetooth_status_handler(BLUETOOTH_ON);
        }
    }
}

#ifdef USE_SPRINGBOARD
static void preferences_changed_callback(void){
    int logging = platform_iphone_logging_enabled();
    log_info("Logging enabled: %u\n", logging);
    daemon_set_logging_enabled(logging);
}
#endif

static void deamon_status_event_handler(uint8_t *packet, uint16_t size){
    
    uint8_t update_status = 0;
    
    // handle state event
    switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            hci_state = packet[2];
            log_info("New state: %u\n", hci_state);
            update_status = 1;
            break;
        case BTSTACK_EVENT_NR_CONNECTIONS_CHANGED:
            num_connections = packet[2];
            log_info("New nr connections: %u\n", num_connections);
            update_status = 1;
            break;
        default:
            break;
    }
    
    // choose full bluetooth state 
    if (update_status) {
        update_ui_status();
    }
}

static void daemon_retry_parked(void){
    
    // socket_connection_retry_parked is not reentrant
    static int retry_mutex = 0;

    // lock mutex
    if (retry_mutex) return;
    retry_mutex = 1;
    
    // ... try sending again  
    socket_connection_retry_parked();

    // unlock mutex
    retry_mutex = 0;
}

#if 0

Minimal Code for LE Peripheral

enum {
    SET_ADVERTISEMENT_PARAMS = 1 << 0,
    SET_ADVERTISEMENT_DATA   = 1 << 1,
    ENABLE_ADVERTISEMENTS    = 1 << 2,
};

const uint8_t adv_data[] = {
    // Flags general discoverable
    0x02, 0x01, 0x02, 
    // Name
    0x08, 0x09, 'B', 'T', 's', 't', 'a', 'c', 'k' 
};
uint8_t adv_data_len = sizeof(adv_data);
static uint16_t todos = 0;

static void app_run(void){

    if (!hci_can_send_command_packet_now()) return;

    if (todos & SET_ADVERTISEMENT_DATA){
        log_info("app_run: set advertisement data\n");
        todos &= ~SET_ADVERTISEMENT_DATA;
        hci_send_cmd(&hci_le_set_advertising_data, adv_data_len, adv_data);
        return;
    }    

    if (todos & SET_ADVERTISEMENT_PARAMS){
        todos &= ~SET_ADVERTISEMENT_PARAMS;
        uint8_t adv_type = 0;   // default
        bd_addr_t null_addr;
        memset(null_addr, 0, 6);
        uint16_t adv_int_min = 0x0030;
        uint16_t adv_int_max = 0x0030;
        hci_send_cmd(&hci_le_set_advertising_parameters, adv_int_min, adv_int_max, adv_type, 0, 0, &null_addr, 0x07, 0x00);
        return;
    }    

    if (todos & ENABLE_ADVERTISEMENTS){
        log_info("app_run: enable advertisements\n");
        todos &= ~ENABLE_ADVERTISEMENTS;
        hci_send_cmd(&hci_le_set_advertise_enable, 1);
        return;
    }
}
#endif 

static void daemon_emit_packet(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    if (connection) {
        socket_connection_send_packet(connection, packet_type, channel, packet, size);
    } else {
        socket_connection_send_packet_all(packet_type, channel, packet, size);
    }
}

static uint8_t remote_name_event[2+1+6+DEVICE_NAME_LEN+1]; // +1 for \0 in log_info
static void daemon_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    uint16_t cid;
    int i;
    bd_addr_t addr;
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            deamon_status_event_handler(packet, size);
            switch (hci_event_packet_get_type(packet)){

                case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS:
                    // ACL buffer freed...
                    daemon_retry_parked();
                    // no need to tell clients
                    return;

                case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
                    if (!btstack_device_name_db) break;
                    if (packet[2]) break; // status not ok

                    reverse_bd_addr(&packet[3], addr);
                    // fix for invalid remote names - terminate on 0xff
                    for (i=0; i<248;i++){
                        if (packet[9+i] == 0xff){
                            packet[9+i] = 0;
                            break;
                        }
                    }
                    packet[9+248] = 0;
                    btstack_device_name_db->put_name(addr, (device_name_t *)&packet[9]);
                    break;
                
                case HCI_EVENT_INQUIRY_RESULT:
                case HCI_EVENT_INQUIRY_RESULT_WITH_RSSI:{
                    if (!btstack_device_name_db) break;
                    
                    // first send inq result packet
                    daemon_emit_packet(connection, packet_type, channel, packet, size);

                    // then send cached remote names
                    int offset = 3;
                    for (i=0; i<packet[2];i++){
                        reverse_bd_addr(&packet[offset], addr);
                        if (btstack_device_name_db->get_name(addr, (device_name_t *) &remote_name_event[9])){
                            remote_name_event[0] = DAEMON_EVENT_REMOTE_NAME_CACHED;
                            remote_name_event[1] = sizeof(remote_name_event) - 2 - 1;
                            remote_name_event[2] = 0;   // just to be compatible with HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE
                            reverse_bd_addr(addr, &remote_name_event[3]);
                            
                            remote_name_event[9+248] = 0;   // assert \0 for log_info
                            log_info("DAEMON_EVENT_REMOTE_NAME_CACHED %s = '%s'", bd_addr_to_str(addr), &remote_name_event[9]);
                            hci_dump_packet(HCI_EVENT_PACKET, 0, remote_name_event, sizeof(remote_name_event)-1);
                            daemon_emit_packet(connection, HCI_EVENT_PACKET, channel, remote_name_event, sizeof(remote_name_event) -1);
                        }
                        offset += 14; // 6 + 1 + 1 + 1 + 3 + 2; 
                    }
                    return;
                }
                
                case DAEMON_EVENT_RFCOMM_CREDITS:
                    // RFCOMM CREDITS received...
                    daemon_retry_parked();
                    break;
                
                case RFCOMM_EVENT_CHANNEL_OPENED:
                    cid = little_endian_read_16(packet, 13);
                    connection = connection_for_rfcomm_cid(cid);
                    if (!connection) break;
                    if (packet[2]) {
                        daemon_remove_client_rfcomm_channel(connection, cid);
                    } else {
                        daemon_add_client_rfcomm_channel(connection, cid);
                    }
                    break;
                case RFCOMM_EVENT_CHANNEL_CLOSED:
                    cid = little_endian_read_16(packet, 2);
                    connection = connection_for_rfcomm_cid(cid);
                    if (!connection) break;
                    daemon_remove_client_rfcomm_channel(connection, cid);
                    break;
                case DAEMON_EVENT_RFCOMM_SERVICE_REGISTERED:
                    if (packet[2]) break;
                    daemon_add_client_rfcomm_service(connection, packet[3]);
                    break;
                case L2CAP_EVENT_CHANNEL_OPENED:
                    cid = little_endian_read_16(packet, 13);
                    connection = connection_for_l2cap_cid(cid);
                    if (!connection) break;
                    if (packet[2]) {
                        daemon_remove_client_l2cap_channel(connection, cid);
                    } else {
                        daemon_add_client_l2cap_channel(connection, cid);
                    }
                    break;
                case L2CAP_EVENT_CHANNEL_CLOSED:
                    cid = little_endian_read_16(packet, 2);
                    connection = connection_for_l2cap_cid(cid);
                    if (!connection) break;
                    daemon_remove_client_l2cap_channel(connection, cid);
                    break;
#if defined(ENABLE_BLE) && defined(HAVE_MALLOC)
                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    log_info("daemon : ignore HCI_EVENT_DISCONNECTION_COMPLETE ingnoring.");
                    // note: moved to gatt_client_handler because it's received here prematurely
                    // daemon_remove_gatt_client_helper(little_endian_read_16(packet, 3));
                    break;
#endif
                default:
                    break;
            }
            break;
        case L2CAP_DATA_PACKET:
            connection = connection_for_l2cap_cid(channel);
            if (!connection) return;
            break;
        case RFCOMM_DATA_PACKET:        
            connection = connection_for_l2cap_cid(channel);
            if (!connection) return;
            break;
        default:
            break;
    }
    
    daemon_emit_packet(connection, packet_type, channel, packet, size);
}

static void l2cap_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size){
    daemon_packet_handler(NULL, packet_type, channel, packet, size);
}
static void rfcomm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size){
    daemon_packet_handler(NULL, packet_type, channel, packet, size);
}

static void handle_sdp_rfcomm_service_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_RFCOMM_SERVICE:
        case SDP_EVENT_QUERY_COMPLETE:
            // already HCI Events, just forward them
            hci_dump_packet(HCI_EVENT_PACKET, 0, packet, size);
            socket_connection_send_packet(sdp_client_query_connection, HCI_EVENT_PACKET, 0, packet, size);
            break;
        default: 
            break;
    }
}

static void sdp_client_assert_buffer(int size){
    if (size > attribute_value_buffer_size){
        log_error("SDP attribute value buffer size exceeded: available %d, required %d", attribute_value_buffer_size, size);
    }
}

// define new packet type SDP_CLIENT_PACKET
static void handle_sdp_client_query_result(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    int event_len;

    switch (hci_event_packet_get_type(packet)){
        case SDP_EVENT_QUERY_ATTRIBUTE_BYTE:
            sdp_client_assert_buffer(sdp_event_query_attribute_byte_get_attribute_length(packet));
            attribute_value[sdp_event_query_attribute_byte_get_data_offset(packet)] = sdp_event_query_attribute_byte_get_data(packet);
            if ((uint16_t)(sdp_event_query_attribute_byte_get_data_offset(packet)+1) == sdp_event_query_attribute_byte_get_attribute_length(packet)){
                log_info_hexdump(attribute_value, sdp_event_query_attribute_byte_get_attribute_length(packet));

                int event_len = 1 + 3 * 2 + sdp_event_query_attribute_byte_get_attribute_length(packet); 
                uint8_t event[event_len];
                event[0] = SDP_EVENT_QUERY_ATTRIBUTE_VALUE;
                little_endian_store_16(event, 1, sdp_event_query_attribute_byte_get_record_id(packet));
                little_endian_store_16(event, 3, sdp_event_query_attribute_byte_get_attribute_id(packet));
                little_endian_store_16(event, 5, (uint16_t)sdp_event_query_attribute_byte_get_attribute_length(packet));
                memcpy(&event[7], attribute_value, sdp_event_query_attribute_byte_get_attribute_length(packet));
                hci_dump_packet(SDP_CLIENT_PACKET, 0, event, event_len);
                socket_connection_send_packet(sdp_client_query_connection, SDP_CLIENT_PACKET, 0, event, event_len);
            }
            break;
        case SDP_EVENT_QUERY_COMPLETE:
            event_len = packet[1] + 2;
            hci_dump_packet(HCI_EVENT_PACKET, 0, packet, event_len);
            socket_connection_send_packet(sdp_client_query_connection, HCI_EVENT_PACKET, 0, packet, event_len);
            break;
    }
}

static void power_notification_callback(POWER_NOTIFICATION_t notification){
    switch (notification) {
        case POWER_WILL_SLEEP:
            // let's sleep
            power_management_sleep = 1;
            hci_power_control(HCI_POWER_SLEEP);
            break;
        case POWER_WILL_WAKE_UP:
            // assume that all clients use Bluetooth -> if connection, start Bluetooth
            power_management_sleep = 0;
            if (clients_require_power_on()) {
                hci_power_control(HCI_POWER_ON);
            }
            break;
        default:
            break;
    }
}

static void daemon_sigint_handler(int param){
    
#ifdef HAVE_PLATFORM_IPHONE_OS
    // notify daemons
    notify_post("ch.ringwald.btstack.stopped");
#endif
    
    log_info(" <= SIGINT received, shutting down..\n");    

    hci_power_control( HCI_POWER_OFF);
    hci_close();
    
    log_info("Good bye, see you.\n");    
    
    exit(0);
}

// MARK: manage power off timer

#define USE_POWER_OFF_TIMER

static void stop_power_off_timer(void){
#ifdef USE_POWER_OFF_TIMER
    if (timeout_active) {
        btstack_run_loop_remove_timer(&timeout);
        timeout_active = 0;
    }
#endif
}

static void start_power_off_timer(void){
#ifdef USE_POWER_OFF_TIMER    
    stop_power_off_timer();
    btstack_run_loop_set_timer(&timeout, DAEMON_NO_ACTIVE_CLIENT_TIMEOUT);
    btstack_run_loop_add_timer(&timeout);
    timeout_active = 1;
#else
    hci_power_control(HCI_POWER_OFF);
#endif
}

// MARK: manage list of clients


static client_state_t * client_for_connection(connection_t *connection) {
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) clients; it ; it = it->next){
        client_state_t * client_state = (client_state_t *) it;
        if (client_state->connection == connection) {
            return client_state;
        }
    }
    return NULL;
}

static void clients_clear_power_request(void){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) clients; it ; it = it->next){
        client_state_t * client_state = (client_state_t *) it;
        client_state->power_mode = HCI_POWER_OFF;
    }
}

static int clients_require_power_on(void){
    
    if (global_enable) return 1;
    
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) clients; it ; it = it->next){
        client_state_t * client_state = (client_state_t *) it;
        if (client_state->power_mode == HCI_POWER_ON) {
            return 1;
        }
    }
    return 0;
}

static int clients_require_discoverable(void){
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) clients; it ; it = it->next){
        client_state_t * client_state = (client_state_t *) it;
        if (client_state->discoverable) {
            return 1;
        }
    }
    return 0;
}

static void usage(const char * name) {
    printf("%s, BTstack background daemon\n", name);
    printf("usage: %s [--help] [--tcp port]\n", name);
    printf("    --help   display this usage\n");
    printf("    --tcp    use TCP server on port %u\n", BTSTACK_PORT);
    printf("Without the --tcp option, BTstack daemon is listening on unix domain socket %s\n\n", BTSTACK_UNIX);
}

#ifdef HAVE_PLATFORM_IPHONE_OS 
static void * btstack_run_loop_thread(void *context){
    btstack_run_loop_execute();
    return NULL;
}
#endif

#ifdef ENABLE_BLE

static void handle_gatt_client_event(uint8_t packet_type, uint16_t channel, uint8_t * packet, uint16_t size){

    // hack: handle disconnection_complete_here instead of main hci event packet handler
    // we receive a HCI event packet in disguise
    if (hci_event_packet_get_type(packet) == HCI_EVENT_DISCONNECTION_COMPLETE){
        log_info("daemon hack: handle disconnection_complete in handle_gatt_client_event instead of main hci event packet handler");
        hci_con_handle_t con_handle = little_endian_read_16(packet, 3);
        daemon_remove_gatt_client_helper(con_handle);
        return;
    }

    // only handle GATT Events
    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_SERVICE_QUERY_RESULT:
        case GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT:
        case GATT_EVENT_NOTIFICATION:
        case GATT_EVENT_INDICATION:
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
        case GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
        case GATT_EVENT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
        case GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
        case GATT_EVENT_QUERY_COMPLETE:
           break;
        default:
            return;
    }

    hci_con_handle_t con_handle = little_endian_read_16(packet, 2);
    btstack_linked_list_gatt_client_helper_t * gatt_client_helper = daemon_get_gatt_client_helper(con_handle);
    if (!gatt_client_helper){
        log_info("daemon handle_gatt_client_event: gc helper for handle 0x%2x is NULL.", con_handle);
        return;
    } 

    connection_t *connection = NULL;

    // daemon doesn't track which connection subscribed to this particular handle, so we just notify all connections
    switch(hci_event_packet_get_type(packet)){
        case GATT_EVENT_NOTIFICATION:
        case GATT_EVENT_INDICATION:{
            hci_dump_packet(HCI_EVENT_PACKET, 0, packet, size);
            
            btstack_linked_item_t *it;
            for (it = (btstack_linked_item_t *) clients; it ; it = it->next){
                client_state_t * client_state = (client_state_t *) it;
                socket_connection_send_packet(client_state->connection, HCI_EVENT_PACKET, 0, packet, size);
            }
            return;
        }
        default:
            break;
    }

    // otherwise, we have to have an active connection
    connection = gatt_client_helper->active_connection;
    uint16_t offset;
    uint16_t length;

    if (!connection) return;

    switch(hci_event_packet_get_type(packet)){

        case GATT_EVENT_SERVICE_QUERY_RESULT:
        case GATT_EVENT_INCLUDED_SERVICE_QUERY_RESULT:
        case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT:
        case GATT_EVENT_CHARACTERISTIC_VALUE_QUERY_RESULT:
        case GATT_EVENT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
        case GATT_EVENT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
            hci_dump_packet(HCI_EVENT_PACKET, 0, packet, size);
            socket_connection_send_packet(connection, HCI_EVENT_PACKET, 0, packet, size);
            break;
            
        case GATT_EVENT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
        case GATT_EVENT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
            offset = little_endian_read_16(packet, 6);
            length = little_endian_read_16(packet, 8);
            gatt_client_helper->characteristic_buffer[0] = hci_event_packet_get_type(packet);  // store type (characteristic/descriptor)
            gatt_client_helper->characteristic_handle    = little_endian_read_16(packet, 4);   // store attribute handle
            gatt_client_helper->characteristic_length = offset + length;            // update length
            memcpy(&gatt_client_helper->characteristic_buffer[10 + offset], &packet[10], length);
            break;

        case GATT_EVENT_QUERY_COMPLETE:{
            gatt_client_helper->active_connection = NULL;
            if (gatt_client_helper->characteristic_length){
                // send re-combined long characteristic value or long characteristic descriptor value
                uint8_t * event = gatt_client_helper->characteristic_buffer;
                uint16_t event_size = 10 + gatt_client_helper->characteristic_length;
                // event[0] == already set by previsous case
                event[1] = 8 + gatt_client_helper->characteristic_length;
                little_endian_store_16(event, 2, little_endian_read_16(packet, 2));
                little_endian_store_16(event, 4, gatt_client_helper->characteristic_handle);
                little_endian_store_16(event, 6, 0);   // offset
                little_endian_store_16(event, 8, gatt_client_helper->characteristic_length);
                hci_dump_packet(HCI_EVENT_PACKET, 0, event, event_size);
                socket_connection_send_packet(connection, HCI_EVENT_PACKET, 0, event, event_size);
                gatt_client_helper->characteristic_length = 0;
            }
            hci_dump_packet(HCI_EVENT_PACKET, 0, packet, size);
            socket_connection_send_packet(connection, HCI_EVENT_PACKET, 0, packet, size);
            break;
        }
        default:
            break;
    }
}
#endif

static char hostname[30];

int main (int argc,  char * const * argv){
    
    static int tcp_flag = 0;
    
    while (1) {
        static struct option long_options[] = {
            { "tcp", no_argument, &tcp_flag, 1 },
            { "help", no_argument, 0, 0 },
            { 0,0,0,0 } // This is a filler for -1
        };
        
        int c;
        int option_index = -1;
        
        c = getopt_long(argc, argv, "h", long_options, &option_index);
        
        if (c == -1) break; // no more option
        
        // treat long parameter first
        if (option_index == -1) {
            switch (c) {
                case '?':
                case 'h':
                    usage(argv[0]);
                    return 0;
                    break;
            }
        } else {
            switch (option_index) {
                case 1:
                    usage(argv[0]);
                    return 0;
                    break;
            }
        }
    }
    
    if (tcp_flag){
        printf("BTstack Daemon started on port %u\n", BTSTACK_PORT);
    } else {
        printf("BTstack Daemon started on socket %s\n", BTSTACK_UNIX);
    }

    // make stdout unbuffered
    setbuf(stdout, NULL);

    // handle CTRL-c
    signal(SIGINT, daemon_sigint_handler);
    // handle SIGTERM - suggested for launchd
    signal(SIGTERM, daemon_sigint_handler);

    socket_connection_init();

    btstack_control_t * control = NULL;
    void * config;
    const btstack_uart_block_t * uart_block_implementation = NULL;
    (void) uart_block_implementation;
    
#ifdef HAVE_TRANSPORT_H4
    hci_transport_config_uart.type = HCI_TRANSPORT_CONFIG_UART;
    hci_transport_config_uart.baudrate_init = UART_SPEED;
    hci_transport_config_uart.baudrate_main = 0;
    hci_transport_config_uart.flowcontrol = 1;
    hci_transport_config_uart.device_name   = UART_DEVICE;

#ifndef HAVE_PLATFORM_IPHONE_OS
    uart_block_implementation = btstack_uart_block_posix_instance();
#endif

#ifdef HAVE_PLATFORM_IPHONE_OS
    // use default (max) UART baudrate over netgraph interface
    hci_transport_config_uart.baudrate_init = 0;
#endif

    config = &hci_transport_config_uart;
    transport = hci_transport_h4_instance(uart_block_implementation);
#endif

#ifdef HAVE_TRANSPORT_USB
    transport = hci_transport_usb_instance();
#endif

#ifdef HAVE_PLATFORM_IPHONE_OS
    control = &btstack_control_iphone;
    if (btstack_control_iphone_power_management_supported()){
        hci_transport_h4_iphone_set_enforce_wake_device("/dev/btwake");
    }
    bluetooth_status_handler = platform_iphone_status_handler;
    platform_iphone_register_window_manager_restart(update_ui_status);
    platform_iphone_register_preferences_changed(preferences_changed_callback);
#endif
    
#ifdef BTSTACK_LINK_KEY_DB_INSTANCE
    btstack_link_key_db = BTSTACK_LINK_KEY_DB_INSTANCE();
#endif

#ifdef BTSTACK_DEVICE_NAME_DB_INSTANCE
    btstack_device_name_db = BTSTACK_DEVICE_NAME_DB_INSTANCE();
#endif

    btstack_run_loop_init(btstack_run_loop_posix_get_instance());
    
    // init power management notifications
    if (control && control->register_for_power_notifications){
        control->register_for_power_notifications(power_notification_callback);
    }

    // logging
    loggingEnabled = 0;
    int newLoggingEnabled = 1;
#ifdef HAVE_PLATFORM_IPHONE_OS
    // iPhone has toggle in Preferences.app
    newLoggingEnabled = platform_iphone_logging_enabled();
#endif
    daemon_set_logging_enabled(newLoggingEnabled);
    
    // dump version
    log_info("BTdaemon started\n");
    log_info("version %s, build %s", BTSTACK_VERSION, BTSTACK_DATE);

    // init HCI
    hci_init(transport, config);
    if (btstack_link_key_db){
        hci_set_link_key_db(btstack_link_key_db);
    }
    if (control){
        hci_set_control(control);
    }

    // hostname for POSIX systems
    gethostname(hostname, 30);
    hostname[29] = '\0';
    gap_set_local_name(hostname);

#ifdef HAVE_PLATFORM_IPHONE_OS
    // iPhone doesn't use SSP yet as there's no UI for it yet and auto accept is not an option
    gap_ssp_set_enable(0);
#endif

    // register for HCI events
    hci_event_callback_registration.callback = &l2cap_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // init L2CAP
    l2cap_init();
    l2cap_register_packet_handler(&l2cap_packet_handler);
    timeout.process = daemon_no_connections_timeout;

#ifdef ENABLE_RFCOMM
    log_info("config.h: ENABLE_RFCOMM\n");
    rfcomm_init();
#endif
    
#ifdef ENABLE_SDP
    sdp_init();
#endif

#ifdef ENABLE_BLE
    // GATT Client
    gatt_client_init();

    // sm_init();
    // sm_set_io_capabilities(IO_CAPABILITY_DISPLAY_ONLY);
    // sm_set_authentication_requirements( SM_AUTHREQ_BONDING | SM_AUTHREQ_MITM_PROTECTION); 

    // GATT Server - empty attribute database
    le_device_db_init();
    att_server_init(NULL, NULL, NULL);    

#endif
    
#ifdef USE_LAUNCHD
    socket_connection_create_launchd();
#else
    // create server
    if (tcp_flag) {
        socket_connection_create_tcp(BTSTACK_PORT);
    } else {
        socket_connection_create_unix(BTSTACK_UNIX);
    }
#endif
    socket_connection_register_packet_callback(&daemon_client_handler);
        
#ifdef HAVE_PLATFORM_IPHONE_OS 
    // notify daemons
    notify_post("ch.ringwald.btstack.started");

    // spawn thread to have BTstack run loop on new thread, while main thread is used to keep CFRunLoop
    pthread_t run_loop;
    pthread_create(&run_loop, NULL, &btstack_run_loop_thread, NULL);

    // needed to receive notifications
    CFRunLoopRun();
#endif
        // go!
    btstack_run_loop_execute();
    return 0;
}
