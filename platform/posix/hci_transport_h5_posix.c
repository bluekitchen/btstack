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
 *  hci_transport_h5.c
 *
 *  HCI Transport API implementation for basic H5 protocol
 *
 *  Created by Matthias Ringwald on 4/29/09.
 */

#include <termios.h>  /* POSIX terminal control definitions */
#include <fcntl.h>    /* File control definitions */
#include <unistd.h>   /* UNIX standard function definitions */
#include <stdio.h>
#include <string.h>

#include "hci.h"
#include "hci_transport.h"

#ifdef HAVE_EHCILL
#error "HCI Transport H5 POSIX does not support eHCILL. Please either use HAVE_EHCILL or H5 Transport"
#endif 

/// newer

// h5 slip state machine
typedef enum {
    SLIP_UNKNOWN = 1,
    SLIP_DECODING,
    SLIP_X_C0,
    SLIP_X_DB
} hci_transport_slip_state_t;

static const uint8_t slip_c0_encoded[] = { 0xdb, 0xdc };
static const uint8_t slip_db_encoded[] = { 0xdb, 0xdd };

static uint8_t   hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + 1 + HCI_PACKET_BUFFER_SIZE]; // packet type + max(acl header + acl payload, event header + event data)

static hci_transport_slip_state_t slip_state;
static uint8_t * slip_payload = &hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];
static int       slip_payload_pos;
static uint8_t   slip_header[4];
static int       slip_header_pos;

static hci_transport_config_uart_t * hci_transport_config_uart;
static hci_transport_t *             h5_hci_transport;
static btstack_data_source_t *       h5_data_source;

// Prototypes
static void hci_transport_h5_process_frame(void);

// Generic helper
static void hci_transport_h5_send_really(uint8_t * data, int size){
    while (size > 0) {
        int bytes_written = write(hci_transport_h5_data_source->fd, data, size);
        if (bytes_written < 0) {
            usleep(5000);
            continue;
        }
        data += bytes_written;
        size -= bytes_written;
    }
}

// SLIP 
static inline void hci_transport_slip_send_eof(void){
    uint8_t data = 0xc0;
    hci_transport_h5_send_really(&data, 1);
}

static inline void hci_transport_slip_send_byte(uint8_t data){
    switch (data){
        case 0xc0:
            hci_transport_h5_send_really(slip_c0_encoded, sizeof(slip_c0_encoded));
            break;
        case 0xdb:
            hci_transport_h5_send_really(slip_db_encoded, sizeof(slip_db_encoded));
            break;
        default:
            hci_transport_h5_send_really(&data, 1);
            break;
    }
}

static inline void hci_transport_slip_send_block(uint8_t * data, uint16_t size){
    uint16_t i;
    for (i=0;i<size;i++){
        hci_transport_slip_send_byte(data++);
    }
}

// format: 0xc0 HEADER PACKER 0xc0
static void hci_transport_slip_send_frame(uint32_t header, uint8_t * packet, uint16_t packet_size){
    // reset data integrity flag
        if (header & 0x040){
        log_error("hci_transport_slip_send_frame: data integrity not supported, dropping flag");
        header &= ~0x40;
    }
    
    // Start of Frame
    hci_transport_slip_send_eof();

    // Header
    uint8_t header_buffer[4];
    little_endian_store_32(header_buffer, 0, header);
    hci_transport_slip_send_block(header_buffer, sizeof(header_buffer));

    // Packet
    hci_transport_slip_send_block(packet, packet_size);

    // Endo of frame
    hci_transport_slip_send_eof();
}

static void hci_transport_h5_slip_reset(void){
    slip_header_pos = 0;
    slip_payload_pos = 0;
}

static void hci_transport_h5_slip_init(void){
    slip_state = SLIP_UNKNOWN;
    hci_transport_h5_slip_reset();
}

static void hci_transport_h5_slip_store_byte(uint8_t input){
    if (slip_header_pos < 4){
        slip_header[slip_header_pos++] = input;
        return;
    }
    if (slip_packet_pos < HCI_PACKET_BUFFER_SIZE){
        slip_packet[slip_packet_pos++] = input;
    }
    log_error("hci_transport_h5_slip_store_byte: packet to long");
    hci_transport_h5_slip_init();
}

