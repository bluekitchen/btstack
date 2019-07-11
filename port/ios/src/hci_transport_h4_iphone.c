/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
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
 * Please inquire about commercial licensing options at contact@bluekitchen-gmbh.com
 *
 */

/*
 *  hci_h4_transport_iphone.c
 *
 *  HCI Transport API implementation for basic H4 protocol
 *  with extension for "enforced wake device" using the netgraph styele
 *  interface to the Bluetooth UART on iOS
 *
 *  This is only tested on newer devices with Broadcom chipsets 
 *
 */

#include "btstack_config.h"
#include "btstack_control_iphone.h"

#define SOCKET_DEVICE "com.apple.uart.bluetooth"
#include <sys/socket.h>
#include <sys/ioctl.h>
#ifndef PF_NETGRAPH
#define PF_NETGRAPH       32          /* normally in sys/socket.h */
#endif
#define NG_CONTROL        2           /* from freeBSD sys/ng_socket.h */
#define SOCK_IOCTL_VAL    0xC0644E03  /* from reversing BTServer */

#ifdef __LP64__
// values changed for newer 64 bit devices
#define GETSOCKOPT_VAL    0x40487413  /* from reversing BTServer */
#define SETSOCKOPT_VAL    0x80487414  /* from reversing BTServer */
#else
 // older devices run 32 bit version
#define GETSOCKOPT_VAL    0x402C7413  /* from reversing BTServer */
#define SETSOCKOPT_VAL    0x802C7414  /* from reversing BTServer */
#endif

// don't enforce wake after 3s idle
#define HCI_WAKE_TIMER_MS 3000
#define HCI_WAKE_DURATION 10000

#include <errno.h>
#include <termios.h>  /* POSIX terminal control definitions */
#include <fcntl.h>    /* File control definitions */
#include <unistd.h>   /* UNIX standard function definitions */
#include <stdio.h>
#include <string.h>
#include <pthread.h> 

#include "btstack_debug.h"
#include "hci.h"
#include "hci_transport.h"

static void h4_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type);
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 
static      hci_transport_config_uart_t *hci_transport_config_uart;

static void h4_enforce_wake_on(void);
static void h4_enforce_wake_off(void);
static void h4_enforce_wake_timeout(struct btstack_timer_source *ts);

typedef enum {
    H4_W4_PACKET_TYPE,
    H4_W4_EVENT_HEADER,
    H4_W4_ACL_HEADER,
    H4_W4_PAYLOAD,
} H4_STATE;

typedef struct hci_transport_h4 {
    hci_transport_t transport;
    btstack_data_source_t *ds;
    int uart_fd;    // different from ds->fd for HCI reader thread
    /* power management support, e.g. used by iOS */
    btstack_timer_source_t sleep_timer;
} hci_transport_h4_t;


/* NG sockaddr definition */
struct sockaddr_ng {
    uint8_t  sg_len;     /* total length */
    uint8_t  sg_family;  /* address family */
    uint16_t sg_subtype;
    uint32_t sg_node;
    uint32_t sg_null;
    uint8_t  sg_dummy[20];
};

// single instance
static hci_transport_h4_t * hci_transport_h4 = NULL;

// enforced wake support
static char * enforce_wake_device = NULL;
static int    enforce_wake_fd = 0;

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

// packet reader state machine
static  H4_STATE h4_state;
static int bytes_to_read;
static int read_pos;

static uint8_t hci_packet[1+HCI_INCOMING_PACKET_BUFFER_SIZE]; // packet type + max(acl header + acl payload, event header + event data)

static void h4_init(const void * transport_config){
    // check for hci_transport_config_uart_t
    if (!transport_config) {
        log_error("hci_transport_h4_iphone: no config!");
        return;
    }
    if (((hci_transport_config_t*)transport_config)->type != HCI_TRANSPORT_CONFIG_UART) {
        log_error("hci_transport_h4_iphone: config not of type != HCI_TRANSPORT_CONFIG_UART!");
        return;
    }
    hci_transport_config_uart = (hci_transport_config_uart_t*) transport_config;
}

