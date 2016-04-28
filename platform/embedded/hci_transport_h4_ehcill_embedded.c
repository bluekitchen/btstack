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
 *  hci_h4_transport_dma.c
 *
 *  HCI Transport implementation of H4 protocol with eHCILL support
 *  for IRQ-driven blockwise RX and TX, and IRQ callback on CTS toggle
 *
 *  Based on information found at http://e2e.ti.com/support/low_power_rf/f/660/t/134855.aspx
 *
 *  Created by Matthias Ringwald on 9/16/11.
 */

#include "btstack_config.h"

#include <stdio.h>
#include <string.h>

#include "btstack_debug.h"
#include "hci.h"
#include "hci_transport.h"
#include "btstack_run_loop.h"
#include "btstack_run_loop_embedded.h"

#include "hal_uart_dma.h"

// assert pre-buffer for packet type is available
#if !defined(HCI_OUTGOING_PRE_BUFFER_SIZE) || (HCI_OUTGOING_PRE_BUFFER_SIZE == 0)
#error HCI_OUTGOING_PRE_BUFFER_SIZE not defined. Please update hci.h
#endif

// #define LOG_EHCILL

// eHCILL commands (+interal CTS signal)
#define EHCILL_GO_TO_SLEEP_IND 0x030
#define EHCILL_GO_TO_SLEEP_ACK 0x031
#define EHCILL_WAKE_UP_IND     0x032
#define EHCILL_WAKE_UP_ACK     0x033
#define EHCILL_CTS_SIGNAL      0x034

typedef enum {
    H4_W4_PACKET_TYPE = 1,
    H4_W4_EVENT_HEADER,
    H4_W4_ACL_HEADER,
    H4_W4_PAYLOAD,
    H4_PACKET_RECEIVED
} H4_STATE;

typedef enum {
    TX_IDLE = 1,
    TX_W4_WAKEUP,   // eHCILL only
    TX_W4_PACKET_SENT,
    TX_W2_EHCILL_SEND,
    TX_W4_EHCILL_SENT,
    TX_DONE
} TX_STATE;

typedef enum {
    EHCILL_STATE_SLEEP = 1,
    EHCILL_STATE_W4_ACK,
    EHCILL_STATE_AWAKE
} EHCILL_STATE;

typedef struct hci_transport_h4 {
    hci_transport_t transport;
    btstack_data_source_t *ds;
} hci_transport_h4_t;

// prototypes
static void h4_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type);
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 
static void h4_block_received(void);
static void h4_block_sent(void);
static int  h4_open(void);
static int  h4_close(void);
static void h4_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size));
static int  h4_can_send_packet_now(uint8_t packet_type);
static int  h4_set_baudrate(uint32_t baudrate);

static int  ehcill_send_packet(uint8_t packet_type, uint8_t *packet, int size);
static void ehcill_uart_dma_receive_block(uint8_t *buffer, uint16_t size);
static int  ehcill_sleep_mode_active(void);
static void ehcill_handle(uint8_t action);
static void ehcill_cts_irq_handler(void);

// eCHILL: state machine
static EHCILL_STATE ehcill_state = EHCILL_STATE_AWAKE;
static uint8_t *    ehcill_defer_rx_buffer;
static uint16_t     ehcill_defer_rx_size = 0; 
static uint8_t      ehcill_command_to_send;

// H4: packet reader state machine
static volatile  H4_STATE  h4_state;
static int read_pos;
static int bytes_to_read;

 // bigger than largest packet
static uint8_t hci_packet_prefixed[HCI_INCOMING_PRE_BUFFER_SIZE + HCI_PACKET_BUFFER_SIZE];
static uint8_t * hci_packet = &hci_packet_prefixed[HCI_INCOMING_PRE_BUFFER_SIZE];

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

// H4: tx state
static volatile TX_STATE  tx_state;				// updated from block_sent callback
static volatile int       tx_send_packet_sent;	// updated from block_sent callback
static uint8_t * tx_data;
static uint16_t  tx_len;                        // 0 == no outgoing packet

// work around for eHCILL problem
static btstack_timer_source_t ehcill_sleep_ack_timer;

// data source used in run_loop
static btstack_data_source_t hci_transport_h4_dma_ds;

