/*
 * Copyright (C) 2025 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hci_transport_linux.c"

#include "hci_transport_linux.h"

#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_run_loop.h"
#include "hci_transport.h"
#include "hci.h"

#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>

#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

// we cannot include hci_lib.h directly as it clashes with some of our hci functions
// prefix all clashing functions with linux_
#define hci_send_cmd linux_hci_send_cmd
#define hci_create_connection linux_hci_create_connection
#define hci_disconnect linux_hci_disconnect
#define hci_inquiry linux_hci_inquiry
#define hci_read_local_name linux_hci_read_local_name
#define hci_write_local_name linux_hci_write_local_name
#define hci_read_clock_offset linux_hci_read_clock_offset
#define hci_read_bd_addr linux_hci_read_bd_addr
#define hci_delete_stored_link_key linux_hci_delete_stored_link_key
#define hci_write_inquiry_scan_type linux_hci_write_inquiry_scan_type
#define hci_write_inquiry_mode linux_hci_write_inquiry_mode
#define hci_write_simple_pairing_mode linux_hci_write_simple_pairing_mode
#define hci_read_local_oob_data linux_hci_read_local_oob_data
#define hci_write_inquiry_transmit_power_level linux_hci_write_inquiry_transmit_power_level
#define hci_read_transmit_power_level linux_hci_read_transmit_power_level
#define hci_read_link_supervision_timeout linux_hci_read_link_supervision_timeout
#define hci_write_link_supervision_timeout linux_hci_write_link_supervision_timeout
#define hci_read_link_quality linux_hci_read_link_quality
#define hci_read_rssi linux_hci_read_rssi
#define hci_read_clock linux_hci_read_clock
#define hci_le_set_scan_enable linux_hci_le_set_scan_enable
#define hci_le_set_scan_parameters linux_hci_le_set_scan_parameters
#define hci_le_set_advertise_enable linux_hci_le_set_advertise_enable
#define hci_le_read_white_list_size linux_hci_le_read_white_list_size
#define hci_le_clear_white_list linux_hci_le_clear_white_list
#define hci_le_clear_resolving_list linux_hci_le_clear_resolving_list
#define hci_le_read_resolving_list_size linux_hci_le_read_resolving_list_size

// assert pre-buffer for packet type is available
#if !defined(HCI_OUTGOING_PRE_BUFFER_SIZE) || (HCI_OUTGOING_PRE_BUFFER_SIZE == 0)
#error HCI_OUTGOING_PRE_BUFFER_SIZE not defined. Please update hci.h
#endif


// hci packet handler
static void (*hci_transport_linux_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = NULL;

// data source for integration with BTstack Runloop
static btstack_data_source_t hci_transport_linux_data_source;

// incoming packet buffer
static uint8_t hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + HCI_INCOMING_PACKET_BUFFER_SIZE + 1]; // packet type + max(acl header + acl payload, event header + event data)
static uint8_t * hci_packet = &hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];

// linux hci socket
static int hci_socket = -1;

static const hci_transport_config_linux_t * hci_transport_config_linux;

static void hci_transport_linux_process_read(btstack_data_source_t * ds) {
    UNUSED(ds);

    int bytes_read = read(hci_socket, hci_packet, HCI_INCOMING_PACKET_BUFFER_SIZE);
    if (bytes_read < 0) {
        perror("Failed to read HCI packet");
        return;
    }

    hci_transport_linux_packet_handler(hci_packet[0], &hci_packet[1], bytes_read-1);
}

static void hci_transport_linux_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    switch (callback_type){
        case DATA_SOURCE_CALLBACK_READ:
            hci_transport_linux_process_read(ds);
            break;
        default:
            break;
    }
}

static void hci_transport_linux_init (const void *transport_config){
    btstack_assert(transport_config != NULL);
    hci_transport_config_linux = (const hci_transport_config_linux_t *) transport_config;
    btstack_assert(hci_transport_config_linux->type == HCI_TRANSPORT_CONFIG_LINUX);
    log_info("Using device id %u", hci_transport_config_linux->device_id);
}

static int hci_transport_linux_open(void) {
    struct sockaddr_hci addr;

    hci_socket = socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, BTPROTO_HCI);
    if (hci_socket < 0) {
        perror("Failed to create HCI socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.hci_family = AF_BLUETOOTH;
    addr.hci_dev = hci_transport_config_linux->device_id;
    addr.hci_channel = HCI_CHANNEL_USER;
    if (bind(hci_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Failed to bind HCI socket");
        close(hci_socket);
        return -1;
    }

    // set up data_source
    btstack_run_loop_set_data_source_fd(&hci_transport_linux_data_source, hci_socket);
    btstack_run_loop_set_data_source_handler(&hci_transport_linux_data_source, &hci_transport_linux_process);
    btstack_run_loop_add_data_source(&hci_transport_linux_data_source);
    btstack_run_loop_enable_data_source_callbacks(&hci_transport_linux_data_source, DATA_SOURCE_CALLBACK_READ);

    return 0;
}

static int hci_transport_linux_close(void){
    btstack_run_loop_remove_data_source(&hci_transport_linux_data_source);
    if (hci_socket >= 0) {
        close(hci_socket);
        hci_socket = -1;
    }
    return 0;
}

static void hci_transport_linux_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    hci_transport_linux_packet_handler = handler;
}

static int hci_transport_linux_send_packet(uint8_t packet_type, uint8_t * packet, int size) {
    // store packet type before actual data and increase size
    uint8_t * buffer = &packet[-1];
    uint32_t  buffer_size = size + 1;
    buffer[0] = packet_type;

    if (write(hci_socket, buffer, buffer_size) < 0) {
        perror("Failed to send HCI packet");
        return -1;
    }
    return 0;
}

static const hci_transport_t hci_transport_linux = {
    .name = "Linux BlueZ",
    .init = &hci_transport_linux_init,
    .open = &hci_transport_linux_open,
    .close = &hci_transport_linux_close,
    .register_packet_handler = &hci_transport_linux_register_packet_handler,
    .send_packet = &hci_transport_linux_send_packet
};

const hci_transport_t * hci_transport_linux_instance(void) {
    return &hci_transport_linux;
}
