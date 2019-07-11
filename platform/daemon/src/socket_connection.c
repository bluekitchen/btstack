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

#define BTSTACK_FILE__ "socket_connection.c"

/*
 *  SocketServer.c
 *  
 *  Handles multiple connections to a single socket without blocking
 *
 *  Created by Matthias Ringwald on 6/6/09.
 *
 */

#include "socket_connection.h"

#include "hci.h"
#include "btstack_debug.h"

#include "btstack_config.h"

#include "btstack.h"
#include "btstack_client.h"
 
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif
 
#ifdef _WIN32
#include "Winsock2.h"
// define missing types
typedef int32_t socklen_t;
//
#define UNIX_PATH_MAX 108
struct sockaddr_un {
    uint16_t sun_family;
    char sun_path[UNIX_PATH_MAX];  
};
//
#endif

// has been missing on mingw32 in the past
#ifndef S_IRWXG
#define S_IRWXG 0
#endif
#ifndef S_IRWXO
#define S_IRWXO 0
#endif

#ifdef USE_LAUNCHD
#include "../port/ios/3rdparty/launch.h"
#endif

#define MAX_PENDING_CONNECTIONS 10

/** prototypes */
static void socket_connection_hci_process(btstack_data_source_t *ds, btstack_data_source_callback_type_t callback_type);
static int socket_connection_dummy_handler(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t length);

/** globals */

/** packet header used over socket connections, in front of the HCI packet */
typedef struct packet_header {
    uint16_t type;
    uint16_t channel;
    uint16_t length;
    uint8_t  data[0];
} packet_header_t;  // 6

typedef enum {
    SOCKET_W4_HEADER,
    SOCKET_W4_DATA
} SOCKET_STATE;

typedef struct linked_connection {
    btstack_linked_item_t item;
    connection_t * connection;
} linked_connection_t;

struct connection {
    btstack_data_source_t ds;                // used for run loop
    linked_connection_t linked_connection;   // used for connection list
    int socket_fd;                           // ds only stores event handle in win32
    SOCKET_STATE state;
    uint16_t bytes_read;
    uint16_t bytes_to_read;
    uint8_t  buffer[6+HCI_ACL_BUFFER_SIZE]; // packet_header(6) + max packet: 3-DH5 = header(6) + payload (1021)
};

/** list of socket connections */
static btstack_linked_list_t connections = NULL;
static btstack_linked_list_t parked = NULL;

#ifdef _WIN32
// workaround as btstack_data_source_t only stores windows event (instead of fd)
static int tcp_socket_fd;
#endif

/** client packet handler */

static int (*socket_connection_packet_callback)(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t length) = socket_connection_dummy_handler;

static int socket_connection_dummy_handler(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t length){
    UNUSED(connection); 
    UNUSED(packet_type); 
    UNUSED(channel); 
    UNUSED(data); 
    UNUSED(length);
    return 0;
}

static void socket_connection_free_connection(connection_t *conn){
    // remove from run_loop 
    btstack_run_loop_remove_data_source(&conn->ds);
    
    // and from connection list
    btstack_linked_list_remove(&connections, &conn->linked_connection.item);
    
#ifdef _WIN32
    if (conn->ds.source.handle){
        WSACloseEvent(conn->ds.source.handle);
    }
#endif

    // destroy
    free(conn);
}

static void socket_connection_init_statemachine(connection_t *connection){
    // wait for next packet
    connection->state = SOCKET_W4_HEADER;
    connection->bytes_read = 0;
    connection->bytes_to_read = sizeof(packet_header_t);
}

static connection_t * socket_connection_register_new_connection(int fd){
    // create connection objec 
    connection_t * conn = malloc( sizeof(connection_t));
    if (conn == NULL) return NULL;
    memset(conn, 0, sizeof(connection_t));
    // store reference from linked item to base object
    conn->linked_connection.connection = conn;

    // keep fd around
    conn->socket_fd = fd;

#ifdef _WIN32
    // wrap fd in windows event and configure for accept and close
    WSAEVENT event = WSACreateEvent();
    if (!event){
        log_error("Error creating WSAEvent for socket");
        return NULL;
    }
    int res = WSAEventSelect(fd, event, FD_READ | FD_WRITE | FD_CLOSE);
    if (res == SOCKET_ERROR){
        log_error("WSAEventSelect() error: %d\n" , WSAGetLastError());
        free(conn);
        return NULL;
    }
#endif

    btstack_run_loop_set_data_source_handler(&conn->ds, &socket_connection_hci_process);
#ifdef _WIN32
    conn->ds.source.handle = event;
#else
    btstack_run_loop_set_data_source_fd(&conn->ds, fd);
#endif
    btstack_run_loop_enable_data_source_callbacks(&conn->ds, DATA_SOURCE_CALLBACK_READ);
    
    // prepare state machine and
    socket_connection_init_statemachine(conn);
    
    // add this socket to the run_loop
    btstack_run_loop_add_data_source( &conn->ds );
    
    // and the connection list
    btstack_linked_list_add( &connections, &conn->linked_connection.item);
    
    return conn;
}

