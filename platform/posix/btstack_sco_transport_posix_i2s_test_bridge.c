/*
 * Copyright (C) 2021 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_sco_transport_posix_i2s_test_bridge.c"

/*
 * Implementation of btstack_sco_transport interface for posix systems with an i2s test bridge connected via UART
 * https://github.com/bluekitchen/i2s-test-bridge
 *
 * Assumptions: UART is faster (230400 baud) than I2S Data Rate (left channel only = 128 kbit/s)
 * This allows to let received block trigger new send. As sending is faster than I2S, there's no need for storing more
 * than the current block.
 *
 * If there is significant delay in higher layers, e.g. opening PortAudio on Mac takes about 50 ms, this would cause
 * a burst of received packets that get queued in the OS UART buffers. To work around, the first BRIDGE_RX_BLOCKS_DROP
 * are dropped. Expected period for SCO packets: 7.5 ms
 */

#include "btstack_sco_transport_posix_i2s_test_bridge.h"

#include "btstack_run_loop.h"
#include "btstack_debug.h"
#include "btstack_util.h"
#include <stdint.h>
#include <termios.h>  /* POSIX terminal control definitions */
#include <fcntl.h>    /* File control definitions */
#include <unistd.h>   /* UNIX standard function definitions */
#include <string.h>
#include <errno.h>
#ifdef __APPLE__
#include <sys/ioctl.h>
#include <IOKit/serial/ioss.h>
#endif

#define BRIDGE_BLOCK_SIZE_BYTES 120
#define BRIDGE_RX_BLOCKS_DROP 10

// data source for integration with BTstack Runloop
static btstack_data_source_t    sco_data_source;
static sco_format_t             sco_format;
static hci_con_handle_t         sco_handle;

// RX
static uint8_t  sco_rx_buffer[BRIDGE_BLOCK_SIZE_BYTES];
static uint16_t sco_rx_bytes_read;
static uint16_t sco_rx_counter;

// TX
static uint8_t                sco_tx_packet[BRIDGE_BLOCK_SIZE_BYTES];
static uint16_t               sco_tx_packet_pos;
static uint16_t               sco_tx_counter;

static void (*posix_i2s_test_bridge_packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size);

static void posix_i2s_test_bridge_process_write(btstack_data_source_t *ds) {
    ssize_t bytes_to_write = BRIDGE_BLOCK_SIZE_BYTES - sco_tx_packet_pos;
    ssize_t bytes_written = write(ds->source.fd, &sco_tx_packet[sco_tx_packet_pos], bytes_to_write);
    if (bytes_written < 0) {
        log_error("write returned error");
        return;
    }

    sco_tx_packet_pos += bytes_written;
    if (sco_tx_packet_pos < BRIDGE_BLOCK_SIZE_BYTES) {
        return;
    }

    // block sent
    btstack_run_loop_disable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_WRITE);
}

static void posix_i2s_test_bride_send_packet(const uint8_t *packet, uint16_t length){
    // ignore con handle
    btstack_assert( (packet[2]+3) == length);
    uint16_t i;
    switch (sco_format){
        case SCO_FORMAT_8_BIT:
            btstack_assert(length == 63);
            for (i=0;i<60;i++){
                big_endian_store_16(sco_tx_packet, i * 2, packet[ 3 + i ]);
            }
            break;
        case SCO_FORMAT_16_BIT:
            btstack_assert(length == 123);
            for (i=0;i<60;i++){
                sco_tx_packet[ i * 2    ] = packet[4 + i * 2];
                sco_tx_packet[ i * 2 + 1] = packet[3 + i * 2];
            }
            break;
        default:
            btstack_assert(false);
            break;
    }

    // start sending
    sco_tx_packet_pos = 0;
    btstack_run_loop_enable_data_source_callbacks(&sco_data_source, DATA_SOURCE_CALLBACK_WRITE);
}

static void posix_i2s_test_bridge_process_read(btstack_data_source_t *ds) {

    // read up to bytes_to_read data in
    ssize_t bytes_to_read = BRIDGE_BLOCK_SIZE_BYTES - sco_rx_bytes_read;
    ssize_t bytes_read = read(ds->source.fd, &sco_rx_buffer[sco_rx_bytes_read], bytes_to_read);
    if (bytes_read == 0){
        log_error("read zero bytes of %d bytes", (int) bytes_to_read);
        return;
    }
    if (bytes_read < 0) {
        log_error("read returned error");
        return;
    }
    sco_rx_bytes_read += bytes_read;
    if (sco_rx_bytes_read < BRIDGE_BLOCK_SIZE_BYTES) {
        return;
    }

    // block received
    sco_rx_bytes_read = 0;

    // already active
    if (sco_handle == HCI_CON_HANDLE_INVALID) {
        log_info("drop SCO packet, no con Handle yet");
        return;
    }

    // drop first packets
    if (sco_rx_counter < BRIDGE_RX_BLOCKS_DROP) {
        sco_rx_counter++;
        log_info("drop packet %u/%u\n", sco_rx_counter, BRIDGE_RX_BLOCKS_DROP);
        return;
    }

    // setup SCO header
    uint8_t packet[BRIDGE_BLOCK_SIZE_BYTES + 3];
    little_endian_store_16(packet,0, sco_handle);
    uint16_t index;
    switch (sco_format) {
        case SCO_FORMAT_8_BIT:
            // data is received big endian and transparent data is in lower byte
            packet[2] = BRIDGE_BLOCK_SIZE_BYTES / 2;
            for (index= 0 ; index < (BRIDGE_BLOCK_SIZE_BYTES / 2) ; index++) {
                packet[3+index] = sco_rx_buffer[2 * index + 1];
            }
            break;
        case SCO_FORMAT_16_BIT:
            // data is received big endian but sco packet contains little endian data -> swap bytes
            packet[2] = BRIDGE_BLOCK_SIZE_BYTES;
            for (index = 0 ; index < (BRIDGE_BLOCK_SIZE_BYTES / 2) ; index++) {
                packet[3 + 2 * index] = sco_rx_buffer[2 * index + 1];
                packet[4 + 2 * index] = sco_rx_buffer[2 * index];
            }
            break;
        default:
            btstack_assert(false);
            break;
    }
    (*posix_i2s_test_bridge_packet_handler)(HCI_SCO_DATA_PACKET, packet, 3 + packet[2]);
}

