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
 *  HCI Transport implementation for basic H4 protocol for blocking UART write and IRQ-driven blockwise RX
 *
 *  Created by Matthias Ringwald on 4/29/09.
 */

#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "hci.h"
#include "hci_transport.h"
#include <btstack/run_loop.h>

#include <btstack/hal_uart_dma.h>

typedef enum {
    H4_W4_PACKET_TYPE = 1,
    H4_W4_EVENT_HEADER,
    H4_W4_ACL_HEADER,
    H4_W4_PAYLOAD,
    H4_PACKET_RECEIVED
} H4_STATE;

typedef enum {
    TX_IDLE = 1,
    TX_W4_HEADER_SENT,
    TX_W4_PACKET_SENT,
    TX_DONE
} TX_STATE;

typedef struct hci_transport_h4 {
    hci_transport_t transport;
    data_source_t *ds;
} hci_transport_h4_t;

// prototypes
static int  h4_process(struct data_source *ds);
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 
static void h4_block_received(void);
static void h4_block_sent(void);
static int h4_open(void *transport_config);
static int h4_close();
static void h4_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size));
static int h4_send_packet(uint8_t packet_type, uint8_t *packet, int size);
static const char * h4_get_transport_name();
static int h4_set_baudrate(uint32_t baudrate);
static int h4_can_send_packet_now(uint8_t packet_type);

// packet reader state machine
static  H4_STATE h4_state;
static int read_pos;
static int bytes_to_read;

 // bigger than largest packet
static uint8_t hci_packet_prefixed[HCI_INCOMING_PRE_BUFFER_SIZE + HCI_PACKET_BUFFER_SIZE];
static uint8_t * hci_packet = &hci_packet_prefixed[HCI_INCOMING_PRE_BUFFER_SIZE];

// tx state
static TX_STATE tx_state;
static uint8_t  tx_packet_type;
static uint8_t *tx_data;
static uint16_t tx_len;

// static hci_transport_h4_t * hci_transport_h4 = NULL;
static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

static data_source_t hci_transport_h4_dma_ds = {
  /*  .item    = */  { NULL, NULL },
  /*  .fd      = */  0,
  /*  .process = */  h4_process
};

static hci_transport_h4_t hci_transport_h4_dma = {
    {
  /*  .transport.open                          = */  h4_open,
  /*  .transport.close                         = */  h4_close,
  /*  .transport.send_packet                   = */  h4_send_packet,
  /*  .transport.register_packet_handler       = */  h4_register_packet_handler,
  /*  .transport.get_transport_name            = */  h4_get_transport_name,
  /*  .transport.set_baudrate                  = */  h4_set_baudrate,
  /*  .transport.can_send_packet_now           = */  h4_can_send_packet_now,
    },
  /*  .ds                                      = */  &hci_transport_h4_dma_ds
};

static void h4_init_sm(void){
    h4_state = H4_W4_PACKET_TYPE;
    read_pos = 0;
    bytes_to_read = 1;
    hal_uart_dma_receive_block(hci_packet, bytes_to_read);
}


static int h4_open(void *transport_config){

	// open uart
	hal_uart_dma_init();
    hal_uart_dma_set_block_received(h4_block_received);
    hal_uart_dma_set_block_sent(h4_block_sent);
    
	// set up data_source
    run_loop_add_data_source(&hci_transport_h4_dma_ds);
    
    //
    h4_init_sm();
    tx_state = TX_IDLE;

    return 0;
}

static int h4_close(){
    // first remove run loop handler
	run_loop_remove_data_source(&hci_transport_h4_dma_ds);
    
    // close device 
    // ...
    return 0;
}

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
                default:
                    log_error("h4_process: invalid packet type 0x%02x", hci_packet[0]);
                    read_pos = 0;
                    h4_state = H4_W4_PACKET_TYPE;
                    bytes_to_read = 1;
                    break;
            }
            break;
            
        case H4_W4_EVENT_HEADER:
            bytes_to_read = hci_packet[2];
            if (bytes_to_read == 0) {
                h4_state = H4_PACKET_RECEIVED; 
                break;
            }
            h4_state = H4_W4_PAYLOAD;
            break;
            
        case H4_W4_ACL_HEADER:
            bytes_to_read = READ_BT_16( hci_packet, 3);
            if (bytes_to_read == 0) {
                h4_state = H4_PACKET_RECEIVED; 
                break;
            }
            h4_state = H4_W4_PAYLOAD;
            break;
            
        case H4_W4_PAYLOAD:
            h4_state = H4_PACKET_RECEIVED;
            bytes_to_read = 0;
            // trigger run loop
            embedded_trigger();
            break;
            
        default:
            bytes_to_read = 0;
            break;
    }
    
    // read next block
    if (bytes_to_read) {
        hal_uart_dma_receive_block(&hci_packet[read_pos], bytes_to_read);
    }
}

static void h4_block_sent(void){
    switch (tx_state){
        case TX_W4_HEADER_SENT:
            tx_state = TX_W4_PACKET_SENT;
            // h4 packet type + actual packet
            hal_uart_dma_send_block(tx_data, tx_len);
            break;
        case TX_W4_PACKET_SENT:
            tx_state = TX_DONE;
            // trigger run loop
            embedded_trigger();
            break;
        default:
            break;
    }
}

static void h4_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

static int h4_process(struct data_source *ds) {
    
    // notify about packet sent
    if (tx_state == TX_DONE){
        // reset state
        tx_state = TX_IDLE;
        uint8_t event[] = { DAEMON_EVENT_HCI_PACKET_SENT, 0 };
        packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
    }

    if (h4_state != H4_PACKET_RECEIVED) return 0;
            
    packet_handler(hci_packet[0], &hci_packet[1], read_pos-1);

    h4_init_sm();
                                
    return 0;
}

static int h4_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    
    // write in progress
    if (tx_state != TX_IDLE) {
        log_error("h4_send_packet with tx_state = %u, type %u, data %02x %02x %02x", tx_state, packet_type, packet[0], packet[1], packet[2]);
        return -1;
    }
    
    tx_packet_type = packet_type;
    tx_data = packet;
    tx_len  = size;
    
    tx_state = TX_W4_HEADER_SENT;
	hal_uart_dma_send_block(&tx_packet_type, 1);
    
    return 0;
}

static int h4_set_baudrate(uint32_t baudrate){
    log_info("h4_set_baudrate - set baud %lu", baudrate);
    return hal_uart_dma_set_baud(baudrate);
}

static int h4_can_send_packet_now(uint8_t packet_type){
    return tx_state == TX_IDLE;

}

static const char * h4_get_transport_name(){
    return "H4_DMA";
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

// get h4 singleton
hci_transport_t * hci_transport_h4_dma_instance() { 
    return (hci_transport_t *) &hci_transport_h4_dma;
}
