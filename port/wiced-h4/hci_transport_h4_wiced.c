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
 *  HCI Transport API implementation for basic H4 protocol for use with btstack_run_loop_wiced.c
 */

#define __BTSTACK_FILE__ "hci_transport_h4_wiced.c"

#include "btstack_config.h"
#include "btstack_run_loop_wiced.h"

#include "btstack_debug.h"
#include "hci.h"
#include "hci_transport.h"
#include "platform_bluetooth.h"

#include "wiced.h"

#include <stdio.h>
#include <string.h>

// priority higher than WIFI to make sure RTS is set
#define WICED_BT_UART_THREAD_PRIORITY        (WICED_NETWORK_WORKER_PRIORITY - 2)
#define WICED_BT_UART_THREAD_STACK_SIZE      300

// assert pre-buffer for packet type is available
#if !defined(HCI_OUTGOING_PRE_BUFFER_SIZE) || (HCI_OUTGOING_PRE_BUFFER_SIZE == 0)
#error HCI_OUTGOING_PRE_BUFFER_SIZE not defined. Please update hci.h
#endif

/* Default of 512 bytes should be fine. Only needed if WICED_BT_UART_MANUAL_CTS_RTS */
#ifndef RX_RING_BUFFER_SIZE
#define RX_RING_BUFFER_SIZE 512
#endif


static wiced_result_t h4_rx_worker_receive_packet(void * arg);
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 

typedef struct hci_transport_h4 {
    hci_transport_t transport;
    btstack_data_source_t *ds;
    int uart_fd;    // different from ds->fd for HCI reader thread
    /* power management support, e.g. used by iOS */
    btstack_timer_source_t sleep_timer;
} hci_transport_h4_t;

// single instance
static hci_transport_h4_t * hci_transport_h4 = NULL;
static hci_transport_config_uart_t * hci_transport_config_uart = NULL;

static void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

static wiced_worker_thread_t tx_worker_thread;
static const uint8_t *       tx_worker_data_buffer;
static uint16_t              tx_worker_data_size;

static wiced_worker_thread_t rx_worker_thread;
static int                   rx_worker_read_pos;

static uint8_t hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + 1 + HCI_PACKET_BUFFER_SIZE]; // packet type + max(acl header + acl payload, event header + event data)
static uint8_t * hci_packet = &hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];

#ifdef WICED_BT_UART_MANUAL_CTS_RTS
static volatile wiced_ring_buffer_t rx_ring_buffer;
static volatile uint8_t             rx_data[RX_RING_BUFFER_SIZE];
#endif

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
    uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
    packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
    return WICED_SUCCESS;
}

// executed on rx worker thread

static void h4_rx_worker_receive_bytes(int bytes_to_read){

#ifdef WICED_UART_READ_DOES_NOT_RETURN_BYTES_READ
    // older API passes in number of bytes to read (checked in 3.3.1 and 3.4.0)
    platform_uart_receive_bytes(wiced_bt_uart_driver, &hci_packet[rx_worker_read_pos], bytes_to_read, WICED_NEVER_TIMEOUT);
#else
    // newer API uses pointer to return number of read bytes
    uint32_t bytes = bytes_to_read;
    platform_uart_receive_bytes(wiced_bt_uart_driver, &hci_packet[rx_worker_read_pos], &bytes, WICED_NEVER_TIMEOUT);
    // assumption: bytes = bytes_to_rad as timeout is never    
#endif
    rx_worker_read_pos += bytes_to_read;        

}

static wiced_result_t h4_rx_worker_receive_packet(void * arg){

#ifdef WICED_BT_UART_MANUAL_CTS_RTS
    platform_gpio_output_low(wiced_bt_uart_pins[WICED_BT_PIN_UART_RTS]);
#endif

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
                h4_rx_worker_receive_bytes(little_endian_read_16( hci_packet, 3));
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

#ifdef WICED_BT_UART_MANUAL_CTS_RTS
        platform_gpio_output_high(wiced_bt_uart_pins[WICED_BT_PIN_UART_RTS]);
#endif

        // deliver packet on main thread
        btstack_run_loop_wiced_execute_code_on_main_thread(&h4_main_deliver_packet, NULL);
        return WICED_SUCCESS;
    }
}