static int h4_open(void)
{
    int fd = socket(PF_NETGRAPH, SOCK_STREAM, NG_CONTROL);
    log_info("h4_open: open socket(%u, %u, %u) %d", PF_NETGRAPH, SOCK_STREAM, NG_CONTROL, fd);
    if (fd < 0) {
        log_error("h4_open: open socket failed");
        perror("socket(HCI_IF)");
        goto err_out0;
    }
    
    // get node address
    struct ioctl_arg_t {
        uint32_t result;
        char socket_name[96];
    } ioctl_arg;
    memset((void *) &ioctl_arg, 0x00, sizeof(struct ioctl_arg_t));
    strcpy((char *) &ioctl_arg.socket_name, SOCKET_DEVICE);
    if (ioctl(fd, SOCK_IOCTL_VAL, &ioctl_arg) != 0) {
        log_error("h4_open: ioctl failed");
        perror("ioctl(fd_sock, SOCK_IOCTL_VAL)");
        goto err_out1;
    }
    
    // setup sock addr struct
    struct sockaddr_ng sock_addr;
    sock_addr.sg_len     = sizeof(struct sockaddr_ng);
    sock_addr.sg_family  = PF_NETGRAPH;
    sock_addr.sg_subtype = 0x02;
    sock_addr.sg_node    = ioctl_arg.result;
    sock_addr.sg_null    = 0;
    
    // connect
    if (connect(fd, (const struct sockaddr *) &sock_addr, sizeof(struct sockaddr_ng)) != 0) {
        log_error("h4_open: connect failed");
        perror("connect(fd_sock)");
        goto err_out2;
    }
    log_info("h4_open: connect to socket ok");
    
    // configure UART
    struct termios toptions;
    socklen_t toptions_len = sizeof(struct termios);
    log_info("h4_open: sizeof (struct termios) = %lu", sizeof(struct termios));
    log_info("h4_open: getsockopt (fd, %d, %x, buffer, %u)", SO_ACCEPTCONN, GETSOCKOPT_VAL, toptions_len);
    int err = getsockopt(fd, SO_ACCEPTCONN, GETSOCKOPT_VAL, &toptions, &toptions_len);
    log_info("h4_open: -> err %d, options_len %u", err, toptions_len);
    if (err) {
        log_error("h4_open: getsockopt failed with errno %d", errno);
        perror("getsockopt(fd_sock)");
        goto err_out3;
    }

    uint32_t baud = hci_transport_config_uart->baudrate_init;
    // if baud == 0, we're using system default: set in transport config
    if (baud == 0) {
        baud = btstack_control_iphone_get_transport_speed();
    }

    // make raw and set speed
    cfmakeraw(&toptions);
    speed_t brate = (speed_t) hci_transport_config_uart->baudrate_init;
    cfsetspeed(&toptions, brate);    
    toptions.c_iflag |=  IGNPAR;
    toptions.c_cflag = 0x00038b00;

    log_info("h4_open: setsockopt (fd, %d, %x, buffer, %u)", SO_ACCEPTCONN, SETSOCKOPT_VAL, toptions_len);
    err = setsockopt(fd, SO_ACCEPTCONN, SETSOCKOPT_VAL, &toptions, toptions_len);
    if (err) {
        log_error("h4_open: setsockopt failed with errno %d", errno);
        perror("setsockopt(fd_sock)");
        goto err_out4;
    }
        
    hci_transport_h4->uart_fd = fd;

    // set up data_source
    hci_transport_h4->ds = calloc(sizeof(btstack_data_source_t), 1);
    if (!hci_transport_h4->ds) return -1;
    btstack_run_loop_set_data_source_fd(hci_transport_h4->ds, fd);
    btstack_run_loop_set_data_source_handler(hci_transport_h4->ds, &h4_process);
    btstack_run_loop_enable_data_source_callbacks(hci_transport_h4->ds, DATA_SOURCE_CALLBACK_READ);
    btstack_run_loop_add_data_source(hci_transport_h4->ds);
    
    // init state machine
    bytes_to_read = 1;
    h4_state = H4_W4_PACKET_TYPE;
    read_pos = 0;
    
    return 0;
    
err_out4:
err_out3:
err_out2:
err_out1:
    close(fd);
err_out0:
    fprintf(stderr, "h4_open error\n");

    return -1;
}

static int h4_close(void){
    // first remove run loop handler
	btstack_run_loop_remove_data_source(hci_transport_h4->ds);
    
    // close device 
    close(hci_transport_h4->ds->source.fd);
    
    // let module sleep
    h4_enforce_wake_off();
    
    // free struct
    free(hci_transport_h4->ds);
    hci_transport_h4->ds = NULL;
    return 0;
}

