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
 *  HCI Transport API implementation for basic H4 protocol over POSIX
 *
 *  Created by Matthias Ringwald on 4/29/09.
 */

#include "btstack_config.h"

#include <termios.h>  /* POSIX terminal control definitions */
#include <fcntl.h>    /* File control definitions */
#include <unistd.h>   /* UNIX standard function definitions */
#include <stdio.h>
#include <string.h>
#include <pthread.h> 

#include "btstack_debug.h"
#include "hci.h"
#include "hci_transport.h"

// assert pre-buffer for packet type is available
#if !defined(HCI_OUTGOING_PRE_BUFFER_SIZE) || (HCI_OUTGOING_PRE_BUFFER_SIZE == 0)
#error HCI_OUTGOING_PRE_BUFFER_SIZE not defined. Please update hci.h
#endif

static int  h4_process(struct btstack_data_source *ds);
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 

typedef enum {
    H4_W4_PACKET_TYPE,
    H4_W4_EVENT_HEADER,
    H4_W4_ACL_HEADER,
    H4_W4_SCO_HEADER,
    H4_W4_PAYLOAD,
} H4_STATE;

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

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

// packet reader state machine
static  H4_STATE h4_state;
static int bytes_to_read;
static int read_pos;

static uint8_t hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + 1 + HCI_PACKET_BUFFER_SIZE]; // packet type + max(acl header + acl payload, event header + event data)
static uint8_t * hci_packet = &hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];

static int    h4_set_baudrate(uint32_t baudrate){

    log_info("h4_set_baudrate %u", baudrate);

    struct termios toptions;
    int fd = hci_transport_h4->ds->fd;

    if (tcgetattr(fd, &toptions) < 0) {
        perror("init_serialport: Couldn't get term attributes");
        return -1;
    }
    
    speed_t brate = baudrate; // let you override switch below if needed
    switch(baudrate) {
        case 57600:  brate=B57600;  break;
        case 115200: brate=B115200; break;
#ifdef B230400
        case 230400: brate=B230400; break;
#endif
#ifdef B460800
        case 460800: brate=B460800; break;
#endif
#ifdef B921600
        case 921600: brate=B921600; break;
#endif
    }
    cfsetospeed(&toptions, brate);
    cfsetispeed(&toptions, brate);

    if( tcsetattr(fd, TCSANOW, &toptions) < 0) {
        perror("init_serialport: Couldn't set term attributes");
        return -1;
    }

    return 0;
}

static void h4_init(const void * transport_config){
    // check for hci_transport_config_uart_t
    if (!transport_config) {
        log_error("hci_transport_h4_posix: no config!");
        return;
    }
    if (((hci_transport_config_t*)transport_config)->type != HCI_TRANSPORT_CONFIG_UART) {
        log_error("hci_transport_h4_posix: config not of type != HCI_TRANSPORT_CONFIG_UART!");
        return;
    }
    hci_transport_config_uart = (hci_transport_config_uart_t*) transport_config;
}