// executed on tx worker thread
static wiced_result_t h4_tx_worker_send_packet(void * arg){
#ifdef WICED_BT_UART_MANUAL_CTS_RTS
    while (platform_gpio_input_get(wiced_bt_uart_pins[WICED_BT_PIN_UART_CTS]) == WICED_TRUE){
        wiced_rtos_delay_milliseconds(100);
    }
#endif
    // blocking send
    platform_uart_transmit_bytes(wiced_bt_uart_driver, tx_worker_data_buffer, tx_worker_data_size);
    // let stack know
    btstack_run_loop_wiced_execute_code_on_main_thread(&h4_main_notify_packet_send, NULL);
    return WICED_SUCCESS;
}

static int h4_set_baudrate(uint32_t baudrate){

#if defined(_STM32F205RGT6_) || defined(STM32F40_41xxx)

    // directly use STM peripheral functions to change baud rate dynamically
    
    // set TX to high
    log_info("h4_set_baudrate %u", (int) baudrate);
    const platform_gpio_t* gpio = wiced_bt_uart_pins[WICED_BT_PIN_UART_TX];
    platform_gpio_output_high(gpio);

    // reconfigure TX pin as GPIO
    GPIO_InitTypeDef gpio_init_structure;
    gpio_init_structure.GPIO_Speed = GPIO_Speed_50MHz;
    gpio_init_structure.GPIO_Mode  = GPIO_Mode_OUT;
    gpio_init_structure.GPIO_OType = GPIO_OType_PP;
    gpio_init_structure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    gpio_init_structure.GPIO_Pin   = (uint32_t) ( 1 << gpio->pin_number );
    GPIO_Init( gpio->port, &gpio_init_structure );

    // disable USART
    USART_Cmd( wiced_bt_uart_peripheral->port, DISABLE );

    // setup init structure
    USART_InitTypeDef uart_init_structure;
    uart_init_structure.USART_Mode       = USART_Mode_Rx | USART_Mode_Tx;
    uart_init_structure.USART_BaudRate   = baudrate;
    uart_init_structure.USART_WordLength = USART_WordLength_8b;
    uart_init_structure.USART_StopBits   = USART_StopBits_1;
    uart_init_structure.USART_Parity     = USART_Parity_No;
    uart_init_structure.USART_HardwareFlowControl = USART_HardwareFlowControl_RTS_CTS;
#ifdef WICED_BT_UART_MANUAL_CTS_RTS
    uart_init_structure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
#endif
    USART_Init(wiced_bt_uart_peripheral->port, &uart_init_structure);

    // enable USART again
    USART_Cmd( wiced_bt_uart_peripheral->port, ENABLE );

    // set TX pin as USART again
    gpio_init_structure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_Init( gpio->port, &gpio_init_structure );

#else
    log_error("h4_set_baudrate not implemented for this WICED Platform");
#endif
    return 0;
}

static void h4_init(const void * transport_config){
    // check for hci_transport_config_uart_t
    if (!transport_config) {
        log_error("hci_transport_h4_wiced: no config!");
        return;
    }
    if (((hci_transport_config_t*)transport_config)->type != HCI_TRANSPORT_CONFIG_UART) {
        log_error("hci_transport_h4_wiced: config not of type != HCI_TRANSPORT_CONFIG_UART!");
        return;
    }
    hci_transport_config_uart = (hci_transport_config_uart_t*) transport_config;
}

