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
 *  hci_h4_transport.c
 *
 *  HCI Transport API implementation for H4 protocol over POSIX with optional support for eHCILL
 *
 *  Created by Matthias Ringwald on 4/29/09.
 */

#include "btstack_config.h"

#include "btstack_debug.h"
#include "hci.h"
#include "hci_transport.h"
#include "btstack_uart_block.h"

#ifdef ENABLE_EHCILL

// eHCILL commands
#define EHCILL_GO_TO_SLEEP_IND 0x030
#define EHCILL_GO_TO_SLEEP_ACK 0x031
#define EHCILL_WAKE_UP_IND     0x032
#define EHCILL_WAKE_UP_ACK     0x033

static int  hci_transport_h4_ehcill_outgoing_packet_ready(void);
static int  hci_transport_h4_ehcill_sleep_mode_active(void);
static void hci_transport_h4_echill_send_wakeup_ind(void);
static void hci_transport_h4_ehcill_handle_command(uint8_t action);
static void hci_transport_h4_ehcill_handle_ehcill_command_sent(void);
static void hci_transport_h4_ehcill_handle_packet_sent(void);
static void hci_transport_h4_ehcill_open(void);
static void hci_transport_h4_ehcill_reset_statemachine(void);
static void hci_transport_h4_ehcill_send_ehcill_command(void);
static void hci_transport_h4_ehcill_sleep_ack_timer_setup(void);
static void hci_transport_h4_ehcill_trigger_wakeup(void);

typedef enum {
    EHCILL_STATE_SLEEP,
    EHCILL_STATE_W4_ACK,
    EHCILL_STATE_AWAKE
} EHCILL_STATE;

// eHCILL state machine
static EHCILL_STATE ehcill_state;
static uint8_t      ehcill_command_to_send;

static btstack_uart_sleep_mode_t btstack_uart_sleep_mode;

// work around for eHCILL problem
static btstack_timer_source_t ehcill_sleep_ack_timer;

#endif


// assert pre-buffer for packet type is available
#if !defined(HCI_OUTGOING_PRE_BUFFER_SIZE) || (HCI_OUTGOING_PRE_BUFFER_SIZE == 0)
#error HCI_OUTGOING_PRE_BUFFER_SIZE not defined. Please update hci.h
#endif

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 

typedef enum {
    H4_W4_PACKET_TYPE,
    H4_W4_EVENT_HEADER,
    H4_W4_ACL_HEADER,
    H4_W4_SCO_HEADER,
    H4_W4_PAYLOAD,
} H4_STATE;

typedef enum {
    TX_IDLE = 1,
    TX_W4_PACKET_SENT,
#ifdef ENABLE_EHCILL
    TX_W4_WAKEUP, 
    TX_W2_EHCILL_SEND,
    TX_W4_EHCILL_SENT,
#endif
} TX_STATE;

// UART Driver + Config
static const btstack_uart_block_t * btstack_uart;
static btstack_uart_config_t uart_config;

// write state
static TX_STATE tx_state;         
static uint8_t * tx_data;
static uint16_t  tx_len;   // 0 == no outgoing packet

static uint8_t packet_sent_event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

// packet reader state machine
static  H4_STATE h4_state;
static int bytes_to_read;
static int read_pos;

// incoming packet buffer
static uint8_t hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + 1 + HCI_PACKET_BUFFER_SIZE]; // packet type + max(acl header + acl payload, event header + event data)
static uint8_t * hci_packet = &hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];

static int hci_transport_h4_set_baudrate(uint32_t baudrate){
    log_info("hci_transport_h4_set_baudrate %u", baudrate);
    return btstack_uart->set_baudrate(baudrate);
}

static void hci_transport_h4_reset_statemachine(void){
    h4_state = H4_W4_PACKET_TYPE;
    read_pos = 0;
    bytes_to_read = 1;
}

static void hci_transport_h4_trigger_next_read(void){
    // log_info("hci_transport_h4_trigger_next_read: %u bytes", bytes_to_read);
    btstack_uart->receive_block(&hci_packet[read_pos], bytes_to_read);  
}

