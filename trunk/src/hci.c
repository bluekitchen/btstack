/*
 * Copyright (C) 2009 by Matthias Ringwald
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
 */

/*
 *  hci.c
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *
 */

#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "hci.h"
#include "hci_dump.h"

#include "../include/btstack/hci_cmds.h"
#include "../include/btstack/version.h"

// temp
#include "l2cap.h"

#define HCI_CONNECTION_TIMEOUT_MS 5000

// the STACK is here
static hci_stack_t       hci_stack;

/**
 * get connection for a given handle
 *
 * @return connection OR NULL, if not found
 */
static hci_connection_t * connection_for_handle(hci_con_handle_t con_handle){
    linked_item_t *it;
    for (it = (linked_item_t *) hci_stack.connections; it ; it = it->next){
        if ( ((hci_connection_t *) it)->con_handle == con_handle){
            return (hci_connection_t *) it;
        }
    }
    return NULL;
}

static void hci_connection_timeout_handler(timer_source_t *timer){
    hci_connection_t * connection = linked_item_get_user(&timer->item);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (tv.tv_sec >= connection->timestamp.tv_sec + HCI_CONNECTION_TIMEOUT_MS/1000) {
        // connections might be timed out
        hci_emit_l2cap_check_timeout(connection);
        run_loop_set_timer(timer, HCI_CONNECTION_TIMEOUT_MS);
    } else {
        // next timeout check at
        timer->timeout.tv_sec = connection->timestamp.tv_sec + HCI_CONNECTION_TIMEOUT_MS/1000;
    }
    run_loop_add_timer(timer);
}

static void hci_connection_timestamp(hci_connection_t *connection){
    gettimeofday(&connection->timestamp, NULL);
}

static void hci_connection_update_timestamp_for_acl(uint8_t *packet) {
    // update timestamp
    hci_con_handle_t con_handle = READ_ACL_CONNECTION_HANDLE(packet);
    hci_connection_t *connection = connection_for_handle( con_handle);
    if (connection) hci_connection_timestamp(connection);
}

/**
 * create connection for given address
 *
 * @return connection OR NULL, if not found
 */
static hci_connection_t * create_connection_for_addr(bd_addr_t addr){
    hci_connection_t * conn = malloc( sizeof(hci_connection_t) );
    if (!conn) return NULL;
    BD_ADDR_COPY(conn->address, addr);
    conn->con_handle = 0xffff;
    conn->flags = 0;
    linked_item_set_user(&conn->timeout.item, conn);
    conn->timeout.process = hci_connection_timeout_handler;
    hci_connection_timestamp(conn);
    linked_list_add(&hci_stack.connections, (linked_item_t *) conn);
    return conn;
}

/**
 * get connection for given address
 *
 * @return connection OR NULL, if not found
 */
static hci_connection_t * connection_for_address(bd_addr_t address){
    linked_item_t *it;
    for (it = (linked_item_t *) hci_stack.connections; it ; it = it->next){
        if ( ! BD_ADDR_CMP( ((hci_connection_t *) it)->address, address) ){
            return (hci_connection_t *) it;
        }
    }
    return NULL;
}

/**
 * count connections
 */
static int nr_hci_connections(){
    int count = 0;
    linked_item_t *it;
    for (it = (linked_item_t *) hci_stack.connections; it ; it = it->next, count++);
    return count;
}

/** 
 * Dummy handler called by HCI
 */
static void dummy_handler(uint8_t *packet, uint16_t size){
}

/**
 * Dummy control handler
 */
static int null_control_function(void *config){
    return 0;
}
static const char * null_control_name(void *config){
    return "Hardware unknown";
}
static bt_control_t null_control = {
    null_control_function,
    null_control_function,
    null_control_function,
    null_control_name
}; 


int hci_send_acl_packet(uint8_t *packet, int size){
    hci_connection_update_timestamp_for_acl(packet);
    return hci_stack.hci_transport->send_acl_packet(packet, size);
}

static void acl_handler(uint8_t *packet, int size){
    hci_connection_update_timestamp_for_acl(packet);
    hci_stack.acl_packet_handler(packet, size);
    
    // execute main loop
    hci_run();
}

