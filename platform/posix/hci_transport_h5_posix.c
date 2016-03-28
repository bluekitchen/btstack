/*
 * Copyright (C) 2016 BlueKitchen GmbH
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
#include "btstack_slip.h"
#include "btstack_debug.h"
#include "hci_transport.h"

#ifdef HAVE_EHCILL
#error "HCI Transport H5 POSIX does not support eHCILL. Please either use HAVE_EHCILL or H5 Transport"
#endif 

/// newer

typedef enum {
    LINK_UNINITIALIZED,
    LINK_INITIALIZED,
    LINK_ACTIVE
} hci_transport_link_state_t;

// Configuration Field. No packet buffers -> sliding window = 1, no OOF flow control, no data integrity check
#define LINK_CONFIG_SLIDING_WINDOW_SIZE 1
#define LINK_CONFIG_OOF_FLOW_CONTROL 0
#define LINK_CONFIG_DATA_INTEGRITY_CHECK 0
#define LINK_CONFIG_VERSION_NR 0
#define LINK_CONFIG_FIELD (LINK_CONFIG_SLIDING_WINDOW_SIZE | (LINK_CONFIG_OOF_FLOW_CONTROL << 3) | (LINK_CONFIG_DATA_INTEGRITY_CHECK << 4) | (LINK_CONFIG_VERSION_NR << 5))

// periodic sending during link establishment
#define LINK_PERIOD_MS 250

// resend wakeup
#define LINK_WAKEUP_MS 50

// additional packet types
#define LINK_ACKNOWLEDGEMENT_TYPE 0x00
#define LINK_CONTROL_PACKET_TYPE 0x0f

static const uint8_t link_control_sync[] =   { 0x01, 0x7e};
static const uint8_t link_control_sync_response[] = { 0x02, 0x7d};
static const uint8_t link_control_config[] = { 0x03, 0xfc, LINK_CONFIG_FIELD};
static const uint8_t link_control_config_response[] = { 0x04, 0x7b, LINK_CONFIG_FIELD};
static const uint8_t link_control_config_response_prefix_len  = 2;
static const uint8_t link_control_wakeup[] = { 0x05, 0xfa};
static const uint8_t link_control_woken[] =  { 0x06, 0xf9};
static const uint8_t link_control_sleep[] =  { 0x07, 0x78};

// incoming pre-bufffer + 4 bytes H5 header + max(acl header + acl payload, event header + event data) + 2 bytes opt CRC
static uint8_t   hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + 6 + HCI_PACKET_BUFFER_SIZE];

// Non-optimized outgoing buffer (EOF, 4 bytes header, payload, EOF)
static uint8_t slip_outgoing_buffer[2 + 2 * (HCI_PACKET_BUFFER_SIZE + 4)];

// H5 Link State
static hci_transport_link_state_t link_state;
static btstack_timer_source_t link_timer;
static uint8_t  link_seq_nr;
static uint8_t  link_ack_nr;
static uint16_t link_resend_timeout_ms;
static uint8_t  link_peer_asleep;

// Outgoing packet
static uint8_t   hci_packet_type;
static uint16_t  hci_packet_size;
static uint8_t * hci_packet;

// device
static hci_transport_config_uart_t * hci_transport_config_uart;

// data source for device
static btstack_data_source_t         hci_transport_h5_data_source;

// hci_transport_t instance
static hci_transport_t *             hci_transport_h5;

// hci packet handler
static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);


// Prototypes
static int hci_transport_h5_process(btstack_data_source_t *ds);
static void hci_transport_link_set_timer(uint16_t timeout_ms);
static void hci_transport_link_timeout_handler(btstack_timer_source_t * timer);
static int  hci_transport_h5_outgoing_packet(void);
static void hci_transport_h5_send_queued_packet(void);

// Generic helper
static void hci_transport_h5_send_really(const uint8_t * data, int size){
    // log_info("hci_transport_h5_send_really (%u bytes)", size);
    // log_info_hexdump(data, size);
    while (size > 0) {
        int bytes_written = write(hci_transport_h5_data_source.fd, data, size);
        if (bytes_written < 0) {
            usleep(5000);
            continue;
        }
        data += bytes_written;
        size -= bytes_written;
    }
}

// SLIP Outgoing

// format: 0xc0 HEADER PACKER 0xc0
// @param uint8_t header[4]
static void hci_transport_slip_send_frame(const uint8_t * header, const uint8_t * packet, uint16_t packet_size){
    
    int pos = 0;

    // Start of Frame
    slip_outgoing_buffer[pos++] = BTSTACK_SLIP_SOF;

    // Header
    btstack_slip_encoder_start(header, 4);
    while (btstack_slip_encoder_has_data()){
        slip_outgoing_buffer[pos++] = btstack_slip_encoder_get_byte();
    }

    // Packet
    btstack_slip_encoder_start(packet, packet_size);
    while (btstack_slip_encoder_has_data()){
        slip_outgoing_buffer[pos++] = btstack_slip_encoder_get_byte();
    }

    // Start of Frame
    slip_outgoing_buffer[pos++] = BTSTACK_SLIP_SOF;

    hci_transport_h5_send_really(slip_outgoing_buffer, pos);
}

// SLIP Incoming

static void hci_transport_slip_init(void){
    btstack_slip_decoder_init(&hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE], 6 + HCI_PACKET_BUFFER_SIZE);
}

// H5 Three-Wire Implementation

static void hci_transport_link_calc_header(uint8_t * header,
    uint8_t  sequence_nr,
    uint8_t  acknowledgement_nr,
    uint8_t  data_integrity_check_present,
    uint8_t  reliable_packet,
    uint8_t  packet_type,
    uint16_t payload_length){

    // reset data integrity flag
    if (data_integrity_check_present){
        log_error("hci_transport_link_calc_header: data integrity not supported, dropping flag");
        data_integrity_check_present = 0;
    }

    header[0] = sequence_nr | (acknowledgement_nr << 3) | (data_integrity_check_present << 6) | (reliable_packet << 7);
    header[1] = packet_type | ((payload_length & 0x0f) << 4);
    header[2] = payload_length >> 4;
    header[3] = 0xff - (header[0] + header[1] + header[2]);
}

static void hci_transport_link_send_control(const uint8_t * message, int message_len){
    uint8_t header[4];
    hci_transport_link_calc_header(header, 0, 0, 0, 0, LINK_CONTROL_PACKET_TYPE, message_len);
    hci_transport_slip_send_frame(header, message, message_len);
}

static void hci_transport_link_send_sync(void){
    log_info("link: send sync");
    hci_transport_link_send_control(link_control_sync, sizeof(link_control_sync));
}

static void hci_transport_link_send_sync_response(void){
    log_info("link: send sync response");
    hci_transport_link_send_control(link_control_sync_response, sizeof(link_control_sync_response));
}

static void hci_transport_link_send_config(void){
    log_info("link: send config");
    hci_transport_link_send_control(link_control_config, sizeof(link_control_config));
}

static void hci_transport_link_send_config_response(void){
    log_info("link: send config response");
    hci_transport_link_send_control(link_control_config_response, sizeof(link_control_config_response));
}

static void hci_transport_link_send_woken(void){
    log_info("link: send woken");
    hci_transport_link_send_control(link_control_woken, sizeof(link_control_woken));
}

static void hci_transport_link_send_wakeup(void){
    log_info("link: send wakeup");
    hci_transport_link_send_control(link_control_wakeup, sizeof(link_control_wakeup));
}

static void hci_transport_link_send_ack_packet(void){
    log_info("link: send ack %u", link_ack_nr);
    uint8_t header[4];
    hci_transport_link_calc_header(header, 0, link_ack_nr, 0, 0, LINK_ACKNOWLEDGEMENT_TYPE, 0);
    hci_transport_slip_send_frame(header, NULL, 0);
}

static void hci_transport_link_set_timer(uint16_t timeout_ms){
    btstack_run_loop_set_timer_handler(&link_timer, &hci_transport_link_timeout_handler);
    btstack_run_loop_set_timer(&link_timer, timeout_ms);
    btstack_run_loop_add_timer(&link_timer);
}

static void hci_transport_link_timeout_handler(btstack_timer_source_t * timer){
    switch (link_state){
        case LINK_UNINITIALIZED:
            hci_transport_link_send_sync();
            hci_transport_link_set_timer(LINK_PERIOD_MS);
            break;            
        case LINK_INITIALIZED:
            hci_transport_link_send_config();
            hci_transport_link_set_timer(LINK_PERIOD_MS);
            break;
        case LINK_ACTIVE:
            if (!hci_transport_h5_outgoing_packet()){
                log_info("h5 timeout while active, but no outgoing packet");
                return;
            }
            if (link_peer_asleep){
                hci_transport_link_send_wakeup();
                hci_transport_link_set_timer(LINK_WAKEUP_MS);
                return;
            }
            // resend packet
            hci_transport_h5_send_queued_packet();
            hci_transport_link_set_timer(link_resend_timeout_ms);
            break;
        default:
            break;
    }
}

static void hci_transport_link_init(void){
    link_state = LINK_UNINITIALIZED;
    link_peer_asleep = 0;
 
    // get started
    hci_transport_link_send_sync();
    hci_transport_link_set_timer(LINK_PERIOD_MS);
}

static int hci_transport_h5_inc_seq_nr(int seq_nr){
    return (seq_nr + 1) & 0x07;    
}

static int hci_transport_h5_outgoing_packet(void){
    return hci_packet != 0;
}

static void hci_transport_h5_clear_queue(void){
    btstack_run_loop_remove_timer(&link_timer);
    hci_packet = NULL;
}

static void hci_transport_h5_queue_packet(uint8_t packet_type, uint8_t *packet, int size){
    hci_packet = packet;
    hci_packet_type = packet_type;
    hci_packet_size = size;
}

static void hci_transport_h5_send_queued_packet(void){
    log_info("hci_transport_h5_send_queued_packet: seq %u, ack %u, size %u", link_seq_nr, link_ack_nr, hci_packet_size);
    log_info_hexdump(hci_packet, hci_packet_size);

    uint8_t header[4];
    hci_transport_link_calc_header(header, link_seq_nr, link_ack_nr, 0, 1, hci_packet_type, hci_packet_size);
    hci_transport_slip_send_frame(header, hci_packet, hci_packet_size);
}

static int hci_transport_h5_can_send_packet_now(uint8_t packet_type){
    if (hci_transport_h5_outgoing_packet()) return 0;
    return link_state == LINK_ACTIVE;
}

static void hci_transport_h5_process_frame(uint16_t frame_size){

    if (frame_size < 4) return;

    uint8_t * slip_header  = &hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE];
    uint8_t * slip_payload = &hci_packet_with_pre_buffer[HCI_INCOMING_PRE_BUFFER_SIZE + 4];
    int       frame_size_without_header = frame_size - 4;

    int      seq_nr =  slip_header[0] & 0x07;
    int      ack_nr = (slip_header[0] >> 3)    & 0x07;
    int      data_integrity_check_present = (slip_header[0] & 0x40) != 0;
    int      reliable_packet  = (slip_header[0] & 0x80) != 0;
    uint8_t  link_packet_type = slip_header[1] & 0x0f;
    uint16_t link_payload_len = (slip_header[1] >> 4) | (slip_header[2] << 4);

    log_info("hci_transport_h5_process_frame, reliable %u, packet type %u, seq_nr %u, ack_nr %u", reliable_packet, link_packet_type, seq_nr, ack_nr);
    log_info_hexdump(slip_header, 4);
    log_info_hexdump(slip_payload, frame_size_without_header);

    // validate header checksum
    uint8_t header_checksum = slip_header[0] + slip_header[1] + slip_header[2] + slip_header[3];
    if (header_checksum != 0xff){
        log_info("h5: header checksum 0x%02x (instead of 0xff)", header_checksum);
        return;
    }

    // validate payload length
    int data_integrity_len = data_integrity_check_present ? 2 : 0;
    uint16_t received_payload_len = frame_size_without_header - data_integrity_len;
    if (link_payload_len != received_payload_len){
        log_info("h5: expected payload len %u but got %u", link_payload_len, received_payload_len);
        return;
    }

    // (TODO data integrity check)

    switch (link_state){
        case LINK_UNINITIALIZED:
            if (link_packet_type != LINK_CONTROL_PACKET_TYPE) break;
            if (memcmp(slip_payload, link_control_sync, sizeof(link_control_sync)) == 0){
                log_info("link: received sync");
                hci_transport_link_send_sync_response();
            }
            if (memcmp(slip_payload, link_control_sync_response, sizeof(link_control_sync_response)) == 0){
                log_info("link: received sync response");
                link_state = LINK_INITIALIZED;
                btstack_run_loop_remove_timer(&link_timer);
                log_info("link initialized");
                //
                hci_transport_link_send_config();
                hci_transport_link_set_timer(LINK_PERIOD_MS);
            }
            break;
        case LINK_INITIALIZED:
            if (link_packet_type != LINK_CONTROL_PACKET_TYPE) break;
            if (memcmp(slip_payload, link_control_sync, sizeof(link_control_sync)) == 0){
                log_info("link: received sync");
                hci_transport_link_send_sync_response();
            }
            if (memcmp(slip_payload, link_control_config, sizeof(link_control_config)) == 0){
                log_info("link: received config");
                hci_transport_link_send_config_response();
            }
            if (memcmp(slip_payload, link_control_config_response, link_control_config_response_prefix_len) == 0){
                log_info("link: received config response");
                link_state = LINK_ACTIVE;
                btstack_run_loop_remove_timer(&link_timer);
                log_info("link activated");
                // 
                link_seq_nr = 0;
                link_ack_nr = 0;
                // notify upper stack that it can start
                uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
                packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
            }
            break;
        case LINK_ACTIVE:

            // validate packet sequence nr in reliable packets (check for out of sequence error)
            if (reliable_packet){
                if (seq_nr != link_ack_nr){
                    log_info("expected seq nr %u, but received %u", link_ack_nr, seq_nr);
                    hci_transport_link_send_ack_packet();
                    return;
                }
                // ack packet right away
                link_ack_nr = hci_transport_h5_inc_seq_nr(link_ack_nr);
                hci_transport_link_send_ack_packet();
            }
          
            // Process ACKs in reliable packet and explicit ack packets
            if (reliable_packet || link_packet_type == LINK_ACKNOWLEDGEMENT_TYPE){
                // our packet is good if the remote expects our seq nr + 1
                int next_seq_nr = hci_transport_h5_inc_seq_nr(link_seq_nr);
                if (hci_transport_h5_outgoing_packet() && next_seq_nr == ack_nr){
                    log_info("h5: outoing packet with seq %u ack'ed", link_seq_nr);
                    link_seq_nr = next_seq_nr;
                    hci_transport_h5_clear_queue();

                    // notify upper stack that it can send again
                    uint8_t event[] = { HCI_EVENT_TRANSPORT_PACKET_SENT, 0};
                    packet_handler(HCI_EVENT_PACKET, &event[0], sizeof(event));
                }
            } 

            switch (link_packet_type){
                case LINK_CONTROL_PACKET_TYPE:
                    if (memcmp(slip_payload, link_control_config, sizeof(link_control_config)) == 0){
                        log_info("link: received config");
                        hci_transport_link_send_config_response();
                        break;
                    }
                    if (memcmp(slip_payload, link_control_sync, sizeof(link_control_sync)) == 0){
                        log_info("link: received sync in ACTIVE STATE!");
                        // TODO sync during active indicates peer reset -> full upper layer reset necessary
                        break;
                    }
                    if (memcmp(slip_payload, link_control_sleep, sizeof(link_control_sleep)) == 0){
                        log_info("link: received sleep message");
                        link_peer_asleep = 1;
                        break;
                    }
                    if (memcmp(slip_payload, link_control_wakeup, sizeof(link_control_wakeup)) == 0){
                        log_info("link: received wakupe message -> send woken");
                        link_peer_asleep = 0;
                        hci_transport_link_send_woken();
                        break;
                    }
                    if (memcmp(slip_payload, link_control_woken, sizeof(link_control_woken)) == 0){
                        log_info("link: received woken message");
                        link_peer_asleep = 0;
                        // TODO: send packet if queued....
                        break;
                    }
                    break;
                case HCI_EVENT_PACKET:
                case HCI_ACL_DATA_PACKET:
                case HCI_SCO_DATA_PACKET:
                    packet_handler(link_packet_type, slip_payload, link_payload_len);
                    break;
            }

            break;
        default:
            break;
    }

}

static int hci_transport_h5_send_packet(uint8_t packet_type, uint8_t *packet, int size){
    if (!hci_transport_h5_can_send_packet_now(packet_type)){
        log_error("hci_transport_h5_send_packet called but in state %u", link_state);
        return -1;
    }

    // store request
    hci_transport_h5_queue_packet(packet_type, packet, size);

    // check peer sleep mode
    if (link_peer_asleep){
        hci_transport_link_send_wakeup();
        hci_transport_link_set_timer(LINK_WAKEUP_MS);
        return 0;        
    }

    // otherwise, send packet right away
    hci_transport_h5_send_queued_packet();

    // set timer for retransmit
    hci_transport_link_set_timer(link_resend_timeout_ms);
    return 0;
}

/// H5 Interface

static void hci_transport_h5_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    packet_handler = handler;
}

// recommendet time until resend: 3 * time of largest packet
static uint16_t hci_transport_h5_calc_resend_timeout(uint32_t baudrate){
    uint32_t max_packet_size_in_bit = (HCI_PACKET_BUFFER_SIZE + 6) << 3;
    uint32_t t_max_x3_ms = max_packet_size_in_bit * 3000 / baudrate;
    log_info("resend timeout for %u baud: %u ms", baudrate, t_max_x3_ms);
    return t_max_x3_ms;
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
    hci_transport_h5_data_source.fd = -1;
}

static int hci_transport_h5_set_baudrate(uint32_t baudrate){

    log_info("hci_transport_h5_set_baudrate %u", baudrate);

    struct termios toptions;
    int fd = hci_transport_h5_data_source.fd;

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

    // extra for h5: calc resend timeout
    link_resend_timeout_ms = hci_transport_h5_calc_resend_timeout(baudrate);

    return 0;
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
    //
    // toptions.c_cflag |= PARENB; // enable even parity
    //
    toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl
    
    // see: http://unixwiz.net/techtips/termios-vmin-vtime.html
    toptions.c_cc[VMIN]  = 1;
    toptions.c_cc[VTIME] = 0;
    
    if( tcsetattr(fd, TCSANOW, &toptions) < 0) {
        perror("init_serialport: Couldn't set term attributes");
        return -1;
    }
    
    // set up data_source
    btstack_run_loop_set_data_source_fd(&hci_transport_h5_data_source, fd);
    btstack_run_loop_set_data_source_handler(&hci_transport_h5_data_source, &hci_transport_h5_process);
    btstack_run_loop_enable_data_source_callbacks(&hci_transport_h5_data_source, DATA_SOURCE_CALLBACK_READ);
    btstack_run_loop_add_data_source(&hci_transport_h5_data_source);
    
    // also set baudrate
    if (hci_transport_h5_set_baudrate(hci_transport_config_uart->baudrate_init) < 0){
        return -1;
    }

    // init slip parser state machine
    hci_transport_slip_init();
    
    // init link management - already starts syncing
    hci_transport_link_init();

    return 0;
}

void hci_transport_h5_reset_link(void){

    log_info("hci_transport_h5_reset_link");

    // clear outgoing queue
    hci_transport_h5_clear_queue();

    // init slip parser state machine
    hci_transport_slip_init();
    
    // init link management - already starts syncing
    hci_transport_link_init();
}

static int hci_transport_h5_close(void){
    // first remove run loop handler
    btstack_run_loop_remove_data_source(&hci_transport_h5_data_source);
    
    // then close device 
    close(hci_transport_h5_data_source.fd);
    hci_transport_h5_data_source.fd = -1;
    return 0;
}

static void hci_transport_h5_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    if (hci_transport_h5_data_source.fd < 0) return;

    // process data byte by byte
    uint8_t data;
    while (1) {
        int bytes_read = read(hci_transport_h5_data_source.fd, &data, 1);
        if (bytes_read < 1) break;
        // log_info("slip: process 0x%02x", data);
        btstack_slip_decoder_process(data);
        uint16_t frame_size = btstack_slip_decoder_frame_size();
        if (frame_size) {
            hci_transport_h5_process_frame(frame_size);
            hci_transport_slip_init();
        }
    };
}

// get h5 singleton
const hci_transport_t * hci_transport_h5_instance(void) {
    if (hci_transport_h5 == NULL) {
        hci_transport_h5 = (hci_transport_t*) malloc(sizeof(hci_transport_t));
        memset(hci_transport_h5, 0, sizeof(hci_transport_t));
        hci_transport_h5->name                          = "H5_POSIX";
        hci_transport_h5->init                          = &hci_transport_h5_init;
        hci_transport_h5->open                          = &hci_transport_h5_open;
        hci_transport_h5->close                         = &hci_transport_h5_close;
        hci_transport_h5->register_packet_handler       = &hci_transport_h5_register_packet_handler;
        hci_transport_h5->can_send_packet_now           = &hci_transport_h5_can_send_packet_now;
        hci_transport_h5->send_packet                   = &hci_transport_h5_send_packet;
        hci_transport_h5->set_baudrate                  = &hci_transport_h5_set_baudrate;
    }
    return (const hci_transport_t *) hci_transport_h5;
}