// hci_transport for use by hci
static const hci_transport_h4_t hci_transport_h4_ehcill_dma = {
    {
  /*  .transport.name                          = */  "H4_EHCILL_EMBEDDED",
  /*  .transport.init                          = */  NULL,
  /*  .transport.open                          = */  h4_open,
  /*  .transport.close                         = */  h4_close,
  /*  .transport.register_packet_handler       = */  h4_register_packet_handler,
  /*  .transport.can_send_packet_now           = */  h4_can_send_packet_now,
  /*  .transport.send_packet                   = */  ehcill_send_packet,
  /*  .transport.set_baudrate                  = */  h4_set_baudrate,
  /*  .transport.reset_link                    = */  NULL,    
    },
  /*  .ds                                      = */  &hci_transport_h4_dma_ds
};


static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

// get h4 singleton
const hci_transport_t * hci_transport_h4_instance(const btstack_uart_block_t * uart_driver){ 
    return &hci_transport_h4_ehcill_dma.transport;
}

static void h4_rx_init_sm(void){
    h4_state = H4_W4_PACKET_TYPE;
    read_pos = 0;
    bytes_to_read = 1;
    ehcill_uart_dma_receive_block(hci_packet, bytes_to_read);
}

static void h4_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

static int h4_open(void){

	// open uart
	hal_uart_dma_init();
    hal_uart_dma_set_block_received(h4_block_received);
    hal_uart_dma_set_block_sent(h4_block_sent);
    hal_uart_dma_set_csr_irq_handler(ehcill_cts_irq_handler);
    
	// set up data_source
    btstack_run_loop_set_data_source_handler(&hci_transport_h4_dma_ds, &h4_process);
    btstack_run_loop_enable_data_source_callbacks(&hci_transport_h4_dma_ds, DATA_SOURCE_CALLBACK_POLL);
    btstack_run_loop_add_data_source(&hci_transport_h4_dma_ds);
    
    // init state machines
    h4_rx_init_sm();
    tx_state = TX_IDLE;
    tx_len = 0;
    
    return 0;
}

static int h4_set_baudrate(uint32_t baudrate){
    log_info("h4_set_baudrate - set baud %lu", baudrate);
    return hal_uart_dma_set_baud(baudrate);
}

static int h4_close(void){
    // first remove run loop handler
	btstack_run_loop_remove_data_source(&hci_transport_h4_dma_ds);
    
    // stop IRQ
    hal_uart_dma_set_csr_irq_handler(NULL);
    
    // close device 
    // ...
    return 0;
}

// potentially called from ISR context
static void h4_block_received(void){
    
	read_pos += bytes_to_read;
    
    // act
    switch (h4_state) {
        case H4_W4_PACKET_TYPE:
            switch (hci_packet[0]) {
                case HCI_ACL_DATA_PACKET:
                    h4_state = H4_W4_ACL_HEADER;
                    bytes_to_read = HCI_ACL_HEADER_SIZE;
                    break;
                case HCI_EVENT_PACKET:
                    h4_state = H4_W4_EVENT_HEADER;
                    bytes_to_read = HCI_EVENT_HEADER_SIZE;
                    break;
                case EHCILL_GO_TO_SLEEP_IND:
                case EHCILL_GO_TO_SLEEP_ACK:
                case EHCILL_WAKE_UP_IND:
                case EHCILL_WAKE_UP_ACK:
                    ehcill_handle(hci_packet[0]);
                    h4_rx_init_sm();
                    return;
                default:
                    log_error("h4_process: invalid packet type 0x%02x", hci_packet[0]);
                    h4_rx_init_sm();
                    return;
            }
            break;
            
        case H4_W4_EVENT_HEADER:
            bytes_to_read = hci_packet[2];
            // check ACL length
            if (HCI_ACL_HEADER_SIZE + bytes_to_read >  HCI_PACKET_BUFFER_SIZE){
                log_error("h4_process: invalid ACL payload len %u - only space for %u", bytes_to_read, HCI_PACKET_BUFFER_SIZE - HCI_ACL_HEADER_SIZE);
                h4_rx_init_sm();
                return;              
            }
            if (bytes_to_read) {
                h4_state = H4_W4_PAYLOAD;
                break;
            }
            h4_state = H4_PACKET_RECEIVED; 
            break;
            
        case H4_W4_ACL_HEADER:
            bytes_to_read = little_endian_read_16( hci_packet, 3);
            if (bytes_to_read) {
                h4_state = H4_W4_PAYLOAD;
                break;
            }
            h4_state = H4_PACKET_RECEIVED; 
            break;
            
        case H4_W4_PAYLOAD:
            h4_state = H4_PACKET_RECEIVED;
            bytes_to_read = 0;
            
            // trigger run loop - necessary for use in low power modes
            btstack_run_loop_embedded_trigger();
            break;
            
        default:
            bytes_to_read = 0;
            break;
    }

    
    // read next block
    if (bytes_to_read) {
        ehcill_uart_dma_receive_block(&hci_packet[read_pos], bytes_to_read);
    }
    // gpio_clear(GPIOB, GPIO_DEBUG_0);
}

