/*
 * Copyright (C) 2023 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hci_transport_netgraph.c"

#include "hci_transport_netgraph.h"

#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_event.h"
#include "btstack_run_loop.h"
#include "hci_transport.h"
#include "hci.h"

#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/bitstring.h>
#include <sys/types.h>
#include <sys/socket.h>

// avoid warning by /usr/include/netgraph/bluetooth/include/ng_btsocket.h
#define L2CAP_SOCKET_CHECKED
#include <bluetooth.h>
#include <netgraph.h>
#include <netgraph/bluetooth/include/ng_hci.h>
#include <netgraph/bluetooth/include/ng_l2cap.h>
#include <netgraph/bluetooth/include/ng_btsocket.h>

// hci packet handler
static void (*hci_transport_netgraph_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

// data source for integration with BTstack Runloop
static btstack_data_source_t hci_transport_netgraph_data_source_hci_raw;

// block write
static uint8_t         hci_transport_netgraph_write_packet_type;
static int             hci_transport_netgraph_write_bytes_len;
static const uint8_t * hci_transport_netgraph_write_bytes_data;

// assert pre-buffer for packet type is available
#if !defined(HCI_OUTGOING_PRE_BUFFER_SIZE) || (HCI_OUTGOING_PRE_BUFFER_SIZE == 0)
#error HCI_OUTGOING_PRE_BUFFER_SIZE not defined. Please update hci.h
#endif

typedef enum {
    TX_OFF,
    TX_IDLE,
    TX_W4_PACKET_SENT,
} TX_STATE;

// write state
static TX_STATE hci_transport_netgraph_tx_state;

// incoming packet buffer
static uint8_t hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + HCI_INCOMING_PACKET_BUFFER_SIZE + 1]; // packet type + max(acl header + acl payload, event header + event data)
static uint8_t * hci_packet = &hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];

// Netgraph node
const char * hci_node_name = "ubt0hci";

// Control and Data socket for BTstack Netgraph node
static int hci_transport_netgraph_hci_raw_control_socket;
static int hci_transport_netgraph_hci_raw_data_socket;

// Track HCI Netgraph Initialization
#define HCI_TODO_READ_BD_ADDR                   1
#define HCI_TODO_READ_LOCAL_SUPPORTED_FEATURES  2
#define HCI_TODO_READ_BUFFER_SIZE               4

static uint8_t hci_transport_netgraph_hci_todo_init;

static void hci_transport_netgraph_process_write(btstack_data_source_t *ds) {

    if (hci_transport_netgraph_write_bytes_len == 0) return;

    const char * hook = NULL;
    switch (hci_transport_netgraph_write_packet_type){
        case HCI_COMMAND_DATA_PACKET:
            hook = "raw";
            break;
        case HCI_ACL_DATA_PACKET:
            hook = "acl";
            break;
        default:
            btstack_unreachable();
            break;
    }

    int res = NgSendData(ds->source.fd, hook, hci_transport_netgraph_write_bytes_data, hci_transport_netgraph_write_bytes_len);

    if (res < 0){
        log_error("send data via hook %s returned error %d", hook, errno);
        btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);
        return;
    }

    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);

    hci_transport_netgraph_tx_state = TX_IDLE;
    hci_transport_netgraph_write_bytes_len = 0;
    hci_transport_netgraph_write_bytes_data = NULL;

    // notify upper stack that it can send again
    static const uint8_t packet_sent_event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
    hci_transport_netgraph_packet_handler(HCI_EVENT_PACKET, (uint8_t *) &packet_sent_event[0], sizeof(packet_sent_event));
}

static void hci_transport_netgraph_process_read(btstack_data_source_t * ds) {
    // read complete HCI packet
    char hook[NG_NODESIZ];
    int bytes_read = NgRecvData(ds->source.fd, (u_char *)  hci_packet, HCI_INCOMING_PACKET_BUFFER_SIZE + 1, hook);

    if (bytes_read < 0){
        log_error("Read failed %d", (int) bytes_read);
    } else {

        // ignore 'echo' - only accept hci events from 'raw' hook and acl packet from 'acl' hook
        // - HCI CMDs that are received over 'raw' hook are sent back via 'raw' hook
        if (strcmp(hook, "raw") == 0){
            if (hci_packet[0] != HCI_EVENT_PACKET){
                return;
            }
        }
        // - ACL Packets that are received from driver are also sent via 'raw' hook
        if (strcmp(hook, "acl") == 0){
            if (hci_packet[0] != HCI_ACL_DATA_PACKET){
                return;
            }
        }

        // track HCI Event Complete for init commands
        if (hci_packet[0] == HCI_EVENT_PACKET){
            if (hci_event_packet_get_type(&hci_packet[1]) == HCI_EVENT_COMMAND_COMPLETE){
                uint8_t todos_before = hci_transport_netgraph_hci_todo_init;
                switch (hci_event_command_complete_get_command_opcode(&hci_packet[1])){
                    case HCI_OPCODE_HCI_RESET:
                        hci_transport_netgraph_hci_todo_init = HCI_TODO_READ_BD_ADDR | HCI_TODO_READ_LOCAL_SUPPORTED_FEATURES | HCI_TODO_READ_BUFFER_SIZE;
                        break;
                    case HCI_OPCODE_HCI_READ_BD_ADDR:
                        hci_transport_netgraph_hci_todo_init &= ~HCI_TODO_READ_BD_ADDR;
                        break;
                    case HCI_OPCODE_HCI_READ_LOCAL_SUPPORTED_FEATURES:
                        hci_transport_netgraph_hci_todo_init &= ~HCI_TODO_READ_LOCAL_SUPPORTED_FEATURES;
                        break;
                    case HCI_OPCODE_HCI_READ_BUFFER_SIZE:
                        hci_transport_netgraph_hci_todo_init &= ~HCI_TODO_READ_BUFFER_SIZE;
                        break;
                    default:
                        break;
                }
                if ((todos_before != 0) && (hci_transport_netgraph_hci_todo_init == 0)){
                    // tell ubt0hci to disconnect acl hook to ubt0l2cap
                    char path[NG_NODESIZ];
                    snprintf(path, sizeof(path), "%s:", hci_node_name);
                    if (NgSendAsciiMsg(hci_transport_netgraph_hci_raw_control_socket, path, "%s", "init") < 0) {
                        log_error("Failed to send init to %s", path);
                    }
                }
            }
        }

        uint16_t packet_len = bytes_read-1u;
        hci_transport_netgraph_packet_handler(hci_packet[0], &hci_packet[1], packet_len);
    }
}

static void hci_transport_netgraph_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    if (ds->source.fd < 0) return;
    switch (callback_type){
        case DATA_SOURCE_CALLBACK_READ:
            hci_transport_netgraph_process_read(ds);
            break;
        case DATA_SOURCE_CALLBACK_WRITE:
            hci_transport_netgraph_process_write(ds);
            break;
        default:
            break;
    }
}

// TODO: could pass device name
static void hci_transport_netgraph_init (const void *transport_config){
    log_info("init");

    btstack_assert(transport_config != NULL);

    // extract netgraph device name
    hci_transport_config_netgraph_t * hci_transport_config_netgraph = (hci_transport_config_netgraph_t*) transport_config;
    hci_node_name = hci_transport_config_netgraph->device_name;

    // set state to off
    hci_transport_netgraph_tx_state = TX_OFF;

    // reset sockets file descriptors
    hci_transport_netgraph_hci_raw_control_socket = -1;
    hci_transport_netgraph_hci_raw_data_socket    = -1;
}

static int hci_transport_netgraph_open(void) {

    log_info("open(%s)", hci_node_name);

    int res;
    struct ngm_rmhook rmh;
    struct ngm_connect con;
    char path[NG_NODESIZ];
    char btstack_node_hci_raw_name[NG_NODESIZ];

    // create hci_raw netgraph node
    snprintf(btstack_node_hci_raw_name, sizeof(btstack_node_hci_raw_name), "btstack");
    if (NgMkSockNode(btstack_node_hci_raw_name, &hci_transport_netgraph_hci_raw_control_socket, &hci_transport_netgraph_hci_raw_data_socket) < 0){
        log_error("Cannot create netgraph node '%s'", btstack_node_hci_raw_name);
        return -1;
    }

    // tell hci node to disconnect raw hook to btsock_hci_raw (ignore result)
    snprintf(path, sizeof(path), "%s:", hci_node_name);
    strncpy(rmh.ourhook, "raw", sizeof(rmh.ourhook));
    res = NgSendMsg(hci_transport_netgraph_hci_raw_control_socket, path, NGM_GENERIC_COOKIE,
                        NGM_RMHOOK, &rmh, sizeof(rmh));
    log_info("Remove HCI RAW Hook: %d", res);

    // tell hci node to disconnect acl hook to ubt0l2cap (ignore result)
    snprintf(path, sizeof(path), "%s:", hci_node_name);
    strncpy(rmh.ourhook, "acl", sizeof(rmh.ourhook));
    res = NgSendMsg(hci_transport_netgraph_hci_raw_control_socket, path, NGM_GENERIC_COOKIE,
                    NGM_RMHOOK, &rmh, sizeof(rmh));
    log_info("Remove ACL Hook: %d", res);

    // connect ubth0hci/raw hook
    snprintf(path, sizeof(path), "%s:",         btstack_node_hci_raw_name);
    snprintf(con.path, sizeof(con.path), "%s:", hci_node_name);
    strncpy(con.ourhook,  "raw", sizeof(con.ourhook));
    strncpy(con.peerhook, "raw", sizeof(con.peerhook));
    if (NgSendMsg(hci_transport_netgraph_hci_raw_control_socket, path, NGM_GENERIC_COOKIE,
                  NGM_CONNECT, &con, sizeof(con)) < 0) {
        log_error("Cannot connect %s%s to %s%s", path, con.ourhook, con.path, con.peerhook);
        return -1;
    }

    // connect ubth0hci/acl hook
    snprintf(path, sizeof(path), "%s:",         btstack_node_hci_raw_name);
    snprintf(con.path, sizeof(con.path), "%s:", hci_node_name);
    strncpy(con.ourhook,  "acl", sizeof(con.ourhook));
    strncpy(con.peerhook, "acl", sizeof(con.peerhook));
    if (NgSendMsg(hci_transport_netgraph_hci_raw_control_socket, path, NGM_GENERIC_COOKIE,
                  NGM_CONNECT, &con, sizeof(con)) < 0) {
        log_error("Cannot connect %s%s to %s%s", path, con.ourhook, con.path, con.peerhook);
        return -1;
    }

    // set up HCI RAW data_source
    btstack_run_loop_set_data_source_fd(&hci_transport_netgraph_data_source_hci_raw, hci_transport_netgraph_hci_raw_data_socket);
    btstack_run_loop_set_data_source_handler(&hci_transport_netgraph_data_source_hci_raw, &hci_transport_netgraph_process);
    btstack_run_loop_add_data_source(&hci_transport_netgraph_data_source_hci_raw);
    btstack_run_loop_enable_data_source_callbacks(&hci_transport_netgraph_data_source_hci_raw, DATA_SOURCE_CALLBACK_READ);

    // init tx state machines
    hci_transport_netgraph_tx_state = TX_IDLE;

    return 0;
}

static int hci_transport_netgraph_close(void){
    // first remove data source
    btstack_run_loop_remove_data_source(&hci_transport_netgraph_data_source_hci_raw);

    log_info("Close Netgraph sockets %u and %u", hci_transport_netgraph_hci_raw_control_socket, hci_transport_netgraph_hci_raw_data_socket);

    // then close device
    close(hci_transport_netgraph_hci_raw_control_socket);
    close(hci_transport_netgraph_hci_raw_data_socket);

    // and reset file descriptors
    hci_transport_netgraph_hci_raw_control_socket = -1;
    hci_transport_netgraph_hci_raw_data_socket    = -1;
    return 0;
}

static void hci_transport_netgraph_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    hci_transport_netgraph_packet_handler = handler;
}

static int hci_transport_netgraph_can_send_now(uint8_t packet_type){
    UNUSED(packet_type);
    return hci_transport_netgraph_tx_state == TX_IDLE;
}

static int hci_transport_netgraph_send_packet(uint8_t packet_type, uint8_t * packet, int size) {
    btstack_assert(hci_transport_netgraph_write_bytes_len == 0);

    // store packet type before actual data and increase size
    uint8_t * buffer = &packet[-1];
    uint32_t  buffer_size = size + 1;
    buffer[0] = packet_type;

    // setup async write
    hci_transport_netgraph_write_bytes_data = buffer;
    hci_transport_netgraph_write_bytes_len  = buffer_size;
    hci_transport_netgraph_write_packet_type = packet_type;
    hci_transport_netgraph_tx_state = TX_W4_PACKET_SENT;

    btstack_run_loop_enable_data_source_callbacks(&hci_transport_netgraph_data_source_hci_raw, DATA_SOURCE_CALLBACK_WRITE);
    return 0;
}

static const hci_transport_t hci_transport_netgraph = {
    .name = "Netgraph",
    .init = &hci_transport_netgraph_init,
    .open = &hci_transport_netgraph_open,
    .close = &hci_transport_netgraph_close,
    .register_packet_handler = &hci_transport_netgraph_register_packet_handler,
    .can_send_packet_now = &hci_transport_netgraph_can_send_now,
    .send_packet = &hci_transport_netgraph_send_packet
};

const hci_transport_t * hci_transport_netgraph_instance(void) {
    return &hci_transport_netgraph;
}