static void event_handler(uint8_t *packet, int size){
    bd_addr_t addr;
    hci_con_handle_t handle;
    hci_connection_t * conn;
    
    switch (packet[0]) {
            
        case HCI_EVENT_COMMAND_COMPLETE:
        case HCI_EVENT_COMMAND_STATUS:
            // Get Num_HCI_Command_Packets
            hci_stack.num_cmd_packets = packet[2];
            break;
        
        case HCI_EVENT_CONNECTION_REQUEST:
            bt_flip_addr(addr, &packet[2]);
            // TODO: eval COD 8-10
            uint8_t link_type = packet[11];
            printf("Connection_incoming: "); print_bd_addr(addr); printf(", type %u\n", link_type);
            if (link_type == 1) { // ACL
                conn = connection_for_address(addr);
                if (!conn) {
                    conn = create_connection_for_addr(addr);
                }
                // TODO: check for malloc failure
                conn->state = ACCEPTED_CONNECTION_REQUEST;
                hci_send_cmd(&hci_accept_connection_request, addr, 1);
            } else {
                // TODO: decline request
            }
            break;
            
        case HCI_EVENT_CONNECTION_COMPLETE:
            // Connection management
            bt_flip_addr(addr, &packet[5]);
            printf("Connection_complete (status=%u)", packet[2]); print_bd_addr(addr); printf("\n");
            conn = connection_for_address(addr);
            if (conn) {
                if (!packet[2]){
                    conn->state = OPEN;
                    conn->con_handle = READ_BT_16(packet, 3);
                    conn->flags = 0;
                    
                    gettimeofday(&conn->timestamp, NULL);
                    run_loop_set_timer(&conn->timeout, HCI_CONNECTION_TIMEOUT_MS);
                    run_loop_add_timer(&conn->timeout);
                    
                    printf("New connection: handle %u, ", conn->con_handle);
                    print_bd_addr( conn->address );
                    printf("\n");
                    
                    hci_emit_nr_connections_changed();
                } else {
                    // connection failed, remove entry
                    linked_list_remove(&hci_stack.connections, (linked_item_t *) conn);
                    free( conn );
                }
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            if (!packet[2]){
                handle = READ_BT_16(packet, 3);
                hci_connection_t * conn = connection_for_handle(handle);
                if (conn) {
                    printf("Connection closed: handle %u, ", conn->con_handle);
                    print_bd_addr( conn->address );
                    printf("\n");
                    run_loop_remove_timer(&conn->timeout);
                    linked_list_remove(&hci_stack.connections, (linked_item_t *) conn);
                    free( conn );
                    hci_emit_nr_connections_changed();
                }
            }
            break;
            
        default:
            break;
    }

    // handle BT initialization
    if (hci_stack.state == HCI_STATE_INITIALIZING){
        // handle H4 synchronization loss on restart
        // if (hci_stack.substate == 1 && packet[0] == HCI_EVENT_HARDWARE_ERROR){
        //    hci_stack.substate = 0;
        // }
        // handle normal init sequence
        if (hci_stack.substate % 2){
            // odd: waiting for event
            if (packet[0] == HCI_EVENT_COMMAND_COMPLETE){
                hci_stack.substate++;
            }
        }
    }
        
    hci_stack.event_packet_handler(packet, size);
	
	// execute main loop
	hci_run();
}

/** Register HCI packet handlers */
void hci_register_event_packet_handler(void (*handler)(uint8_t *packet, uint16_t size)){
    hci_stack.event_packet_handler = handler;
}
void hci_register_acl_packet_handler  (void (*handler)(uint8_t *packet, uint16_t size)){
    hci_stack.acl_packet_handler = handler;
}

void hci_init(hci_transport_t *transport, void *config, bt_control_t *control){
    
    // reference to use transport layer implementation
    hci_stack.hci_transport = transport;
    
    // references to used control implementation
    if (control) {
        hci_stack.control = control;
    } else {
        hci_stack.control = &null_control;
    }
    
    // reference to used config
    hci_stack.config = config;
    
    // no connections yet
    hci_stack.connections = NULL;
    
    // empty cmd buffer
    hci_stack.hci_cmd_buffer = malloc(3+255);

    // higher level handler
    hci_stack.event_packet_handler = dummy_handler;
    hci_stack.acl_packet_handler = dummy_handler;

    // register packet handlers with transport
    transport->register_event_packet_handler( event_handler);
    transport->register_acl_packet_handler( acl_handler);
}

int hci_power_control(HCI_POWER_MODE power_mode){
    if (power_mode == HCI_POWER_ON && hci_stack.state == HCI_STATE_OFF) {
        
        // power on
        int err = hci_stack.control->on(hci_stack.config);
        if (err){
            fprintf(stderr, "POWER_ON failed\n");
            hci_emit_hci_open_failed();
            return err;
        }
        
        // open low-level device
        err = hci_stack.hci_transport->open(hci_stack.config);
        if (err){
            fprintf(stderr, "HCI_INIT failed, turning Bluetooth off again\n");
            hci_stack.control->off(hci_stack.config);
            hci_emit_hci_open_failed();
            return err;
        }
        
        // set up state machine
        hci_stack.num_cmd_packets = 1; // assume that one cmd can be sent
        hci_stack.state = HCI_STATE_INITIALIZING;
        hci_stack.substate = 0;
        
    } else if (power_mode == HCI_POWER_OFF && hci_stack.state == HCI_STATE_WORKING){
        
        // close low-level device
        hci_stack.hci_transport->close(hci_stack.config);

        // power off
        hci_stack.control->off(hci_stack.config);

        // we're off now
        hci_stack.state = HCI_STATE_OFF;
    }
    
    // create internal event
	hci_emit_state();
    
	// trigger next/first action
	hci_run();
	
    return 0;
}

void hci_run(){
    switch (hci_stack.state){
        case HCI_STATE_INITIALIZING:
            if (hci_stack.substate % 2) {
                // odd: waiting for command completion
                return;
            }
            if (hci_stack.num_cmd_packets == 0) {
                // cannot send command yet
                return;
            }
            switch (hci_stack.substate/2){
                case 0:
                    hci_send_cmd(&hci_reset);
                    break;
				case 1:
					hci_send_cmd(&hci_read_bd_addr);
					break;
                case 2:
                    // ca. 15 sec
                    hci_send_cmd(&hci_write_page_timeout, 0x6000);
                    break;
				case 3:
					hci_send_cmd(&hci_write_scan_enable, 3); // 3 inq scan + page scan
					break;
                case 4:
                    // done.
                    hci_stack.state = HCI_STATE_WORKING;
                    hci_emit_state();
                    break;
                default:
                    break;
            }
            hci_stack.substate++;
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
        bt_flip_addr(addr, &packet[3]);
        printf("Create_connection to "); print_bd_addr(addr); printf("\n");
        conn = connection_for_address(addr);
        if (conn) {
            // if connection exists
            if (conn->state == OPEN) {
                // if OPEN, emit connection complete command
                hci_emit_connection_complete(conn);
            }
            //    otherwise, just ignore
            return 0; // don't sent packet to controller
            
        } else{
            conn = create_connection_for_addr(addr);
            if (conn){
                //    create connection struct and register, state = SENT_CREATE_CONNECTION
                conn->state = SENT_CREATE_CONNECTION;
            }
        }
    }
    
    // accept connection
    
    // reject connection
    
    // close_connection?
      // set state = SENT_DISCONNECT 
    
    hci_stack.num_cmd_packets--;
    return hci_stack.hci_transport->send_cmd_packet(packet, size);
}

/**
 * pre: numcmds >= 0 - it's allowed to send a command to the controller
 */
int hci_send_cmd(hci_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint8_t * hci_cmd_buffer = hci_stack.hci_cmd_buffer;
    uint16_t size = hci_create_cmd_internal(hci_stack.hci_cmd_buffer, cmd, argptr);
    va_end(argptr);
    return hci_send_cmd_packet(hci_cmd_buffer, size);
}

// Create various non-HCI events. 
// TODO: generalize, use table similar to hci_create_command

void hci_emit_state(){
    uint8_t len = 3; 
    uint8_t event[len];
    event[0] = BTSTACK_EVENT_STATE;
    event[1] = len - 3;
    event[2] = hci_stack.state;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, len);
    hci_stack.event_packet_handler(event, len);
}

