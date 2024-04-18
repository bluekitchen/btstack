/*
 * Copyright (C) 2020 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "controller.c"

#define DEBUG

#include <string.h>

#include "controller.h"

#include "ll.h"

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"
#include "bluetooth_company_id.h"
#include "hci_event.h"
#include "hci_transport.h"
#include "btstack_tlv.h"
#include "btstack_tlv_none.h"
#include "ble/le_device_db_tlv.h"
#include "hci_cmd.h"

// HCI Connection Handle used for all HCI events/connections
#define HCI_CON_HANDLE 0x0001

//
// Controller
//
static uint8_t send_hardware_error;
static bool send_transport_sent;

static btstack_data_source_t hci_transport_data_source;

static uint8_t hci_outgoing_event[258];
static bool    hci_outgoing_event_ready;

static void (*hci_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

/** BTstack Controller Implementation */

// Controller State
static bool controller_ll_acl_reserved;

static void send_command_complete(uint16_t opcode, uint8_t status, const uint8_t * result, uint16_t len){
    hci_event_create_from_template_and_arguments(hci_outgoing_event, sizeof(hci_outgoing_event),
         &hci_event_command_complete, /* num commands */ 1, opcode, status, len, result);
    hci_outgoing_event_ready = true;
    btstack_run_loop_poll_data_sources_from_irq();
}

static void fake_command_complete(uint16_t opcode){
    hci_event_create_from_template_and_arguments(hci_outgoing_event, sizeof(hci_outgoing_event),
         &hci_event_command_complete, /* num commands */ 1, opcode, ERROR_CODE_SUCCESS, 0, NULL);
    hci_outgoing_event_ready = true;
    btstack_run_loop_poll_data_sources_from_irq();
}

static void controller_handle_hci_command(uint8_t * packet, uint16_t size){

    btstack_assert(hci_outgoing_event_ready == false);

    const uint8_t local_supported_features[] = { 0, 0, 0, 0, 0x40, 0, 0, 0};
    const uint8_t read_buffer_size_result[] = { 0x1b, 0, HCI_NUM_TX_BUFFERS_STACK };
    uint8_t status;

    uint16_t opcode = little_endian_read_16(packet, 0);
    switch (opcode){
        case HCI_OPCODE_HCI_RESET:
            fake_command_complete(opcode);
            break;
        case HCI_OPCODE_HCI_READ_LOCAL_SUPPORTED_FEATURES:
            // No. 37, byte 4, bit 6 = LE Supported (Controller)
            send_command_complete(opcode, 0, local_supported_features, 8);
            break;
        case HCI_OPCODE_HCI_LE_READ_BUFFER_SIZE:
            send_command_complete(opcode, 0, read_buffer_size_result, 8);
            break;
        case HCI_OPCODE_HCI_LE_SET_ADVERTISING_PARAMETERS:
            status = ll_set_advertising_parameters(
                    little_endian_read_16(packet,3),
                    little_endian_read_16(packet,5),
                    packet[7],
                    packet[8],
                    packet[9],
                    &packet[10],
                    packet[16],
                    packet[17]);
            send_command_complete(opcode, status, NULL, 0);
            break;
        case HCI_OPCODE_HCI_LE_SET_ADVERTISING_DATA:
            status = ll_set_advertising_data(packet[3], &packet[4]);
            send_command_complete(opcode, status, NULL, 0);
            break;
        case HCI_OPCODE_HCI_LE_SET_ADVERTISE_ENABLE:
            status = ll_set_advertise_enable(packet[3]);
            send_command_complete(opcode, status, NULL, 0);
            break;
        case HCI_OPCODE_HCI_LE_SET_SCAN_ENABLE:
            ll_set_scan_enable(packet[3], packet[4]);
            fake_command_complete(opcode);
            break;
        case HCI_OPCODE_HCI_LE_SET_SCAN_PARAMETERS:
            ll_set_scan_parameters(packet[3], little_endian_read_16(packet, 4), little_endian_read_16(packet, 6), packet[8], packet[9]);
            fake_command_complete(opcode);
            break;
        default:
            log_debug("CMD opcode %02x not handled yet", opcode);
            // try with "OK"
            fake_command_complete(opcode);
            break;
    }
}

