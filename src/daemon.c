/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
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
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 * Please inquire about commercial licensing options at btstack@ringwald.ch
 *
 */

/*
 *  daemon.c
 *
 *  Created by Matthias Ringwald on 7/1/09.
 *
 *  BTstack background daemon
 *
 */

#include "btstack-config.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

#include <getopt.h>

#include <btstack/btstack.h>
#include <btstack/linked_list.h>
#include <btstack/run_loop.h>
#include <btstack/hci_cmds.h>

#include "debug.h"
#include "hci.h"
#include "hci_dump.h"
#include "hci_transport.h"
#include "l2cap.h"
#include "rfcomm.h"
#include "sdp.h"
#include "sdp_parser.h"
#include "sdp_client.h"
#include "sdp_query_util.h"
#include "sdp_query_rfcomm.h"
#include "socket_connection.h"
#include "gatt_client.h"

#ifdef USE_BLUETOOL
#include <CoreFoundation/CoreFoundation.h>
#include "bt_control_iphone.h"
#include <notify.h>
#endif

#ifdef USE_SPRINGBOARD
#include "platform_iphone.h"
#endif

#ifdef HAVE_TRANSPORT_USB
#include <libusb-1.0/libusb.h>
#endif

#ifndef BTSTACK_LOG_FILE
#define BTSTACK_LOG_FILE "/tmp/hci_dump.pklg"
#endif

// use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
#ifndef BTSTACK_LOG_TYPE
#define BTSTACK_LOG_TYPE HCI_DUMP_PACKETLOGGER 
#endif

#define DAEMON_NO_ACTIVE_CLIENT_TIMEOUT 10000


typedef struct {
    // linked list - assert: first field
    linked_item_t    item;
    
    // connection
    connection_t * connection;
    
    // power mode
    HCI_POWER_MODE power_mode;
    
    // discoverable
    uint8_t        discoverable;
    
} client_state_t;

// MARK: prototypes
static void handle_sdp_rfcomm_service_result(sdp_query_event_t * event, void * context);
static void handle_sdp_client_query_result(sdp_query_event_t * event);
static void dummy_bluetooth_status_handler(BLUETOOTH_STATE state);
static client_state_t * client_for_connection(connection_t *connection);
static int              clients_require_power_on(void);
static int              clients_require_discoverable(void);
static void              clients_clear_power_request(void);
static void start_power_off_timer(void);
static void stop_power_off_timer(void);

// MARK: globals
static hci_transport_t * transport;
static hci_uart_config_t config;
static timer_source_t timeout;
static uint8_t timeout_active = 0;
static int power_management_sleep = 0;
static linked_list_t    clients = NULL; // list of connected clients 
static void (*bluetooth_status_handler)(BLUETOOTH_STATE state) = dummy_bluetooth_status_handler;

static int global_enable = 0;

static remote_device_db_t const * remote_device_db = NULL;
static int rfcomm_channel_generator = 1;

static uint8_t   attribute_value[1000];
static const int attribute_value_buffer_size = sizeof(attribute_value);
static uint8_t serviceSearchPattern[200];
static uint8_t attributeIDList[50];
    

static int loggingEnabled;

static void dummy_bluetooth_status_handler(BLUETOOTH_STATE state){
    log_info("Bluetooth status: %u\n", state);
};

static void daemon_no_connections_timeout(struct timer *ts){
    if (clients_require_power_on()) return;    // false alarm :)
    log_info("No active client connection for %u seconds -> POWER OFF\n", DAEMON_NO_ACTIVE_CLIENT_TIMEOUT/1000);
    hci_power_control(HCI_POWER_OFF);
}

#define ATT_MAX_LONG_ATTRIBUTE_SIZE 512


#ifdef HAVE_BLE
#define SERVICE_LENGTH                      20
#define CHARACTERISTIC_LENGTH               24
#define CHARACTERISTIC_DESCRIPTOR_LENGTH    18

// ATT_MTU - 1
#define ATT_MAX_ATTRIBUTE_SIZE 22

typedef struct gatt_client_helper {
    uint16_t characteristic_length;
} gatt_client_helper_t;

static gatt_client_t * daemon_provide_gatt_client_context_for_handle(uint16_t handle){
    gatt_client_t *context;
    context = get_gatt_client_context_for_handle(handle);
    if (context) return context;

    context = (gatt_client_t*)malloc(sizeof(gatt_client_t) + sizeof(gatt_client_helper_t) + ATT_MAX_LONG_ATTRIBUTE_SIZE);
    if (!context) return NULL;

    gatt_client_start(context, handle);
    return context;
}

gatt_client_helper_t * daemon_get_gatt_helper(gatt_client_t * context){
    return (gatt_client_helper_t*) (((uint8_t *) context) + sizeof(gatt_client_t));
}