static int h4_can_send_packet_now(uint8_t packet_type){
    return tx_state == TX_IDLE;
}

static int h4_outgoing_packet_ready(void){
    return tx_len != 0;
}

static void ehcill_sleep_ack_timer_handler(btstack_timer_source_t * timer){
    tx_state = TX_W4_EHCILL_SENT;
    hal_uart_dma_send_block(&ehcill_command_to_send, 1);    
}

static void ehcill_sleep_ack_timer_setup(void){
    // setup timer
    ehcill_sleep_ack_timer.process = &ehcill_sleep_ack_timer_handler;
    btstack_run_loop_set_timer(&ehcill_sleep_ack_timer, 50);
    btstack_run_loop_add_timer(&ehcill_sleep_ack_timer);
    btstack_run_loop_embedded_trigger();    
}

static void ehcill_reactivate_rx(void){
	if (!ehcill_defer_rx_size){
		log_error("EHCILL: NO RX REQUEST PENDING");
		return;
	}
	#ifdef LOG_EHCILL
	log_info ("EHCILL: Re-activate rx");
	#endif
	// receive request, clears RTS
	int rx_size = ehcill_defer_rx_size;
	ehcill_defer_rx_size = 0;
	hal_uart_dma_receive_block(ehcill_defer_rx_buffer, rx_size);
}

static void echill_send_wakeup_ind(void){
	// update state
	tx_state     = TX_W4_WAKEUP;
	ehcill_state = EHCILL_STATE_W4_ACK;
	ehcill_command_to_send = EHCILL_WAKE_UP_IND;
	hal_uart_dma_send_block(&ehcill_command_to_send, 1);
}

// potentially called from ISR context
static void h4_block_sent(void){
    int command;
    switch (tx_state){
        case TX_W4_PACKET_SENT:
            // packet fully sent, reset state
            tx_len = 0;
            // now, send pending ehcill command if neccessary
            switch (ehcill_command_to_send){
                case EHCILL_GO_TO_SLEEP_ACK:
                    ehcill_sleep_ack_timer_setup();
		            tx_send_packet_sent = 1;
                    break;
                case EHCILL_WAKE_UP_IND:
                    tx_state = TX_W4_EHCILL_SENT;
                    // gpio_clear(GPIOB, GPIO_DEBUG_1);
                    hal_uart_dma_send_block(&ehcill_command_to_send, 1);
		            tx_send_packet_sent = 1;
                    break;
                default:
                    // trigger run loop
                    tx_state = TX_DONE;
		            tx_send_packet_sent = 1;
                    btstack_run_loop_embedded_trigger();
                    break;
            }
            break;
        case TX_W4_EHCILL_SENT:
            tx_state = TX_DONE;
            command = ehcill_command_to_send;
            ehcill_command_to_send = 0;
            if (command == EHCILL_GO_TO_SLEEP_ACK) {
                // UART not needed after EHCILL_GO_TO_SLEEP_ACK was sent
                hal_uart_dma_set_sleep(1);
            }
            if (h4_outgoing_packet_ready()){
                // already packet ready? then start wakeup
                hal_uart_dma_set_sleep(0);
                ehcill_reactivate_rx();
                echill_send_wakeup_ind();
            }
            // trigger run loop
            btstack_run_loop_embedded_trigger();
            break;
        default:
            break;
    }
}

static void h4_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    
    // reset tx state before emitting packet sent event
    // to allow for positive can_send_now
    if (tx_state == TX_DONE){
        tx_state = TX_IDLE;
    }

    // notify about packet sent
    if (tx_send_packet_sent){
		// race condition: if tx_state was set after our check, can_send_now will fail, causing a hang
		// workaround: assert that tx_state is TX_IDLE if it was just set to done by ISR
		if (tx_state == TX_DONE){
			tx_state = TX_IDLE;
		}
		
        tx_send_packet_sent = 0;
        uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0 };
        packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
    }

    if (h4_state != H4_PACKET_RECEIVED) return;

    packet_handler(hci_packet[0], &hci_packet[1], read_pos-1);

    h4_rx_init_sm();
}

//////////////////////////


int  ehcill_sleep_mode_active(void){
    return ehcill_state == EHCILL_STATE_SLEEP;
}

static void ehcill_cts_irq_handler(void){
    ehcill_handle(EHCILL_CTS_SIGNAL);
}