void static socket_connection_emit_connection_opened(connection_t *connection){
    uint8_t event[1];
    event[0] = DAEMON_EVENT_CONNECTION_OPENED;
    (*socket_connection_packet_callback)(connection, DAEMON_EVENT_PACKET, 0, (uint8_t *) &event, 1);
}

void static socket_connection_emit_connection_closed(connection_t *connection){
    uint8_t event[1];
    event[0] = DAEMON_EVENT_CONNECTION_CLOSED;
    (*socket_connection_packet_callback)(connection, DAEMON_EVENT_PACKET, 0, (uint8_t *) &event, 1);
}

void socket_connection_hci_process(btstack_data_source_t *socket_ds, btstack_data_source_callback_type_t callback_type) {
    UNUSED(callback_type);
    connection_t *conn = (connection_t *) socket_ds;

    log_debug("socket_connection_hci_process, callback %x", callback_type);

    // get socket_fd
    int socket_fd = conn->socket_fd;

#ifdef _WIN32
    // sync state
    WSANETWORKEVENTS network_events;
    if (WSAEnumNetworkEvents(socket_fd, socket_ds->source.handle, &network_events) == SOCKET_ERROR){
        log_error("WSAEnumNetworkEvents() failed with error %d\n", WSAGetLastError());
        return;
    }
    // check if read possible
    if ((network_events.lNetworkEvents & FD_READ) == 0) return;
#endif

    // read from socket
#ifdef _WIN32
    int flags = 0;
    int bytes_read = recv(socket_fd, (char*) &conn->buffer[conn->bytes_read], conn->bytes_to_read, flags);
#else
    int bytes_read = read(socket_fd, &conn->buffer[conn->bytes_read], conn->bytes_to_read);
#endif

    log_debug("socket_connection_hci_process fd %x, bytes read %d", socket_fd, bytes_read);
    if (bytes_read <= 0){
        // connection broken (no particular channel, no date yet)
        socket_connection_emit_connection_closed(conn);
        
        // free connection
        socket_connection_free_connection(conn);
        
        return;
    }
    conn->bytes_read += bytes_read;
    conn->bytes_to_read -= bytes_read;
    if (conn->bytes_to_read > 0) return;
    
    int dispatch = 0;
    switch (conn->state){
        case SOCKET_W4_HEADER:
            conn->state = SOCKET_W4_DATA;
            conn->bytes_to_read = little_endian_read_16( conn->buffer, 4);
            if (conn->bytes_to_read == 0){
                dispatch = 1;
            }
            break;
        case SOCKET_W4_DATA:
            dispatch = 1;
            break;
        default:
            break;
    }
    
    if (dispatch){
        // dispatch packet !!! connection, type, channel, data, size
        int dispatch_err = (*socket_connection_packet_callback)(conn, little_endian_read_16( conn->buffer, 0), little_endian_read_16( conn->buffer, 2),
                                                            &conn->buffer[sizeof(packet_header_t)], little_endian_read_16( conn->buffer, 4));
        
        // reset state machine
        socket_connection_init_statemachine(conn);
        
        // "park" if dispatch failed
        if (dispatch_err) {
            log_info("socket_connection_hci_process dispatch failed -> park connection");
            btstack_run_loop_remove_data_source(socket_ds);
            btstack_linked_list_add_tail(&parked, (btstack_linked_item_t *) socket_ds);
        }
    }
}

/**
 * try to dispatch packet for all "parked" connections. 
 * if dispatch is successful, a connection is added again to run loop
 * pre: connections get parked iff packet was dispatched but could not be sent
 */
