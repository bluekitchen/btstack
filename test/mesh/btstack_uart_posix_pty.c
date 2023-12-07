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

#define BTSTACK_FILE__ "btstack_uart_posix.c"

/*
 *  btstack_uart_block_posix.c
 *
 *  Common code to access serial port via asynchronous block read/write commands
 *
 */

#include "btstack_uart_block.h"
#include "btstack_run_loop.h"
#include "btstack_debug.h"

#include <termios.h>  /* POSIX terminal control definitions */
#include <fcntl.h>    /* File control definitions */
#include <unistd.h>   /* UNIX standard function definitions */
#include <string.h>
#include <errno.h>
#ifdef __APPLE__
#include <sys/ioctl.h>
#include <IOKit/serial/ioss.h>
#endif

// uart config
static const btstack_uart_config_t * uart_config;

// data source for integration with BTstack Runloop
static btstack_data_source_t transport_data_source;

// block write
static int             write_bytes_len;
static const uint8_t * write_bytes_data;

// block read
static uint16_t  read_bytes_len;
static uint8_t * read_bytes_data;

// callbacks
static void (*block_sent)(void);
static void (*block_received)(void);


static int btstack_uart_posix_init(const btstack_uart_config_t * config){
    uart_config = config;
    return 0;
}

static void btstack_uart_posix_process_write(btstack_data_source_t *ds) {
    
    if (write_bytes_len == 0) return;

    uint32_t start = btstack_run_loop_get_time_ms();

    // write up to write_bytes_len to fd
    int bytes_written = (int) write(ds->source.fd, write_bytes_data, write_bytes_len);
    if (bytes_written < 0) {
        btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);
        return;
    }

    uint32_t end = btstack_run_loop_get_time_ms();
    if (end - start > 10){
        log_info("h4_process: write took %u ms", end - start);
    }

    write_bytes_data += bytes_written;
    write_bytes_len  -= bytes_written;

    if (write_bytes_len){
        btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);
        return;
    }

    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);

    // notify done
    if (block_sent){
        block_sent();
    }
}

static void btstack_uart_posix_process_read(btstack_data_source_t *ds) {

    if (read_bytes_len == 0) {
        log_info("btstack_uart_posix_process_read but no read requested");
        btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);
    }

    uint32_t start = btstack_run_loop_get_time_ms();
    
    // read up to bytes_to_read data in
    ssize_t bytes_read = read(ds->source.fd, read_bytes_data, read_bytes_len);
    // log_info("btstack_uart_posix_process_read need %u bytes, got %d", read_bytes_len, (int) bytes_read);
    uint32_t end = btstack_run_loop_get_time_ms();
    if (end - start > 10){
        log_info("h4_process: read took %u ms", end - start);
    }
    if (bytes_read < 0) return;
    
    read_bytes_len   -= bytes_read;
    read_bytes_data  += bytes_read;
    if (read_bytes_len > 0) return;
    
    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);

    if (block_received){
        block_received();
    }
}

static void hci_transport_h5_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    if (ds->source.fd < 0) return;
    switch (callback_type){
        case DATA_SOURCE_CALLBACK_READ:
            btstack_uart_posix_process_read(ds);
            break;
        case DATA_SOURCE_CALLBACK_WRITE:
            btstack_uart_posix_process_write(ds);
            break;
        default:
            break;
    }
}