static void hci_uart_posix_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type) {
    if (ds->source.fd < 0) return;
    switch (callback_type){
        case DATA_SOURCE_CALLBACK_READ:
            posix_i2s_test_bridge_process_read(ds);
            break;
        case DATA_SOURCE_CALLBACK_WRITE:
            posix_i2s_test_bridge_process_write(ds);
            break;
        default:
            break;
    }
}

static int posix_i2s_test_bridge_set_baudrate(int fd, uint32_t baudrate){

#ifdef __APPLE__

    // From https://developer.apple.com/library/content/samplecode/SerialPortSample/Listings/SerialPortSample_SerialPortSample_c.html

    // The IOSSIOSPEED ioctl can be used to set arbitrary baud rates
    // other than those specified by POSIX. The driver for the underlying serial hardware
    // ultimately determines which baud rates can be used. This ioctl sets both the input
    // and output speed.

    speed_t speed = baudrate;
    if (ioctl(fd, IOSSIOSPEED, &speed) == -1) {
        log_error("set baud: error calling ioctl(..., IOSSIOSPEED, %u) - %s(%d).\n", baudrate, strerror(errno), errno);
        return -1;
    }

#else
    struct termios toptions;

    if (tcgetattr(fd, &toptions) < 0) {
        log_error("set baud: Couldn't get term attributes");
        return -1;
    }

    speed_t brate = baudrate; // let you override switch below if needed
    switch(baudrate) {
        case    9600: brate=B9600;    break;
        case   19200: brate=B19200;   break;
        case   38400: brate=B38400;   break;
        case   57600: brate=B57600;   break;
        case  115200: brate=B115200;  break;
#ifdef B230400
        case  230400: brate=B230400;  break;
#endif
#ifdef B460800
        case  460800: brate=B460800;  break;
#endif
#ifdef B500000
        case  500000: brate=B500000;  break;
#endif
#ifdef B576000
        case  576000: brate=B576000;  break;
#endif
#ifdef B921600
        case  921600: brate=B921600;  break;
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
    cfsetospeed(&toptions, brate);
    cfsetispeed(&toptions, brate);

    if( tcsetattr(fd, TCSANOW, &toptions) < 0) {
        log_error("Couldn't set term attributes");
        return -1;
    }
#endif

    return 0;
}

static int posix_i2s_test_bridge_init(const char * device_path){

    const uint32_t baudrate  = 230400;

    struct termios toptions;
    int flags = O_RDWR | O_NOCTTY | O_NONBLOCK;
    int fd = open(device_path, flags);
    if (fd == -1)  {
        log_error("Unable to open port %s", device_path);
        return -1;
    }

    if (tcgetattr(fd, &toptions) < 0) {
        log_error("Couldn't get term attributes");
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

    if(tcsetattr(fd, TCSANOW, &toptions) < 0) {
        log_error("Couldn't set term attributes");
        return -1;
    }

    // also set baudrate
    if (posix_i2s_test_bridge_set_baudrate(fd, baudrate) < 0){
        return -1;
    }

    log_info("I2S Test Bridge initialized for %u baud", baudrate);

    // set up data_source
    btstack_run_loop_set_data_source_fd(&sco_data_source, fd);
    btstack_run_loop_set_data_source_handler(&sco_data_source, &hci_uart_posix_process);
    btstack_run_loop_add_data_source(&sco_data_source);

    // reset and enable rx
    sco_rx_bytes_read = 0;
    btstack_run_loop_enable_data_source_callbacks(&sco_data_source, DATA_SOURCE_CALLBACK_READ);

    // wait a bit - at least cheap FTDI232 clones might send the first byte out incorrectly
    usleep(100000);

    return 0;
}

static void posix_i2s_test_bridge_open(hci_con_handle_t con_handle, sco_format_t format){
    log_info("open: handle 0x%04x, format %s", con_handle, (format == SCO_FORMAT_16_BIT) ? "16 bit" : "8 bit");
    // store config
    sco_format = format;
    sco_handle = con_handle;
    // reset rx
    sco_rx_counter = 0;
    // reset tx
    sco_tx_counter = 0;
}

static void posix_i2s_test_bridge_close(hci_con_handle_t con_handle){
    log_info("close: handle 0x%04x", con_handle);
    sco_handle = HCI_CON_HANDLE_INVALID;
}

static void posix_i2s_test_bridge_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    posix_i2s_test_bridge_packet_handler = handler;
}

const btstack_sco_transport_t * btstack_sco_transport_posix_i2s_test_bridge_init_instance(const char * device_path){
    static const btstack_sco_transport_t transport = {
        .open                    = posix_i2s_test_bridge_open,
        .close                   = posix_i2s_test_bridge_close,
        .register_packet_handler = posix_i2s_test_bridge_register_packet_handler,
        .send_packet             = posix_i2s_test_bride_send_packet,
    };
    int err = posix_i2s_test_bridge_init(device_path);
    if (err > 0) {
        return NULL;
    }
    sco_format = SCO_FORMAT_8_BIT;
    sco_handle = HCI_CON_HANDLE_INVALID;
    return &transport;
}