uint8_t * daemon_get_data_buffer(gatt_client_t *context) {
    return ((uint8_t *) context) + sizeof(gatt_client_t) + sizeof(gatt_client_helper_t);
}

static void send_gatt_query_complete(connection_t * connection, uint16_t handle, uint8_t status){
    // @format H1
    uint8_t event[5];
    event[0] = GATT_QUERY_COMPLETE;
    event[1] = 3;
    bt_store_16(event, 2, handle);
    event[4] = status;
    socket_connection_send_packet(connection, HCI_EVENT_PACKET, 0, event, sizeof(event));
}

gatt_client_t * daemon_prepare_gatt_client_context(connection_t *connection, uint8_t *packet) {
    hci_con_handle_t handle = READ_BT_16(packet, 3);
    
    gatt_client_t *context = daemon_provide_gatt_client_context_for_handle(handle);
    if (!context) {
        send_gatt_query_complete(connection, handle, BTSTACK_MEMORY_ALLOC_FAILED);
        return NULL;
    }
    // check state
    if (!gatt_client_is_ready(context)){
        send_gatt_query_complete(connection, handle, GATT_CLIENT_BUSY);
        return NULL;
    }
    context->context = connection;
    return context;
}


// (de)serialize structs from/to HCI commands/events

void daemon_gatt_deserialize_service(uint8_t *packet, int offset, le_service_t *service){
    service->start_group_handle = READ_BT_16(packet, offset);
    service->end_group_handle = READ_BT_16(packet, offset + 2);
    swap128(&packet[offset + 4], service->uuid128);
}

void daemon_gatt_serialize_service(le_service_t * service, uint8_t * event, int offset){
    bt_store_16(event, offset, service->start_group_handle);
    bt_store_16(event, offset+2, service->end_group_handle);
    swap128(service->uuid128, &event[offset + 4]);
}

void daemon_gatt_deserialize_characteristic(uint8_t * packet, int offset, le_characteristic_t * characteristic){
    characteristic->start_handle = READ_BT_16(packet, offset);
    characteristic->value_handle = READ_BT_16(packet, offset + 2);
    characteristic->end_handle = READ_BT_16(packet, offset + 4);
    characteristic->properties = READ_BT_16(packet, offset + 6);
    characteristic->uuid16 = READ_BT_16(packet, offset + 8);
    swap128(&packet[offset+10], characteristic->uuid128);
}

void daemon_gatt_serialize_characteristic(le_characteristic_t * characteristic, uint8_t * event, int offset){
    bt_store_16(event, offset, characteristic->start_handle);
    bt_store_16(event, offset+2, characteristic->value_handle);
    bt_store_16(event, offset+4, characteristic->end_handle);
    bt_store_16(event, offset+6, characteristic->properties);
    swap128(characteristic->uuid128, &event[offset+8]);
}

void daemon_gatt_deserialize_characteristic_descriptor(uint8_t * packet, int offset, le_characteristic_descriptor_t * descriptor){
    descriptor->handle = READ_BT_16(packet, offset);
    swap128(&packet[offset+2], descriptor->uuid128);
}

void daemon_gatt_serialize_characteristic_descriptor(le_characteristic_descriptor_t * characteristic_descriptor, uint8_t * event, int offset){
    bt_store_16(event, offset, characteristic_descriptor->handle);
    swap128(characteristic_descriptor->uuid128, &event[offset+2]);
}

// setup events
void daemon_setup_service_event(le_event_t *le_event, uint8_t* event) {
    le_service_event_t * service_event = (le_service_event_t *) le_event;
    event[0] = le_event->type;
    event[1] = SERVICE_LENGTH;
    bt_store_16(event, 2, service_event->client->handle);
    daemon_gatt_serialize_service(&service_event->service, event, 4);
}

void daemon_gatt_setup_characteristic_event(le_event_t *le_event, uint8_t* event) {
    le_characteristic_event_t * characteristic_event = (le_characteristic_event_t *) le_event;
    event[0] = le_event->type;
    event[1] = CHARACTERISTIC_LENGTH;
    bt_store_16(event, 2, characteristic_event->client->handle);
    daemon_gatt_serialize_characteristic(&characteristic_event->characteristic, event, 4);
}

void daemon_setup_characteristic_descriptor_event(le_event_t *le_event, uint8_t* event) {
    le_characteristic_descriptor_event_t * descriptor_event = (le_characteristic_descriptor_event_t *) le_event;
    event[0] = le_event->type;
    event[1] = CHARACTERISTIC_DESCRIPTOR_LENGTH;
    bt_store_16(event, 2, descriptor_event->client->handle);
    daemon_gatt_serialize_characteristic_descriptor(&descriptor_event->characteristic_descriptor, event, 4);
}

