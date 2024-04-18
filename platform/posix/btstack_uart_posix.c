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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

#define BTSTACK_FILE__ "btstack_uart_posix.c"

/*
 *  btstack_uart_posix.c
 *
 *  Common code to access serial port via asynchronous block read/write commands
 *
 */

#include "btstack_uart.h"
#include "btstack_run_loop.h"
#include "btstack_debug.h"

#include <termios.h>  /* POSIX terminal control definitions */
#include <fcntl.h>    /* File control definitions */
#include <unistd.h>   /* UNIX standard function definitions */
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#ifdef __APPLE__
#include <sys/ioctl.h>
#include <IOKit/serial/ioss.h>
#endif

// uart config
static const btstack_uart_config_t * uart_config;

// on macOS 12.1, CTS/RTS control flags are always read back as zero.
// To work around this, we cache our terios settings
struct termios btstack_uart_block_termios;

// data source for integration with BTstack Runloop
static btstack_data_source_t transport_data_source;

// block write
static int             btstack_uart_block_write_bytes_len;
static const uint8_t * btstack_uart_block_write_bytes_data;

// block read
static uint16_t  btstack_uart_block_read_bytes_len;
static uint8_t * btstack_uart_block_read_bytes_data;

// callbacks
static void (*block_sent)(void);
static void (*block_received)(void);


static int btstack_uart_posix_init(const btstack_uart_config_t * config){
    uart_config = config;
    return 0;
}

static void hci_uart_posix_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type);

static void btstack_uart_block_posix_process_write(btstack_data_source_t *ds) {
    
    if (btstack_uart_block_write_bytes_len == 0) return;

    uint32_t start = btstack_run_loop_get_time_ms();

    // write up to write_bytes_len to fd
    int bytes_written = (int) write(ds->source.fd, btstack_uart_block_write_bytes_data, btstack_uart_block_write_bytes_len);
    uint32_t end = btstack_run_loop_get_time_ms();
    if (end - start > 10){
        log_info("write took %u ms", end - start);
    }
    if (bytes_written == 0){
        log_error("wrote zero bytes\n");
        exit(EXIT_FAILURE);
    }
    if (bytes_written < 0) {
        log_error("write returned error\n");
        btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);
        exit(EXIT_FAILURE);
    }

    btstack_uart_block_write_bytes_data += bytes_written;
    btstack_uart_block_write_bytes_len  -= bytes_written;

    if (btstack_uart_block_write_bytes_len){
        btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);
        return;
    }

    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);

    // notify done
    if (block_sent){
        block_sent();
    }
}

static void btstack_uart_block_posix_process_read(btstack_data_source_t *ds) {

    if (btstack_uart_block_read_bytes_len == 0) {
        log_info("called but no read pending");
        btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);
    }

    uint32_t start = btstack_run_loop_get_time_ms();
    
    // read up to bytes_to_read data in
    ssize_t bytes_read = read(ds->source.fd, btstack_uart_block_read_bytes_data, btstack_uart_block_read_bytes_len);
    // log_info("read need %u bytes, got %d", btstack_uart_block_read_bytes_len, (int) bytes_read);
    uint32_t end = btstack_run_loop_get_time_ms();
    if (end - start > 10){
        log_info("read took %u ms", end - start);
    }
    if (bytes_read == 0){
        log_error("read zero bytes\n");
        return;
    }
    if (bytes_read < 0) {
        log_error("read returned error\n");
        return;
    }

    btstack_uart_block_read_bytes_len   -= bytes_read;
    btstack_uart_block_read_bytes_data  += bytes_read;
    if (btstack_uart_block_read_bytes_len > 0) return;
    
    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);

    if (block_received){
        block_received();
    }
}

