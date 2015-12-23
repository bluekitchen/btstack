/*
 * Copyright (C) 2015 BlueKitchen GmbH
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
 *  hci_h4_transport_wiced.c
 *
 *  HCI Transport API implementation for basic H4 protocol for use with run_loop_wiced.c
 */

#include "btstack-config.h"

#include "wiced.h"
#include "platform_bluetooth.h"

#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "hci.h"
#include "hci_transport.h"

// assert pre-buffer for packet type is available
#if !defined(HCI_OUTGOING_PRE_BUFFER_SIZE) || (HCI_OUTGOING_PRE_BUFFER_SIZE == 0)
#error HCI_OUTGOING_PRE_BUFFER_SIZE not defined. Please update hci.h
#endif

static wiced_result_t h4_rx_worker_receive_packet(void * arg);
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 

typedef struct hci_transport_h4 {
    hci_transport_t transport;
    data_source_t *ds;
    int uart_fd;    // different from ds->fd for HCI reader thread
    /* power management support, e.g. used by iOS */
    timer_source_t sleep_timer;
} hci_transport_h4_t;

// single instance
static hci_transport_h4_t * hci_transport_h4 = NULL;

static void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

static wiced_worker_thread_t tx_worker_thread;
static const uint8_t *       tx_worker_data_buffer;
static uint16_t              tx_worker_data_size;

static wiced_worker_thread_t rx_worker_thread;
static int                   rx_worker_read_pos;

static uint8_t hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + 1 + HCI_PACKET_BUFFER_SIZE]; // packet type + max(acl header + acl payload, event header + event data)
static uint8_t * hci_packet = &hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];

// executed on main run loop
static wiced_result_t h4_main_deliver_packet(void *arg){
    // deliver packet
    packet_handler(hci_packet[0], &hci_packet[1], rx_worker_read_pos-1);
    // trigger receive of next packet
    wiced_rtos_send_asynchronous_event(&rx_worker_thread, &h4_rx_worker_receive_packet, NULL);    
    return WICED_SUCCESS;
}

// executed on main run loop
static wiced_result_t h4_main_notify_packet_send(void *arg){
    // prepare for next packet
    tx_worker_data_size = 0;
    // notify upper stack that it might be possible to send again
    uint8_t event[] = { DAEMON_EVENT_HCI_PACKET_SENT, 0};
    packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
    return WICED_SUCCESS;
}

// executed on rx worker thread
static void h4_rx_worker_receive_bytes(int bytes_to_read){
    platform_uart_receive_bytes(wiced_bt_uart_driver, &hci_packet[rx_worker_read_pos], bytes_to_read, WICED_NEVER_TIMEOUT);
    rx_worker_read_pos += bytes_to_read;        
}
static wiced_result_t h4_rx_worker_receive_packet(void * arg){
    while (1){
        rx_worker_read_pos = 0;
        h4_rx_worker_receive_bytes(1);
        switch (hci_packet[0]){
            case HCI_EVENT_PACKET:
                h4_rx_worker_receive_bytes(HCI_EVENT_HEADER_SIZE);
                h4_rx_worker_receive_bytes(hci_packet[2]);
                break;
            case HCI_ACL_DATA_PACKET:
                h4_rx_worker_receive_bytes(HCI_ACL_HEADER_SIZE);
                h4_rx_worker_receive_bytes(READ_BT_16( hci_packet, 3));
                break;
            case HCI_SCO_DATA_PACKET:
                h4_rx_worker_receive_bytes(HCI_SCO_HEADER_SIZE);
                h4_rx_worker_receive_bytes(hci_packet[3]);
                break;
            default:
                // try again
                log_error("h4_rx_worker_receive_packet: invalid packet type 0x%02x", hci_packet[0]);
                continue;
        }
        // deliver packet on main thread
        wiced_execute_code_on_run_loop(&h4_main_deliver_packet, NULL);
        return WICED_SUCCESS;
    }
}