static void hci_transport_slip_process(uint8_t input){
    switch(slip_state){
        case SLIP_UNKNOWN:
            if (input != 0xc0) break;
            hci_transport_h5_slip_reset();
            slip_state = SLIP_X_C0;
            break;
        case SLIP_X_C0:
            switch(input){
                case 0xc0:
                    break;
                case 0xdb:
                    slip_state = SLIP_X_DB;
                    break;
                default:
                    hci_transport_h5_slip_store_byte(input);
                    slip_state = SLIP_DECODING;
                    break; 
            }                   
            break;
        case SLIP_X_DB:
            switch(input){
                case 0xdc:
                    hci_transport_h5_slip_store_byte(0xc0);
                    slip_state = SLIP_DECODING;
                    break;
                case 0xdd;
                    hci_transport_h5_slip_store_byte(0xdb);
                    slip_state = SLIP_DECODING;
                    break;
                default:
                    hci_transport_h5_slip_init();
                    break;
            }
            break;
        case SLIP_DECODING:
            switch(input){
                case 0xc0:
                    if (slip_payload_pos){
                        hci_transport_h5_process_frame();
                    }
                    hci_transport_h5_slip_init();
                    break;
                case 0xdb:
                    slip_state = SLIP_X_DB;
                    break;
                default:
                    hci_transport_h5_slip_store_byte(input);
                    break;
            }
            break;
    }
}

// Three-Wire Implementation

static void hci_transport_h5_process_frame(void){
    // TODO...
}

///
static int hci_transport_h5_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    // TODO...
}

/// H5 Interface

static void hci_transport_h5_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, int size)){
    packet_handler = handler;
}

static void hci_transport_h5_init(const void * transport_config){
    // check for hci_transport_config_uart_t
    if (!transport_config) {
        log_error("hci_transport_h5_posix: no config!");
        return;
    }
    if (((hci_transport_config_t*)transport_config)->type != HCI_TRANSPORT_CONFIG_UART) {
        log_error("hci_transport_h5_posix: config not of type != HCI_TRANSPORT_CONFIG_UART!");
        return;
    }
    hci_transport_config_uart = (hci_transport_config_uart_t*) transport_config;
}

static int hci_transport_h5_open(void){

    struct termios toptions;
    int flags = O_RDWR | O_NOCTTY | O_NONBLOCK;
    int fd = open(hci_transport_config_uart->device_name, flags);
    if (fd == -1)  {
        perror("h5_open: Unable to open port ");
        perror(hci_transport_config_uart->device_name);
        return -1;
    }
    
    if (tcgetattr(fd, &toptions) < 0) {
        perror("h5_open: Couldn't get term attributes");
        return -1;
    }
    
    cfmakeraw(&toptions);   // make raw

    // 8N1
    toptions.c_cflag &= ~CSTOPB;
    toptions.c_cflag |= CS8;

    if (hci_transport_config_uart->flowcontrol) {
        // with flow control
        toptions.c_cflag |= CRTSCTS;
    } else {
        // no flow control
        toptions.c_cflag &= ~CRTSCTS;
    }
    
    toptions.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
    toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl
    
    // see: http://unixwiz.net/techtips/termios-vmin-vtime.html
    toptions.c_cc[VMIN]  = 1;
    toptions.c_cc[VTIME] = 0;
    
    if( tcsetattr(fd, TCSANOW, &toptions) < 0) {
        perror("init_serialport: Couldn't set term attributes");
        return -1;
    }
    
    // set up data_source
    hci_transport_h5_data_source->fd = fd;
    hci_transport_h5_data_source->process = h4_process;
    btstack_run_loop_add_data_source(&h5_data_source);
    
    // also set baudrate
    if (h5_set_baudrate(hci_transport_config_uart->baudrate_init) < 0){
        return -1;
    }

    // init slip parser state machine
    hci_transport_h5_slip_init();
    
    // h5 link layer
    return 0;
}

static int hci_transport_h5_close(const void *transport_config){
    // first remove run loop handler
    btstack_run_loop_remove_data_source(h5_data_source);
    
    // then close device 
    close(hci_transport_h5_data_source->fd);
    return 0;
}

static int hci_transport_h5_process(struct btstack_data_source *ds) {
    if (hci_transport_h5_data_source->fd) return -1;

    // read up to bytes_to_read data in
    uint8_t data;
    while (1) {
        int bytes_read = read(hci_transport_h5_data_source->fd, &data, 1);
        if (bytes_read < 1) break;
        hci_transport_slip_process(data);
    };
    return 0;
}
// get h5 singleton
const hci_transport_t * hci_transport_h5_instance() {
    if (hci_transport_h5 == NULL) {
        hci_transport_h5 = (hci_transport_h5_t*)malloc( sizeof(hci_transport_t));
        memset(hci_transport_h5, 0, sizeof(hci_transport_t));
        hci_transport_h5->name                          = "H5_POSIX";
        hci_transport_h5->init                          = hci_transport_h5_init;
        hci_transport_h5->open                          = hci_transport_h5_open;
        hci_transport_h5->close                         = hci_transport_h5_close;
        hci_transport_h5->register_packet_handler       = hci_transport_h5_register_packet_handler;
        hci_transport_h5->can_send_packet_now           = NULL;
        hci_transport_h5->send_packet                   = hci_transport_h5_send_packet;
        hci_transport_h5->set_baudrate                  = hci_transport_h5_set_baudrate;
    }
    return (const hci_transport_t *) hci_transport_h5;
}