static int h4_send_packet(uint8_t packet_type, uint8_t * packet, int size){
    if (hci_transport_h4->ds == NULL) return -1;
    if (hci_transport_h4->uart_fd == 0) return -1;

    // wake Bluetooth module
    h4_enforce_wake_on();

    char *data = (char*) packet;
    int bytes_written = write(hci_transport_h4->uart_fd, &packet_type, 1);
    while (bytes_written < 1) {
        usleep(5000);
        bytes_written = write(hci_transport_h4->uart_fd, &packet_type, 1);
    };
    while (size > 0) {
        int bytes_written = write(hci_transport_h4->uart_fd, data, size);
        if (bytes_written < 0) {
            usleep(5000);
            continue;
        }
        data += bytes_written;
        size -= bytes_written;
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
            if (hci_packet[0] == HCI_EVENT_PACKET){
                bytes_to_read = HCI_EVENT_HEADER_SIZE;
                h4_state = H4_W4_EVENT_HEADER;
            } else if (hci_packet[0] == HCI_ACL_DATA_PACKET){
                bytes_to_read = HCI_ACL_HEADER_SIZE;
                h4_state = H4_W4_ACL_HEADER;
            } else {
                log_error("h4_process: invalid packet type 0x%02x", hci_packet[0]);
                read_pos = 0;
                bytes_to_read = 1;
            }
            break;
            
        case H4_W4_EVENT_HEADER:
            bytes_to_read = hci_packet[2];
            h4_state = H4_W4_PAYLOAD;
            break;
            
        case H4_W4_ACL_HEADER:
            bytes_to_read = little_endian_read_16( hci_packet, 3);
            h4_state = H4_W4_PAYLOAD;
            break;
            
        case H4_W4_PAYLOAD:
            h4_deliver_packet();
            break;
        default:
            break;
    }
}

static void h4_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    if (hci_transport_h4->uart_fd == 0) return;

    int read_now = bytes_to_read;
    
    // read up to bytes_to_read data in
    ssize_t bytes_read = read(hci_transport_h4->uart_fd, &hci_packet[read_pos], read_now);
    if (bytes_read < 0) return;
        
    bytes_to_read -= bytes_read;
    read_pos      += bytes_read;
    if (bytes_to_read > 0) return;
    
    h4_statemachine();
}

static void h4_enforce_wake_on(void){

    if (!enforce_wake_device) return;
    
    if (!enforce_wake_fd) {
        enforce_wake_fd = open(enforce_wake_device, O_RDWR);
        usleep(HCI_WAKE_DURATION);  // wait until device is ready
    }
    btstack_run_loop_remove_timer(&hci_transport_h4->sleep_timer);
    btstack_run_loop_set_timer(&hci_transport_h4->sleep_timer, HCI_WAKE_TIMER_MS);
    hci_transport_h4->sleep_timer.process = h4_enforce_wake_timeout;
    btstack_run_loop_add_timer(&hci_transport_h4->sleep_timer); 
}

static void h4_enforce_wake_off(void){

    btstack_run_loop_remove_timer(&hci_transport_h4->sleep_timer);
    
    if (enforce_wake_fd) {
        close(enforce_wake_fd);
        enforce_wake_fd = 0;
    }
}

static void h4_enforce_wake_timeout(struct btstack_timer_source *ts){
    h4_enforce_wake_off();
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

// get h4 singleton
const hci_transport_t * hci_transport_h4_instance(const btstack_uart_block_t * uart_driver) {
    if (hci_transport_h4 == NULL) {
        hci_transport_h4 = (hci_transport_h4_t*)malloc( sizeof(hci_transport_h4_t));
        memset(hci_transport_h4, 0, sizeof(hci_transport_h4_t));
        hci_transport_h4->ds                                      = NULL;
        hci_transport_h4->transport.name                          = "H4_IPHONE";
        hci_transport_h4->transport.init                          = h4_init;
        hci_transport_h4->transport.open                          = h4_open;
        hci_transport_h4->transport.close                         = h4_close;
        hci_transport_h4->transport.register_packet_handler       = h4_register_packet_handler;
        hci_transport_h4->transport.can_send_packet_now           = NULL;
        hci_transport_h4->transport.send_packet                   = h4_send_packet;
        hci_transport_h4->transport.set_baudrate                  = NULL;
    }
    return (const hci_transport_t *) hci_transport_h4;
}

void hci_transport_h4_iphone_set_enforce_wake_device(char *path){
    enforce_wake_device = path;
}
