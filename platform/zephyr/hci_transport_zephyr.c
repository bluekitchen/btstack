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

// Zephyr
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/buf.h>
#include <zephyr/bluetooth/hci_raw.h>

// BTstack
#include "btstack_debug.h"
#include "hci.h"
#include "hci_transport.h"

#include "btstack_run_loop_zephyr.h"

static K_FIFO_DEFINE(rx_queue);

static void (*transport_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

static btstack_data_source_t hci_transport_zephyr_receive;
static fat_variable_t rx_queue_handle = { .variable = &rx_queue, .id = K_POLL_TYPE_FIFO_DATA_AVAILABLE };

static void hci_transport_zephyr_handler(btstack_data_source_t * ds, btstack_data_source_callback_type_t callback_type) {
    UNUSED(ds);
    UNUSED(callback_type);
    struct net_buf *buf = k_fifo_get( &rx_queue, K_FOREVER );

    uint16_t    size = buf->len;
    uint8_t * packet = buf->data;
    switch (bt_buf_get_type(buf)) {
        case BT_BUF_ISO_IN:
            transport_packet_handler(HCI_ISO_DATA_PACKET, packet, size);
            break;
        case BT_BUF_ACL_IN:
            transport_packet_handler(HCI_ACL_DATA_PACKET, packet, size);
            break;
        case BT_BUF_EVT:
            transport_packet_handler(HCI_EVENT_PACKET, packet, size);
            break;
        default:
            log_error("Unknown type %u\n", bt_buf_get_type(buf));
            break;
    }
    net_buf_unref(buf);
}


/**
 * init transport
 * @param transport_config
 */
static void transport_init(const void *transport_config){

    btstack_data_source_t *ds = &hci_transport_zephyr_receive;
    btstack_run_loop_set_data_source_handle(ds, &rx_queue_handle);
    btstack_run_loop_set_data_source_handler(ds, &hci_transport_zephyr_handler);
    btstack_run_loop_add_data_source(ds);
    btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);

    /* startup Controller */
    bt_enable_raw(&rx_queue);
    bt_hci_raw_set_mode( BT_HCI_RAW_MODE_PASSTHROUGH );
}

/**
 * open transport connection
 */
static int transport_open(void){
    return 0;
}

/**
 * close transport connection
 */
static int transport_close(void){
    return 0;
}

/**
 * register packet handler for HCI packets: ACL, SCO, and Events
 */
static void transport_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    transport_packet_handler = handler;
}

static void send_hardware_error(uint8_t error_code){
    // hci_outgoing_event[0] = HCI_EVENT_HARDWARE_ERROR;
    // hci_outgoing_event[1] = 1;
    // hci_outgoing_event[2] = error_code;
    // hci_outgoing_event_ready = 1;
}

static int transport_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    struct net_buf *buf;
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            buf = bt_buf_get_tx(BT_BUF_CMD, K_NO_WAIT, packet, size);
            if (!buf) {
                log_error("No available command buffers!\n");
                break;
            }

            bt_send(buf);
            break;
        case HCI_ACL_DATA_PACKET:
            buf = bt_buf_get_tx(BT_BUF_ACL_OUT, K_NO_WAIT, packet, size);
            if (!buf) {
                log_error("No available ACL buffers!\n");
                break;
            }

            bt_send(buf);
            break;
        case HCI_ISO_DATA_PACKET:
            buf = bt_buf_get_tx(BT_BUF_ISO_OUT, K_NO_WAIT, packet, size);
            if (!buf) {
                log_error("No available ISO buffers!\n");
                break;
            }

            bt_send(buf);
            break;
       default:
            send_hardware_error(0x01);  // invalid HCI packet
            break;
    }

    return 0;
}

static const hci_transport_t transport = {
    /* const char * name; */                                        "zephyr",
    /* void   (*init) (const void *transport_config); */            &transport_init,
    /* int    (*open)(void); */                                     &transport_open,
    /* int    (*close)(void); */                                    &transport_close,
    /* void   (*register_packet_handler)(void (*handler)(...); */   &transport_register_packet_handler,
    /* int    (*can_send_packet_now)(uint8_t packet_type); */       NULL,
    /* int    (*send_packet)(...); */                               &transport_send_packet,
    /* int    (*set_baudrate)(uint32_t baudrate); */                NULL,
    /* void   (*reset_link)(void); */                               NULL,
};

const hci_transport_t * hci_transport_zephyr_get_instance(void){
    return &transport;
}