static void hci_transport_h4_block_read(void){

    read_pos += bytes_to_read;

    switch (h4_state) {
        case H4_W4_PACKET_TYPE:
            switch (hci_packet[0]){
                case HCI_EVENT_PACKET:
                    bytes_to_read = HCI_EVENT_HEADER_SIZE;
                    h4_state = H4_W4_EVENT_HEADER;
                    break;
                case HCI_ACL_DATA_PACKET:
                    bytes_to_read = HCI_ACL_HEADER_SIZE;
                    h4_state = H4_W4_ACL_HEADER;
                    break;
                case HCI_SCO_DATA_PACKET:
                    bytes_to_read = HCI_SCO_HEADER_SIZE;
                    h4_state = H4_W4_SCO_HEADER;
                    break;
#ifdef ENABLE_EHCILL
                case EHCILL_GO_TO_SLEEP_IND:
                case EHCILL_GO_TO_SLEEP_ACK:
                case EHCILL_WAKE_UP_IND:
                case EHCILL_WAKE_UP_ACK:
                    hci_transport_h4_ehcill_handle_command(hci_packet[0]);
                    hci_transport_h4_reset_statemachine();
                    break;
#endif
                default:
                    log_error("hci_transport_h4: invalid packet type 0x%02x", hci_packet[0]);
                    hci_transport_h4_reset_statemachine();
                    break;
            }
            break;
            
        case H4_W4_EVENT_HEADER:
            bytes_to_read = hci_packet[2];
            h4_state = H4_W4_PAYLOAD;
            break;
            
        case H4_W4_ACL_HEADER:
            bytes_to_read = little_endian_read_16( hci_packet, 3);
            // check ACL length
            if (HCI_ACL_HEADER_SIZE + bytes_to_read >  HCI_PACKET_BUFFER_SIZE){
                log_error("hci_transport_h4: invalid ACL payload len %u - only space for %u", bytes_to_read, HCI_PACKET_BUFFER_SIZE - HCI_ACL_HEADER_SIZE);
                hci_transport_h4_reset_statemachine();
                break;              
            }
            h4_state = H4_W4_PAYLOAD;
            break;
            
        case H4_W4_SCO_HEADER:
            bytes_to_read = hci_packet[3];
            h4_state = H4_W4_PAYLOAD;
            break;

        case H4_W4_PAYLOAD:
            packet_handler(hci_packet[0], &hci_packet[1], read_pos-1);
            hci_transport_h4_reset_statemachine();
            break;
        default:
            break;
    }
    hci_transport_h4_trigger_next_read();
}

static void hci_transport_h4_block_sent(void){
    switch (tx_state){
        case TX_W4_PACKET_SENT:
            // packet fully sent, reset state
            tx_len = 0;
            tx_state = TX_IDLE;

#ifdef ENABLE_EHCILL
            // notify eHCILL engine
            hci_transport_h4_ehcill_handle_packet_sent();
#endif
            // notify upper stack that it can send again
            packet_handler(HCI_EVENT_PACKET, &packet_sent_event[0], sizeof(packet_sent_event));
            break;

#ifdef ENABLE_EHCILL        
        case TX_W4_EHCILL_SENT: 
            hci_transport_h4_ehcill_handle_ehcill_command_sent();
            break;
#endif

        default:
            break;
    }
}

static int hci_transport_h4_can_send_now(uint8_t packet_type){
    return tx_state == TX_IDLE;
}

static int hci_transport_h4_send_packet(uint8_t packet_type, uint8_t * packet, int size){
    // store packet type before actual data and increase size
    size++;
    packet--;
    *packet = packet_type;

    // store request
    tx_len   = size;
    tx_data  = packet;

#ifdef ENABLE_EHCILL
    if (hci_transport_h4_ehcill_sleep_mode_active()){
        hci_transport_h4_ehcill_trigger_wakeup();
        return 0;
    }
#endif

    // start sending
    tx_state = TX_W4_PACKET_SENT;
    btstack_uart->send_block(packet, size);
    return 0;
}

static void hci_transport_h4_init(const void * transport_config){
    // check for hci_transport_config_uart_t
    if (!transport_config) {
        log_error("hci_transport_h4: no config!");
        return;
    }
    if (((hci_transport_config_t*)transport_config)->type != HCI_TRANSPORT_CONFIG_UART) {
        log_error("hci_transport_h4: config not of type != HCI_TRANSPORT_CONFIG_UART!");
        return;
    }

    // extract UART config from transport config
    hci_transport_config_uart_t * hci_transport_config_uart = (hci_transport_config_uart_t*) transport_config;
    uart_config.baudrate    = hci_transport_config_uart->baudrate_init;
    uart_config.flowcontrol = hci_transport_config_uart->flowcontrol;
    uart_config.device_name = hci_transport_config_uart->device_name;

    // setup UART driver
    btstack_uart->init(&uart_config);
    btstack_uart->set_block_received(&hci_transport_h4_block_read);
    btstack_uart->set_block_sent(&hci_transport_h4_block_sent);
}