static int btstack_uart_posix_set_baudrate(uint32_t baudrate){
    UNUSED(baudrate);
#if 0
    int fd = transport_data_source.source.fd;

    log_info("h4_set_baudrate %u", baudrate);

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

#else
    struct termios toptions;

    if (tcgetattr(fd, &toptions) < 0) {
        log_error("btstack_uart_posix_set_baudrate: Couldn't get term attributes");
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

    cfsetospeed(&toptions, brate);
    cfsetispeed(&toptions, brate);

    if( tcsetattr(fd, TCSANOW, &toptions) < 0) {
        log_error("btstack_uart_posix_set_baudrate: Couldn't set term attributes");
        return -1;
    }
#endif
#endif
    return 0;
}

static void btstack_uart_posix_set_parity_option(struct termios * toptions, int parity){
    if (parity){
        // enable even parity
        toptions->c_cflag |= PARENB;
    } else {
        // disable even parity
        toptions->c_cflag &= ~PARENB;
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
    struct termios toptions;
    if (tcgetattr(fd, &toptions) < 0) {
        log_error("btstack_uart_posix_set_parity: Couldn't get term attributes");
        return -1;
    }
    btstack_uart_posix_set_parity_option(&toptions, parity);
    if(tcsetattr(fd, TCSANOW, &toptions) < 0) {
        log_error("posix_set_parity: Couldn't set term attributes");
        return -1;
    }
    return 0;
}


static int btstack_uart_posix_set_flowcontrol(int flowcontrol){
    int fd = transport_data_source.source.fd;
    struct termios toptions;
    if (tcgetattr(fd, &toptions) < 0) {
        log_error("btstack_uart_posix_set_parity: Couldn't get term attributes");
        return -1;
    }
    btstack_uart_posix_set_flowcontrol_option(&toptions, flowcontrol);
    if(tcsetattr(fd, TCSANOW, &toptions) < 0) {
        log_error("posix_set_flowcontrol: Couldn't set term attributes");
        return -1;
    }
    return 0;
}

static int btstack_uart_posix_open(void){

    const char * device_name = uart_config->device_name;
    const int flowcontrol    = uart_config->flowcontrol;
    const uint32_t baudrate  = uart_config->baudrate;

    struct termios toptions;
    int flags = O_RDWR | O_NOCTTY | O_NONBLOCK;
    int fd = open(device_name, flags);
    if (fd == -1)  {
        log_error("posix_open: Unable to open port %s", device_name);
        return -1;
    }
    
    if (tcgetattr(fd, &toptions) < 0) {
        log_error("posix_open: Couldn't get term attributes");
        return -1;
    }
    
    cfmakeraw(&toptions);   // make raw

    // 8N1
    toptions.c_cflag &= ~CSTOPB;
    toptions.c_cflag |= CS8;

    toptions.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
    toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl
    
    // see: http://unixwiz.net/techtips/termios-vmin-vtime.html
    toptions.c_cc[VMIN]  = 1;
    toptions.c_cc[VTIME] = 0;
    
    // no parity
    btstack_uart_posix_set_parity_option(&toptions, 0);

    // flowcontrol
    btstack_uart_posix_set_flowcontrol_option(&toptions, flowcontrol);

    if(tcsetattr(fd, TCSANOW, &toptions) < 0) {
        log_error("posix_open: Couldn't set term attributes");
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
    btstack_run_loop_set_data_source_handler(&transport_data_source, &hci_transport_h5_process);
    btstack_run_loop_add_data_source(&transport_data_source);

    // wait a bit - at least cheap FTDI232 clones might send the first byte out incorrectly
    usleep(100000);

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
    block_received = block_handler;
}

static void btstack_uart_posix_set_block_sent( void (*block_handler)(void)){
    block_sent = block_handler;
}

static void btstack_uart_posix_send_block(const uint8_t *data, uint16_t size){
    // setup async write
    write_bytes_data = data;
    write_bytes_len  = size;

    // go
    // btstack_uart_posix_process_write(&transport_data_source);
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_WRITE);
}

static void btstack_uart_posix_receive_block(uint8_t *buffer, uint16_t len){
    read_bytes_data = buffer;
    read_bytes_len = len;
    btstack_run_loop_enable_data_source_callbacks(&transport_data_source, DATA_SOURCE_CALLBACK_READ);

    // go
    // btstack_uart_posix_process_read(&transport_data_source);
}

// static void btstack_uart_posix_set_sleep(uint8_t sleep){
// }
// static void btstack_uart_posix_set_csr_irq_handler( void (*csr_irq_handler)(void)){
// }

static const btstack_uart_block_t btstack_uart_posix = {
    /* int  (*init)(hci_transport_config_uart_t * config); */         &btstack_uart_posix_init,
    /* int  (*open)(void); */                                         &btstack_uart_posix_open,
    /* int  (*close)(void); */                                        &btstack_uart_posix_close_new,
    /* void (*set_block_received)(void (*handler)(void)); */          &btstack_uart_posix_set_block_received,
    /* void (*set_block_sent)(void (*handler)(void)); */              &btstack_uart_posix_set_block_sent,
    /* int  (*set_baudrate)(uint32_t baudrate); */                    &btstack_uart_posix_set_baudrate,
    /* int  (*set_parity)(int parity); */                             &btstack_uart_posix_set_parity,
    /* int  (*set_flowcontrol)(int flowcontrol); */                   &btstack_uart_posix_set_flowcontrol,
    /* void (*receive_block)(uint8_t *buffer, uint16_t len); */       &btstack_uart_posix_receive_block,
    /* void (*send_block)(const uint8_t *buffer, uint16_t length); */ &btstack_uart_posix_send_block,
    /* int (*get_supported_sleep_modes); */                           NULL,
    /* void (*set_sleep)(btstack_uart_sleep_mode_t sleep_mode); */    NULL,
    /* void (*set_wakeup_handler)(void (*handler)(void)); */          NULL,
    NULL, NULL, NULL, NULL,
};

const btstack_uart_block_t * btstack_uart_posix_instance(void){
	return &btstack_uart_posix;
}