void hci_emit_connection_complete(hci_connection_t *conn){
    uint8_t len = 13; 
    uint8_t event[len];
    event[0] = HCI_EVENT_CONNECTION_COMPLETE;
    event[1] = len - 3;
    event[2] = 0; // status = OK
    bt_store_16(event, 3, conn->con_handle);
    bt_flip_addr(&event[5], conn->address);
    event[11] = 1; // ACL connection
    event[12] = 0; // encryption disabled
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, len);
    hci_stack.event_packet_handler(event, len);
}

void hci_emit_l2cap_check_timeout(hci_connection_t *conn){
    uint8_t len = 4; 
    uint8_t event[len];
    event[0] = L2CAP_EVENT_TIMEOUT_CHECK;
    event[1] = len - 2;
    bt_store_16(event, 2, conn->con_handle);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, len);
    hci_stack.event_packet_handler(event, len);
}

void hci_emit_nr_connections_changed(){
    uint8_t len = 3; 
    uint8_t event[len];
    event[0] = BTSTACK_EVENT_NR_CONNECTIONS_CHANGED;
    event[1] = len - 2;
    event[2] = nr_hci_connections();
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, len);
    hci_stack.event_packet_handler(event, len);
}

void hci_emit_hci_open_failed(){
    uint8_t len = 2; 
    uint8_t event[len];
    event[0] = BTSTACK_EVENT_POWERON_FAILED;
    event[1] = len - 2;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, len);
    hci_stack.event_packet_handler(event, len);
}


void hci_emit_btstack_version() {
    uint8_t len = 6;
    uint8_t event[len];
    event[0] = BTSTACK_EVENT_VERSION;
    event[1] = len - 2;
    event[len++] = BTSTACK_MAJOR;
    event[len++] = BTSTACK_MINOR;
    bt_store_16(event, len, BTSTACK_REVISION);
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, len);
    hci_stack.event_packet_handler(event, len);
}

void hci_emit_system_bluetooth_enabled(uint8_t enabled){
    uint8_t len = 3; 
    uint8_t event[len];
    event[0] = BTSTACK_EVENT_SYSTEM_BLUETOOTH_ENABLED;
    event[1] = len - 3;
    event[2] = enabled;
    hci_dump_packet( HCI_EVENT_PACKET, 0, event, len);
    hci_stack.event_packet_handler(event, len);
}