static int h4_open(void){

    // UART config
    wiced_uart_config_t uart_config =
    {
        .baud_rate    = 115200,
        .data_width   = DATA_WIDTH_8BIT,
        .parity       = NO_PARITY,
        .stop_bits    = STOP_BITS_1,
        .flow_control = FLOW_CONTROL_CTS_RTS,
    };
    wiced_ring_buffer_t * ring_buffer = NULL;

    // configure HOST and DEVICE WAKE PINs
    platform_gpio_init(wiced_bt_control_pins[WICED_BT_PIN_HOST_WAKE], INPUT_HIGH_IMPEDANCE);
    platform_gpio_init(wiced_bt_control_pins[WICED_BT_PIN_DEVICE_WAKE], OUTPUT_PUSH_PULL);
    platform_gpio_output_low(wiced_bt_control_pins[WICED_BT_PIN_DEVICE_WAKE]);

    /* Configure Reg Enable pin to output. Set to HIGH */
    if (wiced_bt_control_pins[ WICED_BT_PIN_POWER ]){
        platform_gpio_init( wiced_bt_control_pins[ WICED_BT_PIN_POWER ], OUTPUT_OPEN_DRAIN_PULL_UP );
        platform_gpio_output_high( wiced_bt_control_pins[ WICED_BT_PIN_POWER ] );
    }

    wiced_rtos_delay_milliseconds( 100 );

    // -- init UART
#ifdef WICED_BT_UART_MANUAL_CTS_RTS
    // configure RTS pin as output and set to high
    platform_gpio_init(wiced_bt_uart_pins[WICED_BT_PIN_UART_RTS], OUTPUT_PUSH_PULL);
    platform_gpio_output_high(wiced_bt_uart_pins[WICED_BT_PIN_UART_RTS]);

    // configure CTS to input, pull-up
    platform_gpio_init(wiced_bt_uart_pins[WICED_BT_PIN_UART_CTS], INPUT_PULL_UP);

    // use ring buffer to allow to receive RX_RING_BUFFER_SIZE/2 addition bytes before raising RTS
    // casts avoid warnings because of volatile qualifier
    ring_buffer_init((wiced_ring_buffer_t *) &rx_ring_buffer, (uint8_t*) rx_data, sizeof( rx_data ) );
    ring_buffer = (wiced_ring_buffer_t *) &rx_ring_buffer;

    // don't try
    uart_config.flow_control = FLOW_CONTROL_DISABLED;
#endif
    platform_uart_init( wiced_bt_uart_driver, wiced_bt_uart_peripheral, &uart_config, ring_buffer );


    // Reset Bluetooth via RESET line. Fallback to toggling POWER otherwise
    if ( wiced_bt_control_pins[ WICED_BT_PIN_RESET ]){
        platform_gpio_init( wiced_bt_control_pins[ WICED_BT_PIN_RESET ], OUTPUT_PUSH_PULL );
        platform_gpio_output_high( wiced_bt_control_pins[ WICED_BT_PIN_RESET ] );

        platform_gpio_output_low( wiced_bt_control_pins[ WICED_BT_PIN_RESET ] );
        wiced_rtos_delay_milliseconds( 100 );
        platform_gpio_output_high( wiced_bt_control_pins[ WICED_BT_PIN_RESET ] );
    }
    else if ( wiced_bt_control_pins[ WICED_BT_PIN_POWER ]){
        platform_gpio_output_low( wiced_bt_control_pins[ WICED_BT_PIN_POWER ] );
        wiced_rtos_delay_milliseconds( 100 );
        platform_gpio_output_high( wiced_bt_control_pins[ WICED_BT_PIN_POWER ] );
    }

    // wait for Bluetooth to start up
    wiced_rtos_delay_milliseconds( 500 );

    // create worker threads for rx/tx. only single request is posted to their queues
    wiced_rtos_create_worker_thread(&tx_worker_thread, WICED_BT_UART_THREAD_PRIORITY, WICED_BT_UART_THREAD_STACK_SIZE, 1);
    wiced_rtos_create_worker_thread(&rx_worker_thread, WICED_BT_UART_THREAD_PRIORITY, WICED_BT_UART_THREAD_STACK_SIZE, 1);

    // start receiving packet
    wiced_rtos_send_asynchronous_event(&rx_worker_thread, &h4_rx_worker_receive_packet, NULL);    

    // tx is ready
    tx_worker_data_size = 0;
    return 0;
}

static int h4_close(void){
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

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

// get h4 singleton
const hci_transport_t * hci_transport_h4_instance(const btstack_uart_block_t * uart_driver) {
    if (hci_transport_h4 == NULL) {
        hci_transport_h4 = (hci_transport_h4_t*)malloc( sizeof(hci_transport_h4_t));
        memset(hci_transport_h4, 0, sizeof(hci_transport_h4_t));
        hci_transport_h4->ds                                      = NULL;
        hci_transport_h4->transport.name                          = "H4_WICED";
        hci_transport_h4->transport.init                          = h4_init;
        hci_transport_h4->transport.open                          = h4_open;
        hci_transport_h4->transport.close                         = h4_close;
        hci_transport_h4->transport.register_packet_handler       = h4_register_packet_handler;
        hci_transport_h4->transport.can_send_packet_now           = h4_can_send_packet_now;
        hci_transport_h4->transport.send_packet                   = h4_send_packet;
        hci_transport_h4->transport.set_baudrate                  = h4_set_baudrate;
    }
    return (const hci_transport_t *) hci_transport_h4;
}