// executed on tx worker thread
static wiced_result_t h4_tx_worker_send_packet(void * arg){
    // blocking send
    platform_uart_transmit_bytes(wiced_bt_uart_driver, tx_worker_data_buffer, tx_worker_data_size);
    // let stack know
    wiced_execute_code_on_run_loop(&h4_main_notify_packet_send, NULL);
    return WICED_SUCCESS;
}

static int h4_set_baudrate(uint32_t baudrate){
#if 0
    log_info("h4_set_baudrate %u", baudrate);
#endif
    return 0;
}

static int h4_open(void *transport_config){
    wiced_uart_config_t uart_config =
    {
        .baud_rate    = 115200,
        .data_width   = DATA_WIDTH_8BIT,
        .parity       = NO_PARITY,
        .stop_bits    = STOP_BITS_1,
        .flow_control = FLOW_CONTROL_CTS_RTS,
    };

    // configure HOST and DEVICE WAKE PINs
    platform_gpio_init( wiced_bt_control_pins[WICED_BT_PIN_HOST_WAKE], INPUT_HIGH_IMPEDANCE);
    platform_gpio_init( wiced_bt_control_pins[WICED_BT_PIN_DEVICE_WAKE], OUTPUT_PUSH_PULL);
    platform_gpio_output_low( wiced_bt_control_pins[WICED_BT_PIN_DEVICE_WAKE]);
    wiced_rtos_delay_milliseconds( 100 );

    // power cycle Bluetooth
    platform_gpio_init( wiced_bt_control_pins[ WICED_BT_PIN_POWER ], OUTPUT_PUSH_PULL );
    platform_gpio_output_low( wiced_bt_control_pins[ WICED_BT_PIN_POWER ] );
    wiced_rtos_delay_milliseconds( 100 );
    platform_gpio_output_high( wiced_bt_control_pins[ WICED_BT_PIN_POWER ] );

    wiced_rtos_delay_milliseconds( 500 );

    // init UART
    platform_uart_init( wiced_bt_uart_driver, wiced_bt_uart_peripheral, &uart_config, NULL );

    // create worker threads for rx/tx
    wiced_rtos_create_worker_thread(&tx_worker_thread, WICED_NETWORK_WORKER_PRIORITY, 1000, 1);
    wiced_rtos_create_worker_thread(&rx_worker_thread, WICED_NETWORK_WORKER_PRIORITY, 1000, 5);

    // start receiving packet
    wiced_rtos_send_asynchronous_event(&rx_worker_thread, &h4_rx_worker_receive_packet, NULL);    

    // tx is ready
    tx_worker_data_size = 0;
    return 0;
}

static int h4_close(void *transport_config){
    // not implementd
    return 0;
}

static int h4_send_packet(uint8_t packet_type, uint8_t * data, int size){
    // store packet type before actual data and increase size
    size++;
    data--;
    *data = packet_type;

    // store in request
    tx_worker_data_buffer = data;
    tx_worker_data_size = size;
    // send packet as single block
    wiced_rtos_send_asynchronous_event(&tx_worker_thread, &h4_tx_worker_send_packet, NULL);    
    return 0;
}

static void h4_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

static int h4_can_send_packet_now(uint8_t packet_type){
    return tx_worker_data_size == 0;
}

static const char * h4_get_transport_name(void){
    return "H4";
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

// get h4 singleton
hci_transport_t * hci_transport_h4_wiced_instance() {
    if (hci_transport_h4 == NULL) {
        hci_transport_h4 = (hci_transport_h4_t*)malloc( sizeof(hci_transport_h4_t));
        hci_transport_h4->ds                                      = NULL;
        hci_transport_h4->transport.open                          = h4_open;
        hci_transport_h4->transport.close                         = h4_close;
        hci_transport_h4->transport.send_packet                   = h4_send_packet;
        hci_transport_h4->transport.register_packet_handler       = h4_register_packet_handler;
        hci_transport_h4->transport.get_transport_name            = h4_get_transport_name;
        hci_transport_h4->transport.set_baudrate                  = h4_set_baudrate;
        hci_transport_h4->transport.can_send_packet_now           = h4_can_send_packet_now;
    }
    return (hci_transport_t *) hci_transport_h4;
}