static int btstack_uart_posix_set_baudrate(uint32_t baudrate){

    int fd = transport_data_source.source.fd;

    log_info("h4_set_baudrate %u", baudrate);

#ifndef __APPLE__

    speed_t brate = baudrate; // let you override switch below if needed
    switch(baudrate) {
        case    9600: brate=B9600;    break;
        case   19200: brate=B19200;   break;
        case   38400: brate=B38400;   break;
        case 57600:  brate=B57600;  break;
        case 115200: brate=B115200; break;
#ifdef B230400
        case 230400: brate=B230400; break;
#endif
#ifdef B460800
        case 460800: brate=B460800; break;
#endif
#ifdef B500000
        case  500000: brate=B500000;  break;
#endif
#ifdef B576000
        case  576000: brate=B576000;  break;
#endif
#ifdef B921600
        case 921600: brate=B921600; break;
#endif
#ifdef B1000000
        case 1000000: brate=B1000000; break;
#endif
#ifdef B1152000
        case 1152000: brate=B1152000; break;
#endif
#ifdef B1500000
        case 1500000: brate=B1500000; break;
#endif
#ifdef B2000000
        case 2000000: brate=B2000000; break;
#endif
#ifdef B2500000
        case 2500000: brate=B2500000; break;
#endif
#ifdef B3000000
        case 3000000: brate=B3000000; break;
#endif
#ifdef B3500000
        case 3500000: brate=B3500000; break;
#endif
#ifdef B400000
        case 4000000: brate=B4000000; break;
#endif
        default:
            log_error("can't set baudrate %dn", baudrate );
            return -1;
    }
    cfsetospeed(&btstack_uart_block_termios, brate);
    cfsetispeed(&btstack_uart_block_termios, brate);
#endif

    // also set options for __APPLE__ to enforce write drain
    // Mac OS Mojave: tcsdrain did not work as expected

    if( tcsetattr(fd, TCSADRAIN, &btstack_uart_block_termios) < 0) {
        log_error("Couldn't set term attributes");
        return -1;
    }

#ifdef __APPLE__
    // From https://developer.apple.com/library/content/samplecode/SerialPortSample/Listings/SerialPortSample_SerialPortSample_c.html

    // The IOSSIOSPEED ioctl can be used to set arbitrary baud rates
    // other than those specified by POSIX. The driver for the underlying serial hardware
    // ultimately determines which baud rates can be used. This ioctl sets both the input
    // and output speed.

    speed_t speed = baudrate;
    if (ioctl(fd, IOSSIOSPEED, &speed) == -1) {
        log_error("btstack_uart_posix_set_baudrate: error calling ioctl(..., IOSSIOSPEED, %u) - %s(%d).\n", baudrate, strerror(errno), errno);
        return -1;
    }
#endif

    return 0;
}

static void btstack_uart_posix_set_parity_option(struct termios * toptions, int parity){
    switch (parity){
        case BTSTACK_UART_PARITY_OFF:
            toptions->c_cflag &= ~PARENB;
            toptions->c_cflag &= ~PARODD;
            break;
        case BTSTACK_UART_PARITY_EVEN:
            toptions->c_cflag |=  PARENB;
            toptions->c_cflag &= ~PARODD;
            break;
        case BTSTACK_UART_PARITY_ODD:
            toptions->c_cflag |= PARENB;
            toptions->c_cflag |= PARODD;
        default:
            break;
    }
}

static void btstack_uart_posix_set_flowcontrol_option(struct termios * toptions, int flowcontrol){
    if (flowcontrol) {
        // with flow control
        toptions->c_cflag |= CRTSCTS;
    } else {
        // no flow control
        toptions->c_cflag &= ~CRTSCTS;
    }
}

static int btstack_uart_posix_set_parity(int parity){
    int fd = transport_data_source.source.fd;
    btstack_uart_posix_set_parity_option(&btstack_uart_block_termios, parity);
    if(tcsetattr(fd, TCSANOW, &btstack_uart_block_termios) < 0) {
        log_error("Couldn't set term attributes");
        return -1;
    }
    return 0;
}


static int btstack_uart_posix_set_flowcontrol(int flowcontrol){
    int fd = transport_data_source.source.fd;
    btstack_uart_posix_set_flowcontrol_option(&btstack_uart_block_termios, flowcontrol);
    if(tcsetattr(fd, TCSANOW, &btstack_uart_block_termios) < 0) {
        log_error("Couldn't set term attributes");
        return -1;
    }
    return 0;
}