void socket_connection_retry_parked(void){
    // log_info("socket_connection_hci_process retry parked");
    btstack_linked_item_t *it = (btstack_linked_item_t *) &parked;
    while (it->next) {
        connection_t * conn = (connection_t *) it->next;
        
        // dispatch packet !!! connection, type, channel, data, size
        uint16_t packet_type = little_endian_read_16( conn->buffer, 0);
        uint16_t channel     = little_endian_read_16( conn->buffer, 2);
        uint16_t length      = little_endian_read_16( conn->buffer, 4);
        log_info("socket_connection_hci_process retry parked %p (type %u, channel %04x, length %u", conn, packet_type, channel, length);
        int dispatch_err = (*socket_connection_packet_callback)(conn, packet_type, channel, &conn->buffer[sizeof(packet_header_t)], length);
        // "un-park" if successful
        if (!dispatch_err) {
            log_info("socket_connection_hci_process dispatch succeeded -> un-park connection %p", conn);
            it->next = it->next->next;
            btstack_run_loop_add_data_source( (btstack_data_source_t *) conn);
        } else {
            it = it->next;
        }
    }
}

int  socket_connection_has_parked_connections(void){
    return parked != NULL;
}

static void socket_connection_accept(btstack_data_source_t *socket_ds, btstack_data_source_callback_type_t callback_type) {
    UNUSED(callback_type);
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);

    // get socket_fd
#ifdef _WIN32
    int socket_fd = tcp_socket_fd;
#else
    int socket_fd = btstack_run_loop_get_data_source_fd(socket_ds);
#endif

#ifdef _WIN32
    // sync state
    WSANETWORKEVENTS network_events;
    if (WSAEnumNetworkEvents(socket_fd, socket_ds->source.handle, &network_events) == SOCKET_ERROR){
        log_error("WSAEnumNetworkEvents() failed with error %d\n", WSAGetLastError());
        return;
    }
#endif

	/* New connection coming in! */
	int fd = accept(socket_fd, (struct sockaddr *)&ss, &slen);
	if (fd < 0) {
		perror("accept");
        return;
	}
        
    log_info("socket_connection_accept new connection %u", fd);
    
    connection_t * connection = socket_connection_register_new_connection(fd);
    socket_connection_emit_connection_opened(connection);
}

/** 
 * create socket data_source for tcp socket
 *
 * @return data_source object. If null, check errno
 */
int socket_connection_create_tcp(int port){
    
    // create btstack_data_source_t
    btstack_data_source_t *ds = calloc(sizeof(btstack_data_source_t), 1);
    if (ds == NULL) return -1;
    
	// create tcp socket
    int fd = socket (PF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		log_error("Error creating socket ...(%s)", strerror(errno));
		free(ds);
        return -1;
	}
    
	log_info ("Socket created for port %u", port);

#ifdef _WIN32
    // wrap fd in windows event and configure for accept and close
    WSAEVENT event = WSACreateEvent();
    if (!event){
        log_error("Error creating WSAEvent for socket");
        free(ds);
        return -1;
    }
    int res = WSAEventSelect(fd, event, FD_ACCEPT | FD_CLOSE);
    if (res == SOCKET_ERROR){
        log_error("WSAEventSelect() error: %d\n" , WSAGetLastError());
        free(ds);
        return -1;
    }
    // keep fd around
    tcp_socket_fd = fd;
#endif
	
    struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons (port);
	memset (&addr.sin_addr, 0, sizeof (addr.sin_addr));
	
	const int y = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*) &y, sizeof(int));
	
	if (bind ( fd, (struct sockaddr *) &addr, sizeof (addr) ) ) {
		log_error("Error on bind() ...(%s)", strerror(errno));
		free(ds);
        return -1;
	}
	
	if (listen (fd, MAX_PENDING_CONNECTIONS)) {
		log_error("Error on listen() ...(%s)", strerror(errno));
		free(ds);
        return -1;
	}
    
#ifdef _WIN32
    ds->source.handle = event;
#else
    btstack_run_loop_set_data_source_fd(ds, fd);
#endif
    btstack_run_loop_set_data_source_handler(ds, &socket_connection_accept);
    btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);
    btstack_run_loop_add_data_source(ds);
    
	log_info ("Server up and running ...");
    return 0;
}

#ifdef USE_LAUNCHD

/*
 * Register listening sockets with our run loop
 */