static int hci_transport_h4_open(void){
    int res = btstack_uart->open();
    if (res){
        return res;
    }
    hci_transport_h4_reset_statemachine();
    hci_transport_h4_trigger_next_read();

    tx_state = TX_IDLE;

#ifdef ENABLE_EHCILL
    hci_transport_h4_ehcill_open();
#endif
    return 0;
}

static int hci_transport_h4_close(void){
    return btstack_uart->close();
}

static void hci_transport_h4_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

//
// --- main part of eHCILL implementation ---
//

#ifdef ENABLE_EHCILL

static void hci_transport_h4_ehcill_open(void){
    hci_transport_h4_ehcill_reset_statemachine();

    // find best sleep mode to use: wake on CTS, wake on RX, none
    btstack_uart_sleep_mode = BTSTACK_UART_SLEEP_OFF;
    int supported_sleep_modes = 0;
    if (btstack_uart->get_supported_sleep_modes){
        supported_sleep_modes = btstack_uart->get_supported_sleep_modes();
    }
    if (supported_sleep_modes & BTSTACK_UART_SLEEP_MASK_RTS_HIGH_WAKE_ON_CTS_PULSE){
        log_info("eHCILL: using wake on CTS");
        btstack_uart_sleep_mode = BTSTACK_UART_SLEEP_RTS_HIGH_WAKE_ON_CTS_PULSE;
    } else if (supported_sleep_modes & BTSTACK_UART_SLEEP_MASK_RTS_LOW_WAKE_ON_RX_EDGE){
        log_info("eHCILL: using wake on RX");
        btstack_uart_sleep_mode = BTSTACK_UART_SLEEP_RTS_LOW_WAKE_ON_RX_EDGE;
    } else {
        log_info("eHCILL: UART driver does not provide compatible sleep mode");
    }
}

static void hci_transport_h4_echill_send_wakeup_ind(void){
    // update state
    tx_state     = TX_W4_WAKEUP;
    ehcill_state = EHCILL_STATE_W4_ACK;
    ehcill_command_to_send = EHCILL_WAKE_UP_IND;
    btstack_uart->send_block(&ehcill_command_to_send, 1);
}

static int hci_transport_h4_ehcill_outgoing_packet_ready(void){
    return tx_len != 0;
}

static int  hci_transport_h4_ehcill_sleep_mode_active(void){
    return ehcill_state == EHCILL_STATE_SLEEP;
}

static void hci_transport_h4_ehcill_reset_statemachine(void){
    ehcill_state = EHCILL_STATE_AWAKE;
}

static void hci_transport_h4_ehcill_send_ehcill_command(void){
    log_debug("eHCILL: send command %02x", ehcill_command_to_send);
    tx_state = TX_W4_EHCILL_SENT;
    btstack_uart->send_block(&ehcill_command_to_send, 1);
}

static void hci_transport_h4_ehcill_sleep_ack_timer_handler(btstack_timer_source_t * timer){
    log_debug("eHCILL: timer triggered");
    hci_transport_h4_ehcill_send_ehcill_command();
}

static void hci_transport_h4_ehcill_sleep_ack_timer_setup(void){
    // setup timer
    log_debug("eHCILL: set timer for sending command");
    btstack_run_loop_set_timer_handler(&ehcill_sleep_ack_timer, &hci_transport_h4_ehcill_sleep_ack_timer_handler);
    btstack_run_loop_set_timer(&ehcill_sleep_ack_timer, 50);
    btstack_run_loop_add_timer(&ehcill_sleep_ack_timer);
}

static void hci_transport_h4_ehcill_trigger_wakeup(void){
    switch (tx_state){
        case TX_W2_EHCILL_SEND:
        case TX_W4_EHCILL_SENT:
            // wake up / sleep ack in progress, nothing to do now
            return;
        case TX_IDLE:
        default:
            // all clear, prepare for wakeup
            break;
    }
    // UART needed again
    if (btstack_uart_sleep_mode){
        btstack_uart->set_sleep(BTSTACK_UART_SLEEP_OFF);
    }
    hci_transport_h4_echill_send_wakeup_ind();
}