static int btstack_uart_posix_open(void){

    const char * device_name = uart_config->device_name;
    const uint32_t baudrate  = uart_config->baudrate;
    const int flowcontrol    = uart_config->flowcontrol;
    const int parity         = uart_config->parity;

    int flags = O_RDWR | O_NOCTTY | O_NONBLOCK;
    int fd = open(device_name, flags);
    if (fd == -1)  {
        log_error("Unable to open port %s", device_name);
        return -1;
    }
    
    if (tcgetattr(fd, &btstack_uart_block_termios) < 0) {
        log_error("Couldn't get term attributes");
        return -1;
    }
    cfmakeraw(&btstack_uart_block_termios);   // make raw

    // 8N1
    btstack_uart_block_termios.c_cflag &= ~CSTOPB;
    btstack_uart_block_termios.c_cflag |= CS8;

    btstack_uart_block_termios.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
    btstack_uart_block_termios.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl
    
    // see: http://unixwiz.net/techtips/termios-vmin-vtime.html
    btstack_uart_block_termios.c_cc[VMIN]  = 1;
    btstack_uart_block_termios.c_cc[VTIME] = 0;
    
    // no parity
    btstack_uart_posix_set_parity_option(&btstack_uart_block_termios, parity);

    // flowcontrol
    btstack_uart_posix_set_flowcontrol_option(&btstack_uart_block_termios, flowcontrol);

    if(tcsetattr(fd, TCSANOW, &btstack_uart_block_termios) < 0) {
        log_error("Couldn't set term attributes");
        return -1;
    }

    // store fd in data source
    transport_data_source.source.fd = fd;
    
    // also set baudrate
    if (btstack_uart_posix_set_baudrate(baudrate) < 0){
        return -1;
    }

    // set up data_source
    btstack_run_loop_set_data_source_fd(&transport_data_source, fd);
    btstack_run_loop_set_data_source_handler(&transport_data_source, &hci_uart_posix_process);
    btstack_run_loop_add_data_source(&transport_data_source);

    // wait a bit - at least cheap FTDI232 clones might send the first byte out incorrectly
    usleep(100000);

    log_info("Open tty %s", device_name);
    return 0;
} 

static int btstack_uart_posix_close_new(void){

    // first remove run loop handler
    btstack_run_loop_remove_data_source(&transport_data_source);
    
    // then close device 
    close(transport_data_source.source.fd);
    transport_data_source.source.fd = -1;
    return 0;
}

static void btstack_uart_posix_set_block_received( void (*block_handler)(void)){
    btstack_uart_block_read_bytes_len = 0;
    block_received = block_handler;
}

static void btstack_uart_posix_set_block_sent( void (*block_handler)(void)){
    btstack_uart_block_write_bytes_len = 0;
    block_sent = block_handler;
}

static void btstack_uart_posix_send_block(const uint8_t *data, uint16_t size){
    btstack_assert(btstack_uart_block_write_bytes_len == 0);

    // setup async write
    btstack_uart_block_write_bytes_data = data;
    btstack_uart_block_write_bytes_len  = size;
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_WRITE);
}

static void btstack_uart_posix_receive_block(uint8_t *buffer, uint16_t len){
    btstack_assert(btstack_uart_block_read_bytes_len == 0);

    // setup async read
    btstack_uart_block_read_bytes_data = buffer;
    btstack_uart_block_read_bytes_len = len;
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_READ);
}

#ifdef ENABLE_H5

// SLIP Implementation Start
#include "btstack_slip.h"

// max size of outgoing SLIP chunks 
#define SLIP_TX_CHUNK_LEN   128

#define SLIP_RECEIVE_BUFFER_SIZE 128

// encoded SLIP chunk
static uint8_t   btstack_uart_slip_outgoing_buffer[SLIP_TX_CHUNK_LEN+1];

// block write
static int             btstack_uart_slip_write_bytes_len;
static const uint8_t * btstack_uart_slip_write_bytes_data;
static int             btstack_uart_slip_write_active;

// block read
static uint8_t         btstack_uart_slip_receive_buffer[SLIP_RECEIVE_BUFFER_SIZE];
static uint16_t        btstack_uart_slip_receive_pos;
static uint16_t        btstack_uart_slip_receive_len;
static uint8_t         btstack_uart_slip_receive_track_start;
static uint32_t        btstack_uart_slip_receive_start_time;
static int             btstack_uart_slip_receive_active;

// callbacks
static void (*frame_sent)(void);
static void (*frame_received)(uint16_t frame_size);

static void btstack_uart_slip_posix_block_sent(void);