void socket_connection_launchd_register_fd_array(launch_data_t listening_fd_array){
	int i;
    for (i = 0; i < launch_data_array_get_count(listening_fd_array); i++) {
        // get fd
        launch_data_t tempi = launch_data_array_get_index (listening_fd_array, i);
        int listening_fd = launch_data_get_fd(tempi);
        launch_data_free (tempi);
		log_info("file descriptor = %u", listening_fd);
        
        // create btstack_data_source_t for fd
        btstack_data_source_t *ds = calloc(sizeof(btstack_data_source_t), 1);
        if (ds == NULL) return;
        btstack_run_loop_set_data_source_fd(ds, listening_fd);
        btstack_run_loop_set_data_source_handler(ds, &socket_connection_accept);
        btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);
        btstack_run_loop_add_data_source(ds);
	}
}

/** 
 * create socket data_source for socket specified by launchd configuration
 */
int socket_connection_create_launchd(void){
    
    launch_data_t sockets_dict, checkin_response;
	launch_data_t checkin_request;
    launch_data_t listening_fd_array;
    
	/*
	 * Register ourselves with launchd.
	 * 
	 */
	if ((checkin_request = launch_data_new_string(LAUNCH_KEY_CHECKIN)) == NULL) {
		log_error( "launch_data_new_string(\"" LAUNCH_KEY_CHECKIN "\") Unable to create string.");
		return -1;
	}
    
	if ((checkin_response = launch_msg(checkin_request)) == NULL) {
		log_error( "launch_msg(\"" LAUNCH_KEY_CHECKIN "\") IPC failure: %u", errno);
		return -1;
	}
    
	if (LAUNCH_DATA_ERRNO == launch_data_get_type(checkin_response)) {
		errno = launch_data_get_errno(checkin_response);
		log_error( "Check-in failed: %u", errno);
		return -1;
	}
    
    launch_data_t the_label = launch_data_dict_lookup(checkin_response, LAUNCH_JOBKEY_LABEL);
	if (NULL == the_label) {
		log_error( "No label found");
		return -1;
	}
	
	/*
	 * Retrieve the dictionary of Socket entries in the config file
	 */
	sockets_dict = launch_data_dict_lookup(checkin_response, LAUNCH_JOBKEY_SOCKETS);
	if (NULL == sockets_dict) {
		log_error("No sockets found to answer requests on!");
		return -1;
	}
    
	// if (launch_data_dict_get_count(sockets_dict) > 1) {
	// 	log_error("Some sockets will be ignored!");
	// }
    
	/*
	 * Get the dictionary value from the key "Listeners"
	 */
	listening_fd_array = launch_data_dict_lookup(sockets_dict, "Listeners");
	if (listening_fd_array) {
        // log_error("Listeners...");
        socket_connection_launchd_register_fd_array( listening_fd_array );
    }
    
	/*
	 * Get the dictionary value from the key "Listeners"
	 */
	listening_fd_array = launch_data_dict_lookup(sockets_dict, "Listeners2");
	if (listening_fd_array) {
        // log_error("Listeners2...");
        socket_connection_launchd_register_fd_array( listening_fd_array );
    }
    
    // although used in Apple examples, it creates a malloc warning
	// launch_data_free(checkin_response);
    return 0;
}
#endif


/**
 * set packet handler for all auto-accepted connections 
 */
void socket_connection_register_packet_callback( int (*packet_callback)(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t length) ){
    socket_connection_packet_callback = packet_callback;
}

/**
 * send HCI packet to single connection
 */
void socket_connection_send_packet(connection_t *conn, uint16_t type, uint16_t channel, uint8_t *packet, uint16_t size){
    uint8_t header[sizeof(packet_header_t)];
    little_endian_store_16(header, 0, type);
    little_endian_store_16(header, 2, channel);
    little_endian_store_16(header, 4, size);
    // avoid -Wunused-result
    int res;
#ifdef _WIN32
    int flags = 0;
    res = send(conn->socket_fd, (const char *) header, 6, flags);
    res = send(conn->socket_fd, (const char *) packet, size, flags);
#else
    res = write(conn->socket_fd, header, 6);
    res = write(conn->socket_fd, packet, size);
#endif
    UNUSED(res);
}

/**
 * send HCI packet to all connections 
 */
void socket_connection_send_packet_all(uint16_t type, uint16_t channel, uint8_t *packet, uint16_t size){
    btstack_linked_item_t *next;
    btstack_linked_item_t *it;
    for (it = (btstack_linked_item_t *) connections; it ; it = next){
        next = it->next; // cache pointer to next connection_t to allow for removal
        linked_connection_t * linked_connection = (linked_connection_t *) it;
        socket_connection_send_packet( linked_connection->connection, type, channel, packet, size);
    }
}

/**
 * create socket connection to BTdaemon 
 */