void daemon_setup_characteristic_value_event(le_event_t *le_event, uint8_t* event) {
    le_characteristic_value_event_t * cvalue_event = (le_characteristic_value_event_t *) le_event;
    event[0] = le_event->type;
    event[1] = 2 + (2 + 2 + cvalue_event->blob_length);
    bt_store_16(event, 2, cvalue_event->client->handle);
    bt_store_16(event, 4, cvalue_event->value_handle);
    // TODO bt_store_16(event, 6, cvalue_event->blob_length);
    event[6] = cvalue_event->blob_length;
    memcpy(&event[7], cvalue_event->blob, cvalue_event->blob_length); // &event[8] with 2 byte length
}

#endif

static int btstack_command_handler(connection_t *connection, uint8_t *packet, uint16_t size){
    
    bd_addr_t addr;
    bd_addr_type_t addr_type;
    hci_con_handle_t handle;
    uint16_t cid;
    uint16_t psm;
    uint16_t service_channel;
    uint16_t mtu;
    uint8_t  reason;
    uint8_t  rfcomm_channel;
    uint8_t  rfcomm_credits;
    uint32_t service_record_handle;
    client_state_t *client;

#if defined(HAVE_MALLOC) && defined(HAVE_BLE)
    gatt_client_t * context;
    uint8_t uuid128[16];
    le_service_t service;
    le_characteristic_t characteristic;
    le_characteristic_descriptor_t descriptor;
    uint16_t data_length;
    uint8_t  * data;
#endif

    uint16_t serviceSearchPatternLen;
    uint16_t attributeIDListLen;

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
#ifdef USE_BLUETOOL
        case BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED:
            log_info("BTSTACK_SET_SYSTEM_BLUETOOTH_ENABLED %u", packet[3]);
            iphone_system_bt_set_enabled(packet[3]);
            hci_emit_system_bluetooth_enabled(iphone_system_bt_enabled());
            break;
            
        case BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED:
            log_info("BTSTACK_GET_SYSTEM_BLUETOOTH_ENABLED");
            hci_emit_system_bluetooth_enabled(iphone_system_bt_enabled());
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
            hci_discoverable_control(clients_require_discoverable());
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
            bt_flip_addr(addr, &packet[3]);
            psm = READ_BT_16(packet, 9);
            mtu = READ_BT_16(packet, 11);
            l2cap_create_channel_internal( connection, NULL, addr, psm, mtu);
            break;
        case L2CAP_CREATE_CHANNEL:
            bt_flip_addr(addr, &packet[3]);
            psm = READ_BT_16(packet, 9);
            l2cap_create_channel_internal( connection, NULL, addr, psm, 150);   // until r865
            break;
        case L2CAP_DISCONNECT:
            cid = READ_BT_16(packet, 3);
            reason = packet[5];
            l2cap_disconnect_internal(cid, reason);
            break;
        case L2CAP_REGISTER_SERVICE:
            psm = READ_BT_16(packet, 3);
            mtu = READ_BT_16(packet, 5);
            l2cap_register_service_internal(connection, NULL, psm, mtu, LEVEL_0);
            break;
        case L2CAP_UNREGISTER_SERVICE:
            psm = READ_BT_16(packet, 3);
            l2cap_unregister_service_internal(connection, psm);
            break;
        case L2CAP_ACCEPT_CONNECTION:
            cid    = READ_BT_16(packet, 3);
            l2cap_accept_connection_internal(cid);
            break;
        case L2CAP_DECLINE_CONNECTION:
            cid    = READ_BT_16(packet, 3);
            reason = packet[7];
            l2cap_decline_connection_internal(cid, reason);
            break;
        case RFCOMM_CREATE_CHANNEL:
            bt_flip_addr(addr, &packet[3]);
            rfcomm_channel = packet[9];
            rfcomm_create_channel_internal( connection, &addr, rfcomm_channel );
            break;
        case RFCOMM_CREATE_CHANNEL_WITH_CREDITS:
            bt_flip_addr(addr, &packet[3]);
            rfcomm_channel = packet[9];
            rfcomm_credits = packet[10];
            rfcomm_create_channel_with_initial_credits_internal( connection, &addr, rfcomm_channel, rfcomm_credits );
            break;
        case RFCOMM_DISCONNECT:
            cid = READ_BT_16(packet, 3);
            reason = packet[5];
            rfcomm_disconnect_internal(cid);
            break;
        case RFCOMM_REGISTER_SERVICE:
            rfcomm_channel = packet[3];
            mtu = READ_BT_16(packet, 4);
            rfcomm_register_service_internal(connection, rfcomm_channel, mtu);
            break;
        case RFCOMM_REGISTER_SERVICE_WITH_CREDITS:
            rfcomm_channel = packet[3];
            mtu = READ_BT_16(packet, 4);
            rfcomm_credits = packet[6];
            rfcomm_register_service_with_initial_credits_internal(connection, rfcomm_channel, mtu, rfcomm_credits);
            break;
        case RFCOMM_UNREGISTER_SERVICE:
            service_channel = READ_BT_16(packet, 3);
            rfcomm_unregister_service_internal(service_channel);
            break;
        case RFCOMM_ACCEPT_CONNECTION:
            cid    = READ_BT_16(packet, 3);
            rfcomm_accept_connection_internal(cid);
            break;
        case RFCOMM_DECLINE_CONNECTION:
            cid    = READ_BT_16(packet, 3);
            reason = packet[7];
            rfcomm_decline_connection_internal(cid);
            break;            
        case RFCOMM_GRANT_CREDITS:
            cid    = READ_BT_16(packet, 3);
            rfcomm_credits = packet[5];
            rfcomm_grant_credits(cid, rfcomm_credits);
            break;
        case RFCOMM_PERSISTENT_CHANNEL: {
            if (remote_device_db) {
                // enforce \0
                packet[3+248] = 0;
                rfcomm_channel = remote_device_db->persistent_rfcomm_channel((char*)&packet[3]);
            } else {
                // NOTE: hack for non-iOS platforms
                rfcomm_channel = rfcomm_channel_generator++;
            }
            log_info("RFCOMM_EVENT_PERSISTENT_CHANNEL %u", rfcomm_channel);
            uint8_t event[4];
            event[0] = RFCOMM_EVENT_PERSISTENT_CHANNEL;
            event[1] = sizeof(event) - 2;
            event[2] = 0;
            event[3] = rfcomm_channel;
            hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
            socket_connection_send_packet(connection, HCI_EVENT_PACKET, 0, (uint8_t *) event, sizeof(event));
            break;
        }
            
        case SDP_REGISTER_SERVICE_RECORD:
            log_info("SDP_REGISTER_SERVICE_RECORD size %u\n", size);
            sdp_register_service_internal(connection, &packet[3]);
            break;
        case SDP_UNREGISTER_SERVICE_RECORD:
            service_record_handle = READ_BT_32(packet, 3);
            log_info("SDP_UNREGISTER_SERVICE_RECORD handle 0x%x ", service_record_handle);
            sdp_unregister_service_internal(connection, service_record_handle);
            break;
        case SDP_CLIENT_QUERY_RFCOMM_SERVICES: 
            bt_flip_addr(addr, &packet[3]);

            serviceSearchPatternLen = de_get_len(&packet[9]);
            memcpy(serviceSearchPattern, &packet[9], serviceSearchPatternLen);

            sdp_query_rfcomm_register_callback(handle_sdp_rfcomm_service_result, connection);
            sdp_query_rfcomm_channel_and_name_for_search_pattern(addr, serviceSearchPattern);

            break;
        case SDP_CLIENT_QUERY_SERVICES:
            bt_flip_addr(addr, &packet[3]);
            sdp_parser_init();
            sdp_parser_register_callback(handle_sdp_client_query_result);

            serviceSearchPatternLen = de_get_len(&packet[9]);
            memcpy(serviceSearchPattern, &packet[9], serviceSearchPatternLen);
            
            attributeIDListLen = de_get_len(&packet[9+serviceSearchPatternLen]); 
            memcpy(attributeIDList, &packet[9+serviceSearchPatternLen], attributeIDListLen);
            
            sdp_client_query(addr, (uint8_t*)&serviceSearchPattern[0], (uint8_t*)&attributeIDList[0]);

            // sdp_general_query_for_uuid(addr, 0x1002);
            break;
        case GAP_LE_SCAN_START:
            le_central_start_scan();
            break;
        case GAP_LE_SCAN_STOP:
            le_central_stop_scan();
            break;
        case GAP_LE_CONNECT:
            bt_flip_addr(addr, &packet[4]);
            addr_type = packet[3];
            le_central_connect(&addr, addr_type);
            break;
        case GAP_LE_CONNECT_CANCEL:
            le_central_connect_cancel();
            break;
        case GAP_DISCONNECT:
            handle = READ_BT_16(packet, 3);
            gap_disconnect(handle);
            break;
#if defined(HAVE_MALLOC) && defined(HAVE_BLE)
        case GATT_DISCOVER_ALL_PRIMARY_SERVICES:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            gatt_client_discover_primary_services(context);
            break;
        case GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID16:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            gatt_client_discover_primary_services_by_uuid16(context, READ_BT_16(packet, 5));
            break;
        case GATT_DISCOVER_PRIMARY_SERVICES_BY_UUID128:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            swap128(&packet[25], uuid128);
            gatt_client_discover_primary_services_by_uuid128(context, uuid128);
            break;
        case GATT_FIND_INCLUDED_SERVICES_FOR_SERVICE:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_service(packet, 5, &service);
            gatt_client_find_included_services_for_service(context, &service);
            break;
        
        case GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_service(packet, 5, &service);
            gatt_client_discover_characteristics_for_service(context, &service);
            break;
        case GATT_DISCOVER_CHARACTERISTICS_FOR_SERVICE_BY_UUID128:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_service(packet, 5, &service);
            swap128(&packet[5 + SERVICE_LENGTH], uuid128);
            gatt_client_discover_characteristics_for_service_by_uuid128(context, &service, uuid128);
            break;
        case GATT_DISCOVER_CHARACTERISTIC_DESCRIPTOR:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_characteristic(packet, 5, &characteristic);
            gatt_client_discover_characteristic_descriptors(context, &characteristic);
            break;
        
        case GATT_READ_VALUE_OF_CHARACTERISTIC:
            // context = daemon_prepare_gatt_client_context(connection, packet, GATT_CHARACTERISTIC_VALUE_QUERY_RESULT);
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_characteristic(packet, 5, &characteristic);
            gatt_client_read_value_of_characteristic(context, &characteristic);
            break;
        case GATT_READ_LONG_VALUE_OF_CHARACTERISTIC:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_characteristic(packet, 5, &characteristic);
            gatt_client_read_long_value_of_characteristic(context, &characteristic);
            break;
        
        case GATT_WRITE_VALUE_OF_CHARACTERISTIC_WITHOUT_RESPONSE:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_characteristic(packet, 5, &characteristic);
            data_length = READ_BT_16(packet, 5 + CHARACTERISTIC_LENGTH);
            data = daemon_get_data_buffer(context);
            memcpy(data, &packet[7 + CHARACTERISTIC_LENGTH], data_length);
            gatt_client_write_value_of_characteristic_without_response(context, characteristic.value_handle, data_length, data);
            break;
        case GATT_WRITE_VALUE_OF_CHARACTERISTIC:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_characteristic(packet, 5, &characteristic);
            data_length = READ_BT_16(packet, 5 + CHARACTERISTIC_LENGTH);
            data = daemon_get_data_buffer(context);
            memcpy(data, &packet[7 + CHARACTERISTIC_LENGTH], data_length);
            gatt_client_write_value_of_characteristic(context, characteristic.value_handle, data_length, data);
            break;
        case GATT_WRITE_LONG_VALUE_OF_CHARACTERISTIC:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_characteristic(packet, 5, &characteristic);
            data_length = READ_BT_16(packet, 5 + CHARACTERISTIC_LENGTH);
            data = daemon_get_data_buffer(context);
            memcpy(data, &packet[7 + CHARACTERISTIC_LENGTH], data_length);
            gatt_client_write_long_value_of_characteristic(context, characteristic.value_handle, data_length, data);
            break;
        case GATT_RELIABLE_WRITE_LONG_VALUE_OF_CHARACTERISTIC:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_characteristic(packet, 5, &characteristic);
            data_length = READ_BT_16(packet, 5 + CHARACTERISTIC_LENGTH);
            data = daemon_get_data_buffer(context);
            memcpy(data, &packet[7 + CHARACTERISTIC_LENGTH], data_length);
            gatt_client_write_long_value_of_characteristic(context, characteristic.value_handle, data_length, data);
            break;
        case GATT_READ_CHARACTERISTIC_DESCRIPTOR:
            // context = daemon_prepare_gatt_client_context(connection, packet, GATT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT);
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_characteristic_descriptor(packet, 5, &descriptor);
            gatt_client_read_characteristic_descriptor(context, &descriptor);
            break;
        case GATT_READ_LONG_CHARACTERISTIC_DESCRIPTOR:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_characteristic_descriptor(packet, 5, &descriptor);
            gatt_client_read_long_characteristic_descriptor(context, &descriptor);
            break;
            
        case GATT_WRITE_CHARACTERISTIC_DESCRIPTOR:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_characteristic_descriptor(packet, 5, &descriptor);
            data = daemon_get_data_buffer(context);
            data_length = READ_BT_16(packet, 5 + CHARACTERISTIC_DESCRIPTOR_LENGTH);
            gatt_client_write_characteristic_descriptor(context, &descriptor, data_length, data);
            break;
        case GATT_WRITE_LONG_CHARACTERISTIC_DESCRIPTOR:
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_characteristic_descriptor(packet, 5, &descriptor);
            data = daemon_get_data_buffer(context);
            data_length = READ_BT_16(packet, 5 + CHARACTERISTIC_DESCRIPTOR_LENGTH);
            gatt_client_write_long_characteristic_descriptor(context, &descriptor, data_length, data);
            break;
        case GATT_WRITE_CLIENT_CHARACTERISTIC_CONFIGURATION:{
            uint16_t configuration = READ_BT_16(packet, 5 + CHARACTERISTIC_LENGTH);
            context = daemon_prepare_gatt_client_context(connection, packet);
            if (!context) break;
            daemon_gatt_deserialize_characteristic(packet, 5, &characteristic);
            gatt_client_write_client_characteristic_configuration(context, &characteristic, configuration);
            break;
        }
#endif
    default:
            log_error("Error: command %u not implemented\n:", READ_CMD_OCF(packet));
            break;
    }
    
    // verbose log info on command before dumped command unknown to PacketLogger or Wireshark
    hci_dump_packet( HCI_COMMAND_DATA_PACKET, 1, packet, size);

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
        case HCI_ACL_DATA_PACKET:
            err = hci_send_acl_packet(data, length);
            break;
        case L2CAP_DATA_PACKET:
            // process l2cap packet...
            err = l2cap_send_internal(channel, data, length);
            if (err == BTSTACK_ACL_BUFFERS_FULL) {
                l2cap_block_new_credits(1);
            }
            break;
        case RFCOMM_DATA_PACKET:
            // process l2cap packet...
            err = rfcomm_send_internal(channel, data, length);
            break;
        case DAEMON_EVENT_PACKET:
            switch (data[0]) {
                case DAEMON_EVENT_CONNECTION_OPENED:
                    log_info("DAEMON_EVENT_CONNECTION_OPENED %p\n",connection);

                    client = malloc(sizeof(client_state_t));
                    if (!client) break; // fail
                    client->connection   = connection;
                    client->power_mode   = HCI_POWER_OFF;
                    client->discoverable = 0;
                    linked_list_add(&clients, (linked_item_t *) client);
                    break;
                case DAEMON_EVENT_CONNECTION_CLOSED:
                    log_info("DAEMON_EVENT_CONNECTION_CLOSED %p\n",connection);
                    sdp_unregister_services_for_connection(connection);
                    rfcomm_close_connection(connection);
                    l2cap_close_connection(connection);
                    client = client_for_connection(connection);
                    if (!client) break;
                    linked_list_remove(&clients, (linked_item_t *) client);
                    free(client);
                    // update discoverable mode
                    hci_discoverable_control(clients_require_discoverable());
                    // start power off, if last active client
                    if (!clients_require_power_on()){
                        start_power_off_timer();
                    }
                    break;
                case DAEMON_NR_CONNECTIONS_CHANGED:
                    log_info("Nr Connections changed, new %u\n",data[1]);
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
    switch (packet[0]) {
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

    if (!socket_connection_has_parked_connections()){
        l2cap_block_new_credits(0);
    }

    // unlock mutex
    retry_mutex = 0;
}

static void daemon_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            deamon_status_event_handler(packet, size);
            switch (packet[0]){
                case HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS:
                    // ACL buffer freed...
                    daemon_retry_parked();
                    // no need to tell clients
                    return;
                case RFCOMM_EVENT_CREDITS:
                    // RFCOMM CREDITS received...
                    daemon_retry_parked();
                    break;

#if defined(HAVE_BLE) && defined(HAVE_MALLOC)
                case HCI_EVENT_DISCONNECTION_COMPLETE:{
                    uint16_t handle = READ_BT_16(packet, 3);
                    gatt_client_t *context = get_gatt_client_context_for_handle(handle);
                    if (!context) break;
                    // @TODO return some complete event if request is active
                    gatt_client_stop(context);
                    free(context);
                    break;
                }
#endif
                default:
                    break;
            }
        case DAEMON_EVENT_PACKET:
            switch (packet[0]){
                case DAEMON_EVENT_NEW_RFCOMM_CREDITS:
                    daemon_retry_parked();
                    break;
                default:
                    break;
            }
        default:
            break;
    }
    if (connection) {
        socket_connection_send_packet(connection, packet_type, channel, packet, size);
    } else {
        socket_connection_send_packet_all(packet_type, channel, packet, size);
    }
}

