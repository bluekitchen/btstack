/*
 * Copyright (C) 2019 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btp_connection.c"

/*
 *  btp_connection.c
 *  
 *  Handles btp communication with auto-pts client via unix domain socket
 *
 */

#include "btp_socket.h"

#include "hci.h"
#include "btstack_debug.h"
#include "btstack_config.h"
#include "btp.h"
 
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#define BTP_PAYLOAD_LEN_MAX 1000
#define BTP_HEADER_LEN 5

#define LOG_BTP

/** prototypes */

/** globals */

typedef enum {
    SOCKET_W4_HEADER,
    SOCKET_W4_DATA
} SOCKET_STATE;

static btstack_data_source_t socket_ds;

static uint8_t  buffer[6+BTP_PAYLOAD_LEN_MAX];

static SOCKET_STATE state;
static uint16_t packet_bytes_read;
static uint16_t packet_bytes_to_read;

/** client packet handler */

static void (*btp_socket_packet_callback)(uint8_t service_id, uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t * data);

static void btp_socket_free_connection(){
    // remove from run_loop 
    btstack_run_loop_remove_data_source(&socket_ds);
    socket_ds.source.fd = 0;
}

static void btp_socket_init_statemachine(void){
    // wait for next packet
    state = SOCKET_W4_HEADER;
    packet_bytes_read = 0;
    packet_bytes_to_read = BTP_HEADER_LEN;
}

static void btp_socket_emit_connection_closed(void){
    log_info("Connection closed, emit BTP_ERROR_NOT_READY");
    const uint8_t status = BTP_ERROR_NOT_READY;
    (*btp_socket_packet_callback)(BTP_SERVICE_ID_CORE, BTP_OP_ERROR, BTP_INDEX_NON_CONTROLLER, 1, &status);
}

void btp_socket_process(btstack_data_source_t *socket_ds, btstack_data_source_callback_type_t callback_type) {
    // get socket_fd
    int socket_fd = socket_ds->source.fd;

    // read from socket
    int bytes_read = read(socket_fd, &buffer[packet_bytes_read], packet_bytes_to_read);
    if (bytes_read <= 0){
        // free connection
        btp_socket_free_connection();

        // connection broken (no particular channel, no date yet)
        btp_socket_emit_connection_closed();
        return;
    }

    packet_bytes_read    += bytes_read;
    packet_bytes_to_read -= bytes_read;
    if (packet_bytes_to_read > 0) return;
    
    bool dispatch = false;
    switch (state){
        case SOCKET_W4_HEADER:
            state = SOCKET_W4_DATA;
            packet_bytes_to_read = little_endian_read_16( buffer, 3);
            if (packet_bytes_to_read == 0){
                dispatch = true;
            }
            break;
        case SOCKET_W4_DATA:
            dispatch = true;
            break;
        default:
            break;
    }
    
    if (dispatch){
        // dispatch packet !!! connection, type, channel, data, size
        (*btp_socket_packet_callback)(buffer[0], buffer[1], buffer[2], little_endian_read_16( buffer, 3), &buffer[BTP_HEADER_LEN]);
        
        // reset state machine
        btp_socket_init_statemachine();
    }
}


/**
 * send BTP packet
 */
void btp_socket_send_packet(uint16_t service_id, uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data){
#ifdef LOG_BTP
    log_info("btp_socket_send_packet: service_id %0x, opcode %02x, controller_index 0x%02x, length %u", service_id, opcode, controller_index, length);
    log_info_hexdump(data, length);
#endif

    if (socket_ds.source.fd == 0) return;

    uint8_t header[BTP_HEADER_LEN];
    header[0] = service_id;
    header[1] = opcode;
    header[2] = controller_index;
    little_endian_store_16(header, 3, length);
    int res;
    res = write(socket_ds.source.fd, header, sizeof(header));
    res = write(socket_ds.source.fd, data, length);
    UNUSED(res);
}

/**
 * create socket connection to auto-pts client
 */
bool btp_socket_open_unix(const char * socket_name){
    int btp_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if(btp_socket == -1){
		return false;
	}

    struct sockaddr_un server;
    memset(&server, 0, sizeof(server));
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, socket_name);
    if (connect(btp_socket, (struct sockaddr *)&server, sizeof (server)) == -1){
        return false;
    };
    
    btstack_run_loop_set_data_source_handler(&socket_ds, &btp_socket_process);
    btstack_run_loop_set_data_source_fd(&socket_ds, btp_socket);
    btstack_run_loop_enable_data_source_callbacks(&socket_ds, DATA_SOURCE_CALLBACK_READ);
    
    // prepare state machine and
    btp_socket_init_statemachine();
    
    // add this socket to the run_loop
    btstack_run_loop_add_data_source( &socket_ds );

    return true;
}


/**
 * close socket connection to auto-pts client 
 */
bool btp_socket_close_unix(void){
    if (socket_ds.source.fd == 0) return false;
    shutdown(socket_ds.source.fd, SHUT_RDWR);
    btp_socket_free_connection();
    return true;
}

/**
 * Init socket connection module
 */
void btp_socket_init(void){
#if 0
    // just ignore broken sockets - NO_SO_SIGPIPE
    sig_t result = signal(SIGPIPE, SIG_IGN);
    if (result){
        log_error("btp_socket_init: failed to ignore SIGPIPE, error: %s", strerror(errno));
    }
#endif
}

/**
 * set packet handler
 */
void btp_socket_register_packet_handler( void (*packet_handler)(uint8_t service_id, uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data) ){
    btp_socket_packet_callback = packet_handler;
}