connection_t * socket_connection_open_tcp(const char *address, uint16_t port){
    // TCP
    struct protoent* tcp = getprotobyname("tcp");
    
    int btsocket = socket(PF_INET, SOCK_STREAM, tcp->p_proto);
	if(btsocket == -1){
		return NULL;
	}
    // localhost
	struct sockaddr_in btdaemon_address;
	btdaemon_address.sin_family = AF_INET;
	btdaemon_address.sin_port = htons(port);
	struct hostent* localhost = gethostbyname(address);
	if(!localhost){
		return NULL;
	}
    // connect
	char* addr = localhost->h_addr_list[0];
	memcpy(&btdaemon_address.sin_addr.s_addr, addr, sizeof (struct in_addr));
	if(connect(btsocket, (struct sockaddr*)&btdaemon_address, sizeof (btdaemon_address)) == -1){
		return NULL;
	}
    
    return socket_connection_register_new_connection(btsocket);
}


/**
 * close socket connection to BTdaemon 
 */
int socket_connection_close_tcp(connection_t * connection){
    if (!connection) return -1;
#ifdef _WIN32
    shutdown(connection->ds.source.fd, SD_BOTH);
#else    
    shutdown(connection->ds.source.fd, SHUT_RDWR);
#endif
    socket_connection_free_connection(connection);
    return 0;
}

#ifdef HAVE_UNIX_SOCKETS

/** 
 * create socket data_source for unix domain socket
 */
int socket_connection_create_unix(char *path){
        
    // create btstack_data_source_t
    btstack_data_source_t *ds = calloc(sizeof(btstack_data_source_t), 1);
    if (ds == NULL) return -1;

    // create unix socket
    int fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        log_error( "Error creating socket ...(%s)", strerror(errno));
        free(ds);
        return -1;
    }
    log_info ("Socket created at %s", path);
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    unlink(path);
    
    const int y = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*) &y, sizeof(int));
    
    if (bind (fd, (struct sockaddr *) &addr, sizeof (addr) ) ) {
        log_error( "Error on bind() ...(%s)", strerror(errno));
        free(ds);
        return -1;
    }

    // http://blog.henning.makholm.net/2008/06/unix-domain-socket-woes.html
    // make socket accept from all clients
    chmod(path, S_IRWXU | S_IRWXG | S_IRWXO);
    //

    if (listen(fd, MAX_PENDING_CONNECTIONS)) {
        log_error( "Error on listen() ...(%s)", strerror(errno));
        free(ds);
        return -1;
    }
    
    btstack_run_loop_set_data_source_fd(ds, fd);
    btstack_run_loop_set_data_source_handler(ds, &socket_connection_accept);
    btstack_run_loop_enable_data_source_callbacks(ds, DATA_SOURCE_CALLBACK_READ);
    btstack_run_loop_add_data_source(ds);

    log_info ("Server up and running ...");
    return 0;
}

/**
 * create socket connection to BTdaemon 
 */
connection_t * socket_connection_open_unix(void){
    
    int btsocket = socket(AF_UNIX, SOCK_STREAM, 0);
	if(btsocket == -1){
		return NULL;
	}

    struct sockaddr_un server;
    memset(&server, 0, sizeof(server));
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, BTSTACK_UNIX);
    if (connect(btsocket, (struct sockaddr *)&server, sizeof (server)) == -1){
        return NULL;
    };
    
    return socket_connection_register_new_connection(btsocket);
}


/**
 * close socket connection to BTdaemon 
 */
int socket_connection_close_unix(connection_t * connection){
    if (!connection) return -1;
#ifdef _WIN32
    shutdown(connection->ds.source.fd, SD_BOTH);
#else    
    shutdown(connection->ds.source.fd, SHUT_RDWR);
#endif 
    socket_connection_free_connection(connection);
    return 0;
}

#endif /* HAVE_UNIX_SOCKETS */

/**
 * Init socket connection module
 */
void socket_connection_init(void){

    // just ignore broken sockets - NO_SO_SIGPIPE
#ifndef _WIN32
    sig_t result = signal(SIGPIPE, SIG_IGN);
    if (result){
        log_error("socket_connection_init: failed to ignore SIGPIPE, error: %s", strerror(errno));
    }
#endif

#ifdef _WIN32
    // TODO: call WSACleanup with wsa data on shutdown
    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa);
    log_info("WSAStartup(v2.2) = %x", res);
    if (res){
        log_error("WSAStartup error: %d", WSAGetLastError());
        return;
    }
#endif
}