static void hci_transport_h4_ehcill_schedule_ecill_command(uint8_t command){
    ehcill_command_to_send = command;
    switch (tx_state){
        case TX_IDLE:
            if (ehcill_command_to_send == EHCILL_WAKE_UP_ACK){
                // send right away
                hci_transport_h4_ehcill_send_ehcill_command();
            } else {
                // change state so BTstack cannot send and setup timer
                tx_state = TX_W2_EHCILL_SEND;
                hci_transport_h4_ehcill_sleep_ack_timer_setup();
            }
            break;
        default:
            break;
    }
}

static void hci_transport_h4_ehcill_handle_command(uint8_t action){
    // log_info("hci_transport_h4_ehcill_handle: %x, state %u, defer_rx %u", action, ehcill_state, ehcill_defer_rx_size);
    switch(ehcill_state){
        case EHCILL_STATE_AWAKE:
            switch(action){
                case EHCILL_GO_TO_SLEEP_IND:
                    ehcill_state = EHCILL_STATE_SLEEP;
                    log_info("eHCILL: GO_TO_SLEEP_IND RX");
                    hci_transport_h4_ehcill_schedule_ecill_command(EHCILL_GO_TO_SLEEP_ACK);
                    break;
                default:
                    break;
            }
            break;
            
        case EHCILL_STATE_SLEEP:
            switch(action){
                case EHCILL_WAKE_UP_IND:
                    ehcill_state = EHCILL_STATE_AWAKE;
                    log_info("eHCILL: WAKE_UP_IND RX");
                    hci_transport_h4_ehcill_schedule_ecill_command(EHCILL_WAKE_UP_ACK);
                    break;
                    
                default:
                    break;
            }
            break;
            
        case EHCILL_STATE_W4_ACK:
            switch(action){
                case EHCILL_WAKE_UP_IND:
                case EHCILL_WAKE_UP_ACK:
                    log_info("eHCILL: WAKE_UP_IND or ACK");
                    tx_state = TX_W4_PACKET_SENT;
                    ehcill_state = EHCILL_STATE_AWAKE;
                    btstack_uart->send_block(tx_data, tx_len);
                    break;
                default:
                    break;
            }
            break;
    }
}

static void hci_transport_h4_ehcill_handle_packet_sent(void){
    // now, send pending ehcill command if neccessary
    switch (ehcill_command_to_send){
        case EHCILL_GO_TO_SLEEP_ACK:
            hci_transport_h4_ehcill_sleep_ack_timer_setup();
            break;
        case EHCILL_WAKE_UP_IND:
            hci_transport_h4_ehcill_send_ehcill_command();
            break;
        default:
            break;
    }
}

static void hci_transport_h4_ehcill_handle_ehcill_command_sent(void){
    tx_state = TX_IDLE;
    int command = ehcill_command_to_send;
    ehcill_command_to_send = 0;
    if (command == EHCILL_GO_TO_SLEEP_ACK) {
        log_info("eHCILL: GO_TO_SLEEP_ACK sent, enter sleep mode");
        // UART not needed after EHCILL_GO_TO_SLEEP_ACK was sent
        if (btstack_uart_sleep_mode != BTSTACK_UART_SLEEP_OFF){
            btstack_uart->set_sleep(btstack_uart_sleep_mode);
        }
    }
    // already packet ready? then start wakeup
    if (hci_transport_h4_ehcill_outgoing_packet_ready()){
        if (btstack_uart_sleep_mode){
            btstack_uart->set_sleep(BTSTACK_UART_SLEEP_OFF);
        }
        hci_transport_h4_echill_send_wakeup_ind();
    }
}

#endif
// --- end of eHCILL implementation ---------

static const hci_transport_t hci_transport_h4 = {
    /* const char * name; */                                        "H4",
    /* void   (*init) (const void *transport_config); */            &hci_transport_h4_init,
    /* int    (*open)(void); */                                     &hci_transport_h4_open,
    /* int    (*close)(void); */                                    &hci_transport_h4_close,
    /* void   (*register_packet_handler)(void (*handler)(...); */   &hci_transport_h4_register_packet_handler,
    /* int    (*can_send_packet_now)(uint8_t packet_type); */       &hci_transport_h4_can_send_now,
    /* int    (*send_packet)(...); */                               &hci_transport_h4_send_packet,
    /* int    (*set_baudrate)(uint32_t baudrate); */                &hci_transport_h4_set_baudrate,
    /* void   (*reset_link)(void); */                               NULL,
};

// configure and return h4 singleton
const hci_transport_t * hci_transport_h4_instance(const btstack_uart_block_t * uart_driver) {
    btstack_uart = uart_driver;
    return &hci_transport_h4;
}