static int h4_open(void){

    struct termios toptions;
    int flags = O_RDWR | O_NOCTTY | O_NONBLOCK;
    int fd = open(hci_transport_config_uart->device_name, flags);
    if (fd == -1)  {
        perror("init_serialport: Unable to open port ");
        perror(hci_transport_config_uart->device_name);
        return -1;
    }
    
    if (tcgetattr(fd, &toptions) < 0) {
        perror("init_serialport: Couldn't get term attributes");
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
    hci_transport_h4->ds = (btstack_data_source_t*) malloc(sizeof(btstack_data_source_t));
    if (!hci_transport_h4->ds) return -1;
    hci_transport_h4->uart_fd = fd;
    hci_transport_h4->ds->fd = fd;
    hci_transport_h4->ds->process = h4_process;
    btstack_run_loop_add_data_source(hci_transport_h4->ds);
    
    // also set baudrate
    if (h4_set_baudrate(hci_transport_config_uart->baudrate_init) < 0){
        return -1;
    }

    // init state machine
    bytes_to_read = 1;
    h4_state = H4_W4_PACKET_TYPE;
    read_pos = 0;    
    return 0;
}

static int h4_close(void){
    // first remove run loop handler
	btstack_run_loop_remove_data_source(hci_transport_h4->ds);
    
    // close device 
    close(hci_transport_h4->ds->fd);

    // free struct
    free(hci_transport_h4->ds);
    hci_transport_h4->ds = NULL;
    return 0;
}

static int h4_send_packet(uint8_t packet_type, uint8_t * packet, int size){
    if (hci_transport_h4->ds == NULL) return -1;
    if (hci_transport_h4->uart_fd == 0) return -1;

    uint32_t start = btstack_run_loop_get_time_ms();

    // store packet type before actual data and increase size
    char *data = (char*) packet;
    size++;
    data--;
    *data = packet_type;
    while (size > 0) {
        int bytes_written = write(hci_transport_h4->uart_fd, data, size);
        if (bytes_written < 0) {
            log_info("usleep(5000)");
            usleep(5000);
            continue;
        }
        data += bytes_written;
        size -= bytes_written;
    }

    uint32_t end = btstack_run_loop_get_time_ms();
    if (end - start > 10){
        log_info("h4_send_packet: write took %u ms", end - start);
    }

    return 0;
}

static void   h4_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

static void   h4_deliver_packet(void){
    if (read_pos < 3) return; // sanity check
    packet_handler(hci_packet[0], &hci_packet[1], read_pos-1);
    
    h4_state = H4_W4_PACKET_TYPE;
    read_pos = 0;
    bytes_to_read = 1;
}

static void h4_statemachine(void){
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
                default:
                    log_error("h4_process: invalid packet type 0x%02x", hci_packet[0]);
                    read_pos = 0;
                    bytes_to_read = 1;
                    break;
            }
            break;
            
        case H4_W4_EVENT_HEADER:
            bytes_to_read = hci_packet[2];
            h4_state = H4_W4_PAYLOAD;
            break;
            
        case H4_W4_ACL_HEADER:
            bytes_to_read = READ_BT_16( hci_packet, 3);
            h4_state = H4_W4_PAYLOAD;
            break;
            
        case H4_W4_SCO_HEADER:
            bytes_to_read = hci_packet[3];
            h4_state = H4_W4_PAYLOAD;
            break;

        case H4_W4_PAYLOAD:
            h4_deliver_packet();
            break;

        default:
            break;
    }
}

static int h4_process(struct btstack_data_source *ds) {
    if (hci_transport_h4->uart_fd == 0) return -1;

    int read_now = bytes_to_read;

    uint32_t start = btstack_run_loop_get_time_ms();
    
    // read up to bytes_to_read data in
    ssize_t bytes_read = read(hci_transport_h4->uart_fd, &hci_packet[read_pos], read_now);
    // log_info("h4_process: bytes read %u", bytes_read);
    if (bytes_read < 0) {
        return bytes_read;
    }

    uint32_t end = btstack_run_loop_get_time_ms();
    if (end - start > 10){
        log_info("h4_process: read took %u ms", end - start);
    }
    
    bytes_to_read -= bytes_read;
    read_pos      += bytes_read;
    if (bytes_to_read > 0) {
        return 0;
    }
    
    h4_statemachine();
    return 0;
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

// get h4 singleton
const hci_transport_t * hci_transport_h4_instance() {
    if (hci_transport_h4 == NULL) {
        hci_transport_h4 = (hci_transport_h4_t*)malloc( sizeof(hci_transport_h4_t));
        memset(hci_transport_h4, 0, sizeof(hci_transport_h4_t));
        hci_transport_h4->ds                                      = NULL;
        hci_transport_h4->transport.name                          = "H4_POSIX";
        hci_transport_h4->transport.init                          = h4_init;
        hci_transport_h4->transport.open                          = h4_open;
        hci_transport_h4->transport.close                         = h4_close;
        hci_transport_h4->transport.register_packet_handler       = h4_register_packet_handler;
        hci_transport_h4->transport.can_send_packet_now           = NULL;
        hci_transport_h4->transport.send_packet                   = h4_send_packet;
        hci_transport_h4->transport.set_baudrate                  = h4_set_baudrate;
    }
    return (const hci_transport_t *) hci_transport_h4;
}