// ACL handler
static void controller_handle_acl_data(uint8_t * packet, uint16_t size){
    // so far, only single connection supported with fixed con handle
    hci_con_handle_t con_handle = little_endian_read_16(packet, 0) & 0xfff;
    btstack_assert(con_handle == HCI_CON_HANDLE);
    btstack_assert( size > 4);

    // just queue up
    btstack_assert(controller_ll_acl_reserved);
    controller_ll_acl_reserved = false;
    ll_queue_acl_packet(packet, size);
}

static void transport_emit_hci_event(const hci_event_t * event, ...){
    va_list argptr;
    va_start(argptr, event);
    uint16_t length = hci_event_create_from_template_and_arglist(hci_outgoing_event, event, argptr);
    va_end(argptr);
    hci_packet_handler(HCI_EVENT_PACKET, hci_outgoing_event, length);
}

static void transport_run(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type){

    // deliver command complete events caused by command processor
    if (hci_outgoing_event_ready){
        hci_outgoing_event_ready = false;
        hci_packet_handler(HCI_EVENT_PACKET, hci_outgoing_event, hci_outgoing_event[1]+2);
    }

    if (send_hardware_error != 0){
        uint8_t error_code = send_hardware_error;
        send_hardware_error = 0;
        transport_emit_hci_event(&hci_event_hardware_error, error_code);
    }

    if (send_transport_sent){
        send_transport_sent = false;
        // notify upper stack that it might be possible to send again
        transport_emit_hci_event(&hci_event_transport_packet_sent);
    }

    ll_execute_once();
}

static void transport_packet_handler(uint8_t packet_type, uint8_t * packet, uint16_t size){
    // just forward to hci
    (*hci_packet_handler)(packet_type, packet, size);
}

/**
 * init transport
 * @param transport_config
 */
static void transport_init(const void *transport_config){
    UNUSED(transport_config);
    ll_register_packet_handler(&transport_packet_handler);
}

/**
 * open transport connection
 */
static int transport_open(void){
    btstack_run_loop_set_data_source_handler(&hci_transport_data_source, &transport_run);
    btstack_run_loop_enable_data_source_callbacks(&hci_transport_data_source, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&hci_transport_data_source);

    ll_init();
    ll_radio_on();

    return 0;
}

/**
 * close transport connection
 */
static int transport_close(void){
    btstack_run_loop_remove_data_source(&hci_transport_data_source);

    // TODO

    return 0;
}

/**
 * register packet handler for HCI packets: ACL, SCO, and Events
 */
static void transport_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    hci_packet_handler = handler;
}

static int transport_can_send_packet_now(uint8_t packet_type){
    if (send_transport_sent) return 0;
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            return hci_outgoing_event_ready ? 0 : 1;
        case HCI_ACL_DATA_PACKET:
            if (controller_ll_acl_reserved == false){
                controller_ll_acl_reserved = ll_reserve_acl_packet();
            }
            return controller_ll_acl_reserved ? 1 : 0;
        default:
            btstack_assert(false);
            break;
    }
    return 0;
}

static int transport_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            controller_handle_hci_command(packet, size);
            send_transport_sent = true;
            break;
        case HCI_ACL_DATA_PACKET:
            controller_handle_acl_data(packet, size);
            send_transport_sent = true;
            break;
        default:
            send_hardware_error = 0x01; // invalid HCI packet
            break;
    }
    return 0;    
}

void controller_init(void){
}

static const hci_transport_t controller_transport = {
        "sx1280-vhci",
        &transport_init,
        &transport_open,
        &transport_close,
        &transport_register_packet_handler,
        &transport_can_send_packet_now,
        &transport_send_packet,
        NULL, // set baud rate
        NULL, // reset link
        NULL, // set SCO config
};

const hci_transport_t * controller_get_hci_transport(void){
    return &controller_transport;
}