static void ehcill_schedule_ecill_command(uint8_t command){
    ehcill_command_to_send = command;
    switch (tx_state){
        case TX_IDLE:
        case TX_DONE:
            if (ehcill_command_to_send == EHCILL_WAKE_UP_ACK){
                // send right away
                tx_state = TX_W4_EHCILL_SENT;
                hal_uart_dma_send_block(&ehcill_command_to_send, 1);        
                break;
            }
            // change state so BTstack cannot send and setup timer
            tx_state = TX_W2_EHCILL_SEND;
            ehcill_sleep_ack_timer_setup();
            break;
        default:
            break;
    }
}

static void ehcill_handle(uint8_t action){
    // log_info("ehcill_handle: %x, state %u, defer_rx %u", action, ehcill_state, ehcill_defer_rx_size);
    switch(ehcill_state){
        case EHCILL_STATE_AWAKE:
            switch(action){
                case EHCILL_GO_TO_SLEEP_IND:
                    
                    // 1. set RTS high - already done by BT RX ISR
                    // 2. enable CTS   - CTS always enabled
                    
                    ehcill_state = EHCILL_STATE_SLEEP;
#ifdef LOG_EHCILL
                    log_info("EHCILL: GO_TO_SLEEP_IND RX");
#endif
                    // gpio_set(GPIOB, GPIO_DEBUG_1);

                    ehcill_schedule_ecill_command(EHCILL_GO_TO_SLEEP_ACK);
                    break;
                    
                default:
                    break;
            }
            break;
            
        case EHCILL_STATE_SLEEP:
            switch(action){
                case EHCILL_CTS_SIGNAL:
                    // UART needed again
                    hal_uart_dma_set_sleep(0);
                    // and continue to receive
                    ehcill_reactivate_rx();
                    break;
                    
                case EHCILL_WAKE_UP_IND:
                    ehcill_state = EHCILL_STATE_AWAKE;
#ifdef LOG_EHCILL
                    log_info("EHCILL: WAKE_UP_IND RX");
#endif
                    ehcill_schedule_ecill_command(EHCILL_WAKE_UP_ACK);
                    break;
                    
                default:
                    break;
            }
            break;
            
        case EHCILL_STATE_W4_ACK:
            switch(action){
                case EHCILL_WAKE_UP_IND:
                case EHCILL_WAKE_UP_ACK:
#ifdef LOG_EHCILL
                    log_info("EHCILL: WAKE_UP_IND or ACK");
#endif
                    tx_state = TX_W4_PACKET_SENT;
                    hal_uart_dma_send_block(tx_data, tx_len);
                    ehcill_state = EHCILL_STATE_AWAKE;
                    
                    break;
                    
                default:
                    break;
            }
            break;
    }
}

static int ehcill_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    
    // check for ongoing write
    switch (tx_state){
        case TX_W4_WAKEUP:
        case TX_W4_PACKET_SENT:
            // we should not arrive here, log state
            log_error("h4_send_packet with tx_state = %u, ehcill_state %u, packet type %u, data %02x %02x %02x", tx_state, ehcill_state, packet_type, packet[0], packet[1], packet[2]);
            return -1;
        case TX_IDLE:
        case TX_DONE:
        case TX_W2_EHCILL_SEND:
        case TX_W4_EHCILL_SENT:
            break;
    }
    
    // store packet type before actual data and increase size
    size++;
    packet--;
    *packet = packet_type;

    // store request
    tx_data = packet;
    tx_len  = size;
    
    // if awake, just start sending 
    if (!ehcill_sleep_mode_active()){
        tx_state = TX_W4_PACKET_SENT;
        hal_uart_dma_send_block(tx_data, tx_len);
        return 0;
    }

    switch (tx_state){
        case TX_W2_EHCILL_SEND:
        case TX_W4_EHCILL_SENT:
            // wake up / sleep ack in progress, nothing to do now
            return 0;
        case TX_IDLE:
        case TX_DONE:
        default:
            // all clear, prepare for wakeup
            break;
    }
    
    // UART needed again
    hal_uart_dma_set_sleep(0);
    ehcill_reactivate_rx();

    // start wakeup
    echill_send_wakeup_ind();

    return 0;
}

void ehcill_uart_dma_receive_block(uint8_t *buffer, uint16_t size){
    if (!ehcill_sleep_mode_active()){
        ehcill_defer_rx_size = 0;
        hal_uart_dma_receive_block(buffer, size);
        return;
    }
    
    // store receive request for later
    ehcill_defer_rx_buffer = buffer;
    ehcill_defer_rx_size   = size;
}