static void handle_sdp_rfcomm_service_result(sdp_query_event_t * rfcomm_event, void * context){
    switch (rfcomm_event->type){
        case SDP_QUERY_RFCOMM_SERVICE: {
            sdp_query_rfcomm_service_event_t * service_event = (sdp_query_rfcomm_service_event_t*) rfcomm_event;
            int name_len = (int)strlen((const char*)service_event);
            int event_len = 3 + name_len; 
            uint8_t event[event_len];
            event[0] = rfcomm_event->type;
            event[1] = 1 + name_len;
            event[2] = service_event->channel_nr;
            memcpy(&event[3], service_event->service_name, name_len);
            hci_dump_packet(HCI_EVENT_PACKET, 0, event, event_len);
            socket_connection_send_packet(context, HCI_EVENT_PACKET, 0, event, event_len);
            break;
        }
        case SDP_QUERY_COMPLETE: {
            sdp_query_complete_event_t * complete_event = (sdp_query_complete_event_t*) rfcomm_event;
            uint8_t event[] = { rfcomm_event->type, 1, complete_event->status};
            hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
            socket_connection_send_packet(context, HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
    }
}

static void sdp_client_assert_buffer(int size){
    if (size > attribute_value_buffer_size){
        log_error("SDP attribute value buffer size exceeded: available %d, required %d", attribute_value_buffer_size, size);
    }
}

// define new packet type SDP_CLIENT_PACKET
static void handle_sdp_client_query_result(sdp_query_event_t * event){
    sdp_query_attribute_value_event_t * ve;
    sdp_query_complete_event_t * complete_event;

    switch (event->type){
        case SDP_QUERY_ATTRIBUTE_VALUE:
            ve = (sdp_query_attribute_value_event_t*) event;
            
            sdp_client_assert_buffer(ve->attribute_length);

            attribute_value[ve->data_offset] = ve->data;

            if ((uint16_t)(ve->data_offset+1) == ve->attribute_length){
                hexdump(attribute_value, ve->attribute_length);

                int event_len = 1 + 3 * 2 + ve->attribute_length; 
                uint8_t event[event_len];
                event[0] = SDP_QUERY_ATTRIBUTE_VALUE;
                event[1] = ve->record_id;
                event[3] = ve->attribute_id;
                event[5] = ve->attribute_length;

                memcpy(&event[7], attribute_value, ve->attribute_length);
                hci_dump_packet(SDP_CLIENT_PACKET, 0, event, event_len);
                socket_connection_send_packet(NULL, SDP_CLIENT_PACKET, 0, event, event_len);
            }

            break;
        case SDP_QUERY_COMPLETE:
            complete_event = (sdp_query_complete_event_t*) event;
            uint8_t event[] = { SDP_QUERY_COMPLETE, 1, complete_event->status};
            hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
            socket_connection_send_packet(NULL, HCI_EVENT_PACKET, 0, event, sizeof(event));
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
    
#ifdef USE_BLUETOOL
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
        run_loop_remove_timer(&timeout);
        timeout_active = 0;
    }
#endif
}

static void start_power_off_timer(void){
#ifdef USE_POWER_OFF_TIMER    
    stop_power_off_timer();
    run_loop_set_timer(&timeout, DAEMON_NO_ACTIVE_CLIENT_TIMEOUT);
    run_loop_add_timer(&timeout);
    timeout_active = 1;
#else
    hci_power_control(HCI_POWER_OFF);
#endif
}

// MARK: manage list of clients


static client_state_t * client_for_connection(connection_t *connection) {
    linked_item_t *it;
    for (it = (linked_item_t *) clients; it ; it = it->next){
        client_state_t * client_state = (client_state_t *) it;
        if (client_state->connection == connection) {
            return client_state;
        }
    }
    return NULL;
}

static void clients_clear_power_request(void){
    linked_item_t *it;
    for (it = (linked_item_t *) clients; it ; it = it->next){
        client_state_t * client_state = (client_state_t *) it;
        client_state->power_mode = HCI_POWER_OFF;
    }
}

static int clients_require_power_on(void){
    
    if (global_enable) return 1;
    
    linked_item_t *it;
    for (it = (linked_item_t *) clients; it ; it = it->next){
        client_state_t * client_state = (client_state_t *) it;
        if (client_state->power_mode == HCI_POWER_ON) {
            return 1;
        }
    }
    return 0;
}

static int clients_require_discoverable(void){
    linked_item_t *it;
    for (it = (linked_item_t *) clients; it ; it = it->next){
        client_state_t * client_state = (client_state_t *) it;
        if (client_state->discoverable) {
            return 1;
        }
    }
    return 0;
}

static void usage(const char * name) {
    log_info("%s, BTstack background daemon\n", name);
    log_info("usage: %s [-h|--help] [--tcp]\n", name);
    log_info("    -h|--help  display this usage\n");
    log_info("    --tcp      use TCP server socket instead of local unix socket\n");
}

#ifdef USE_BLUETOOL 
static void * run_loop_thread(void *context){
    run_loop_execute();
    return NULL;
}
#endif

#ifdef HAVE_BLE

static void handle_gatt_client_event(le_event_t * le_event){
    connection_t * context = (connection_t *)le_event->client->context;
    gatt_complete_event_t * complete_event = (gatt_complete_event_t *) le_event;
    
    switch(le_event->type){

        case GATT_SERVICE_QUERY_RESULT:
        case GATT_INCLUDED_SERVICE_QUERY_RESULT:{
            uint8_t event[4 + SERVICE_LENGTH];
            daemon_setup_service_event(le_event, event);
            hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
            socket_connection_send_packet(context, HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
        case GATT_CHARACTERISTIC_QUERY_RESULT:{
            uint8_t event[4 + CHARACTERISTIC_LENGTH];
            daemon_gatt_setup_characteristic_event(le_event, event);
            hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
            socket_connection_send_packet(context, HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
            
        case GATT_ALL_CHARACTERISTIC_DESCRIPTORS_QUERY_RESULT:
        case GATT_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:
        case GATT_LONG_CHARACTERISTIC_DESCRIPTOR_QUERY_RESULT:{
            uint8_t event[4 + CHARACTERISTIC_DESCRIPTOR_LENGTH];
            daemon_setup_characteristic_descriptor_event(le_event, event);
            hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
            socket_connection_send_packet(context, HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
        
        case GATT_CHARACTERISTIC_VALUE_QUERY_RESULT: {
            uint8_t event[4 + 2 + 1 + ATT_MAX_ATTRIBUTE_SIZE];  // (type, len, handle), handle, len, data 
            daemon_setup_characteristic_value_event(le_event, event);
            hci_dump_packet(HCI_EVENT_PACKET, 0, event, sizeof(event));
            socket_connection_send_packet(context, HCI_EVENT_PACKET, 0, event, sizeof(event));
            break;
        }
            
        case GATT_LONG_CHARACTERISTIC_VALUE_QUERY_RESULT:
            // TODO
            break;
        case GATT_NOTIFICATION:
            // TODO
            break;
        case GATT_INDICATION:
            // TODO
            break;
            
        case GATT_QUERY_COMPLETE:
            send_gatt_query_complete(context, complete_event->client->handle, complete_event->status);
            break;
       
        default:
            break;
    }
}
#endif

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
    
    // make stdout unbuffered
    setbuf(stdout, NULL);
    log_error("BTdaemon started\n");

    // handle CTRL-c
    signal(SIGINT, daemon_sigint_handler);
    // handle SIGTERM - suggested for launchd
    signal(SIGTERM, daemon_sigint_handler);
    // handle SIGPIPE
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction (SIGPIPE, &act, NULL);
    
    bt_control_t * control = NULL;
    
#ifdef HAVE_TRANSPORT_H4
    config.device_name   = UART_DEVICE;
    config.baudrate_init = UART_SPEED;
    config.baudrate_main = 0;
    config.flowcontrol = 1;
#if defined(USE_BLUETOOL) && defined(USE_POWERMANAGEMENT)
    if (bt_control_iphone_power_management_supported()){
        // use default (max) UART baudrate over netraph interface
        config.baudrate_init = 0;
        transport = hci_transport_h4_iphone_instance();
    } else {
        transport = hci_transport_h4_instance();
    }
#else
    transport = hci_transport_h4_instance();
#endif
#endif

#ifdef HAVE_TRANSPORT_USB
    transport = hci_transport_usb_instance();
#endif

#ifdef USE_BLUETOOL
    control = &bt_control_iphone;
#endif
    
#if defined(USE_BLUETOOL) && defined(USE_POWERMANAGEMENT)
    if (bt_control_iphone_power_management_supported()){
        hci_transport_h4_iphone_set_enforce_wake_device("/dev/btwake");
    }
#endif

#ifdef USE_SPRINGBOARD
    bluetooth_status_handler = platform_iphone_status_handler;
    platform_iphone_register_window_manager_restart(update_ui_status);
    platform_iphone_register_preferences_changed(preferences_changed_callback);
#endif
    
#ifdef REMOTE_DEVICE_DB
    remote_device_db = &REMOTE_DEVICE_DB;
#endif

    run_loop_init(RUN_LOOP_POSIX);
    
    // init power management notifications
    if (control && control->register_for_power_notifications){
        control->register_for_power_notifications(power_notification_callback);
    }

    // logging
    loggingEnabled = 0;
    int newLoggingEnabled = 1;
#ifdef USE_BLUETOOL
    // iPhone has toggle in Preferences.app
    newLoggingEnabled = platform_iphone_logging_enabled();
#endif
    daemon_set_logging_enabled(newLoggingEnabled);
    
    // init HCI
    hci_init(transport, &config, control, remote_device_db);

#ifdef USE_BLUETOOL
    // iPhone doesn't use SSP yet as there's no UI for it yet and auto accept is not an option
    hci_ssp_set_enable(0);
#endif
    // init L2CAP
    l2cap_init();
    l2cap_register_packet_handler(&daemon_packet_handler);
    timeout.process = daemon_no_connections_timeout;

#ifdef HAVE_RFCOMM
    log_info("config.h: HAVE_RFCOMM\n");
    rfcomm_init();
    rfcomm_register_packet_handler(&daemon_packet_handler);
#endif
    
#ifdef HAVE_SDP
    sdp_init();
    sdp_register_packet_handler(&daemon_packet_handler);
#endif

#ifdef HAVE_BLE
    gatt_client_init();
    gatt_client_register_handler(&handle_gatt_client_event);
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
        
#ifdef USE_BLUETOOL 
    // notify daemons
    notify_post("ch.ringwald.btstack.started");

    // spawn thread to have BTstack run loop on new thread, while main thread is used to keep CFRunLoop
    pthread_t run_loop;
    pthread_create(&run_loop, NULL, &run_loop_thread, NULL);

    // needed to receive notifications
    CFRunLoopRun();
#endif
    
    // go!
    run_loop_execute();
    return 0;
}