static void btstack_uart_slip_posix_process_write(btstack_data_source_t *ds) {
    
    if (btstack_uart_slip_write_bytes_len == 0) return;

    uint32_t start = btstack_run_loop_get_time_ms();

    // write up to btstack_uart_slip_write_bytes_len to fd
    int bytes_written = (int) write(ds->source.fd, btstack_uart_slip_write_bytes_data, btstack_uart_slip_write_bytes_len);
    if (bytes_written < 0) {
        btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);
        return;
    }

    uint32_t end = btstack_run_loop_get_time_ms();
    if (end - start > 10){
        log_info("write took %u ms", end - start);
    }

    btstack_uart_slip_write_bytes_data += bytes_written;
    btstack_uart_slip_write_bytes_len  -= bytes_written;

    if (btstack_uart_slip_write_bytes_len){
        btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);
        return;
    }

    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);

    // done with TX chunk
    btstack_uart_slip_posix_block_sent();
}

// @returns frame size if complete frame decoded and delivered
static uint16_t btstack_uart_slip_posix_process_buffer(void){
    log_debug("process buffer: pos %u, len %u", btstack_uart_slip_receive_pos, btstack_uart_slip_receive_len);

    uint16_t frame_size = 0;
    while (btstack_uart_slip_receive_pos < btstack_uart_slip_receive_len && frame_size == 0){
        btstack_slip_decoder_process(btstack_uart_slip_receive_buffer[btstack_uart_slip_receive_pos++]);
        frame_size = btstack_slip_decoder_frame_size();
    }

    // reset buffer if fully processed
    if (btstack_uart_slip_receive_pos == btstack_uart_slip_receive_len ){
        btstack_uart_slip_receive_len = 0;
        btstack_uart_slip_receive_pos = 0;
    }

    // deliver frame if frame complete
    if (frame_size) {

        // receive done
        btstack_uart_slip_receive_active = 0;

        // only print if read was involved
        if (btstack_uart_slip_receive_track_start == 0){
            log_info("frame receive time %u ms", btstack_run_loop_get_time_ms() - btstack_uart_slip_receive_start_time);
            btstack_uart_slip_receive_start_time = 0;
        }

        (*frame_received)(frame_size);
    }

    return frame_size;
}

static void btstack_uart_slip_posix_process_read(btstack_data_source_t *ds) {

    uint32_t start = btstack_run_loop_get_time_ms();

    if (btstack_uart_slip_receive_track_start){
        btstack_uart_slip_receive_track_start = 0;
        btstack_uart_slip_receive_start_time = start;
    }
    
    // read up to bytes_to_read data in
    ssize_t bytes_read = read(ds->source.fd, btstack_uart_slip_receive_buffer, SLIP_RECEIVE_BUFFER_SIZE);

    log_debug("requested %u bytes, got %d", SLIP_RECEIVE_BUFFER_SIZE, (int) bytes_read);
    uint32_t end = btstack_run_loop_get_time_ms();
    if (end - start > 10){
        log_info("read took %u ms", end - start);
    }
    if (bytes_read < 0) return;
    
    btstack_uart_slip_receive_pos = 0;
    btstack_uart_slip_receive_len = (uint16_t ) bytes_read;

    btstack_uart_slip_posix_process_buffer();
}

// -----------------------------
// SLIP ENCODING

static void btstack_uart_slip_posix_encode_chunk_and_send(void){
    uint16_t pos = 0;
    while (btstack_slip_encoder_has_data() & (pos < SLIP_TX_CHUNK_LEN)) {
        btstack_uart_slip_outgoing_buffer[pos++] = btstack_slip_encoder_get_byte();
    }

    // setup async write and start sending
    log_debug("slip: send %d bytes", pos);
    btstack_uart_slip_write_bytes_data = btstack_uart_slip_outgoing_buffer;
    btstack_uart_slip_write_bytes_len  = pos;
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_WRITE);
}

static void btstack_uart_slip_posix_block_sent(void){
    // check if more data to send
    if (btstack_slip_encoder_has_data()){
        btstack_uart_slip_posix_encode_chunk_and_send();
        return;
    }

    // write done
    btstack_uart_slip_write_active = 0;

    // notify done
    if (frame_sent){
        frame_sent();
    }
}

