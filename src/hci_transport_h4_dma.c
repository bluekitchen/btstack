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
 *  hci_h4_transport_dma.c
 *
 *  HCI Transport implementation for basic H4 protocol for blocking UART write and IRQ-driven blockwise RX
 *
 *  Created by Matthias Ringwald on 4/29/09.
 */


#include "debug.h"
#include "hci.h"
#include "hci_transport.h"

#include <btstack/hal_uart_dma.h>

typedef enum {
    H4_W4_PACKET_TYPE,
    H4_W4_EVENT_HEADER,
    H4_W4_ACL_HEADER,
    H4_W4_PAYLOAD,
    H4_PACKET_RECEIVED
} H4_STATE;

typedef struct hci_transport_h4 {
    hci_transport_t transport;
    data_source_t *ds;
} hci_transport_h4_t;

// single instance
static hci_transport_h4_t * hci_transport_h4 = NULL;

static int  h4_process(struct data_source *ds);
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 
static      hci_uart_config_t *hci_uart_config;

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

// packet reader state machine
static  H4_STATE h4_state;
static int read_pos;
static int bytes_to_read;
static uint8_t hci_packet[HCI_ACL_3DH5_SIZE]; // bigger than largest packet

static void h4_init_sm(void){
    h4_state = H4_W4_PACKET_TYPE;
    read_pos = 0;
    bytes_to_read = 1;
    hal_uart_dma_receive_block(hci_packet, bytes_to_read);
}

// prototypes

static void h4_block_received(void){

    read_pos += bytes_to_read;

    // act
    switch (h4_state) {
        case H4_W4_PACKET_TYPE:
            switch (hci_packet[0]) {
                case HCI_ACL_DATA_PACKET:
                    h4_state = H4_W4_ACL_HEADER;
                    bytes_to_read = HCI_ACL_DATA_PKT_HDR;
                    break;
                case HCI_EVENT_PACKET:
                    h4_state = H4_W4_EVENT_HEADER;
                    bytes_to_read = HCI_EVENT_PKT_HDR;
                    break;
                default:
                    log_err("h4_process: invalid packet type 0x%02x\r\n", hci_packet[0]);
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

static int    h4_open(void *transport_config){

    hci_uart_config = (hci_uart_config_t*) transport_config;
	
	// open uart
	hal_uart_dma_init();
    hal_uart_dma_set_block_received(h4_block_received);
    
	// set up data_source
    hci_transport_h4->ds = malloc(sizeof(data_source_t));
    if (!hci_transport_h4) return -1;
    // hci_transport_h4->ds->fd = fd;
    hci_transport_h4->ds->process = h4_process;
    run_loop_add_data_source(hci_transport_h4->ds);
    
    //
    h4_init_sm();

    return 0;
}

static int    h4_close(){
    // first remove run loop handler
	run_loop_remove_data_source(hci_transport_h4->ds);
    
    // close device 
    // ...
    
    // free struct
    free(hci_transport_h4->ds);
    
    hci_transport_h4->ds = NULL;
    return 0;
}

static int h4_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    
    // log packet
    
	// h4 packet type + actual packet
	hal_uart_dma_send_byte(packet_type);
    hal_uart_dma_send_block(packet, size);
    return 0;
}

// static
void   h4_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

static int h4_process(struct data_source *ds) {
    
    if (h4_state == H4_PACKET_RECEIVED) {
        
        // log packet
        
        packet_handler(hci_packet[0], &hci_packet[1], read_pos-1);
        
        h4_init_sm();
    }
                                
    return 0;
}

static const char * h4_get_transport_name(){
    return "H4_DMA";
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

// get h4 singleton
hci_transport_t * hci_transport_h4_dma_instance() { 
    if (hci_transport_h4 == NULL) {
        hci_transport_h4 = malloc(sizeof(hci_transport_h4_t));
        hci_transport_h4->ds                                      = NULL;
        hci_transport_h4->transport.open                          = h4_open;
        hci_transport_h4->transport.close                         = h4_close;
        hci_transport_h4->transport.send_packet                   = h4_send_packet;
        hci_transport_h4->transport.register_packet_handler       = h4_register_packet_handler;
        hci_transport_h4->transport.get_transport_name            = h4_get_transport_name;
    }
    return (hci_transport_t *) hci_transport_h4;
}