static void btstack_uart_slip_posix_send_frame(const uint8_t * frame, uint16_t frame_size){

    // write started
    btstack_uart_slip_write_active = 1;

    // Prepare encoding of Header + Packet (+ DIC)
    btstack_slip_encoder_start(frame, frame_size);

    // Fill rest of chunk from packet and send
    btstack_uart_slip_posix_encode_chunk_and_send();
}

// SLIP ENCODING
// -----------------------------

static void btstack_uart_slip_posix_receive_frame(uint8_t *buffer, uint16_t len){

    // receive started
    btstack_uart_slip_receive_active = 1;

    log_debug("receive block, size %u", len);
    btstack_uart_slip_receive_track_start = 1;

    // setup SLIP decoder
    btstack_slip_decoder_init(buffer, len);

    // process bytes received in earlier read. might deliver packet, which in turn will call us again. 
    // just make sure to exit right away
    if (btstack_uart_slip_receive_len){
        int frame_found = btstack_uart_slip_posix_process_buffer();
        if (frame_found) return;
    }

    // no frame delivered, enable posix read
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_READ);
}



static void btstack_uart_slip_posix_set_frame_received( void (*block_handler)(uint16_t frame_size)){
    frame_received = block_handler;
}

static void btstack_uart_slip_posix_set_frame_sent( void (*block_handler)(void)){
    frame_sent = block_handler;
}

// SLIP Implementation End
#endif

// dispatch into block or SLIP code
static void hci_uart_posix_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    if (ds->source.fd < 0) return;
    switch (callback_type){
        case DATA_SOURCE_CALLBACK_READ:
#ifdef ENABLE_H5
            if (btstack_uart_slip_receive_active){
                btstack_uart_slip_posix_process_read(ds);
            } else
#endif
            {
                btstack_uart_block_posix_process_read(ds);
            }
            break;
        case DATA_SOURCE_CALLBACK_WRITE:
#ifdef ENABLE_H5
            if (btstack_uart_slip_write_active){
                btstack_uart_slip_posix_process_write(ds);
            } else
#endif
            {
                btstack_uart_block_posix_process_write(ds);
            }
            break;
        default:
            break;
    }
}

static const btstack_uart_t btstack_uart_posix = {
    /* int  (*init)(hci_transport_config_uart_t * config); */              &btstack_uart_posix_init,
    /* int  (*open)(void); */                                              &btstack_uart_posix_open,
    /* int  (*close)(void); */                                             &btstack_uart_posix_close_new,
    /* void (*set_block_received)(void (*handler)(void)); */               &btstack_uart_posix_set_block_received,
    /* void (*set_block_sent)(void (*handler)(void)); */                   &btstack_uart_posix_set_block_sent,
    /* int  (*set_baudrate)(uint32_t baudrate); */                         &btstack_uart_posix_set_baudrate,
    /* int  (*set_parity)(int parity); */                                  &btstack_uart_posix_set_parity,
    /* int  (*set_flowcontrol)(int flowcontrol); */                        &btstack_uart_posix_set_flowcontrol,
    /* void (*receive_block)(uint8_t *buffer, uint16_t len); */            &btstack_uart_posix_receive_block,
    /* void (*send_block)(const uint8_t *buffer, uint16_t length); */      &btstack_uart_posix_send_block,
    /* int (*get_supported_sleep_modes); */                                NULL,
    /* void (*set_sleep)(btstack_uart_sleep_mode_t sleep_mode); */         NULL,
    /* void (*set_wakeup_handler)(void (*handler)(void)); */               NULL,

#ifdef ENABLE_H5
    /* void (*set_frame_received)(void (*handler)(uint16_t frame_size); */ &btstack_uart_slip_posix_set_frame_received,
    /* void (*set_fraae_sent)(void (*handler)(void)); */                   &btstack_uart_slip_posix_set_frame_sent,
    /* void (*receive_frame)(uint8_t *buffer, uint16_t len); */            &btstack_uart_slip_posix_receive_frame,
    /* void (*send_frame)(const uint8_t *buffer, uint16_t length); */      &btstack_uart_slip_posix_send_frame,
#else
    NULL, NULL, NULL, NULL,
#endif
};

const btstack_uart_t * btstack_uart_posix_instance(void){
	return &btstack_uart_posix;
}
