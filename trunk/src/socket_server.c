/*
 *  SocketServer.c
 *  
 *  Handles multiple connections to a single socket without blocking
 *
 *  Created by Matthias Ringwald on 6/6/09.
 *
 */

#include "socket_server.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>

#include "hci.h"

#define MAX_PENDING_CONNECTIONS 10
#define DATA_BUF_SIZE           80


typedef struct packet_header {
    uint16_t length;
    uint8_t  type;
    uint8_t  data[0];
} packet_header_t;

typedef enum {
    SOCKET_W4_HEADER,
    SOCKET_W4_DATA,
} SOCKET_STATE;

typedef struct connection {
    data_source_t ds;
    struct connection * next;
    SOCKET_STATE state;
    uint16_t bytes_read;
    uint16_t bytes_to_read;
    char   buffer[3+3+255]; // max HCI CMD + packet_header
} connection_t;

connection_t *connections = NULL;

static char test_buffer[DATA_BUF_SIZE];

static int socket_server_echo_process(struct data_source *ds, int ready);
static int (*socket_server_process)(struct data_source *ds, int ready) = socket_server_echo_process;

int socket_server_set_non_blocking(int fd)
{
    int err;
    int flags;
    // According to the man page, F_GETFL can't error!
    flags = fcntl(fd, F_GETFL, NULL);
    err = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return err;
}

static int socket_server_echo_process(struct data_source *ds, int ready) {
    int bytes_read = read(ds->fd, test_buffer, DATA_BUF_SIZE);
    
    // connection closed by client?
    if (bytes_read < 0){
        
        // emit signal
        
        // remove from run_loop 
        // @note: run_loop_execute must be prepared for this
        run_loop_remove(ds);
        
        // destroy
        free(ds);
        
        return 0;
    }
    
    write(ds->fd, "echo: ", 5);
    write(ds->fd, test_buffer, bytes_read);
    
    return 0;
}

#if 0
static int socket_server_connection_process(struct data_source *ds, int ready) {
    connection_t *conn = (connection_t *) ds;
    int bytes_read = read(ds->fd, &conn->buffer[conn->bytes_read],
                      sizeof(packet_header_t) - conn->bytes_read);
    if (bytes_read < 0){
        //
    }
    conn->bytes_read += bytes_read;
    conn->bytes_to_read -= bytes_read;
    if (conn->bytes_to_read > 0) {
        return 0;
    }
    switch (conn->state){
        case SOCKET_W4_HEADER:
            conn->state = SOCKET_W4_DATA;
            conn->bytes_to_read = READ_BT_16( conn->buffer, 0);
            break;
        case SOCKET_W4_DATA:
            // process packet !!!
            
            
            // wait for next packet
            conn->state = SOCKET_W4_HEADER;
            conn->bytes_read = 0;
            conn->bytes_to_read = sizeof(packet_header_t);
            break;
    }
	return 0;
}
#endif

static int socket_server_accept(struct data_source *socket_ds, int ready) {
    
    // create data_source_t
    connection_t * conn = malloc( sizeof(connection_t));
    if (conn == NULL) return 0;
    conn->ds.fd = 0;
    conn->ds.process = socket_server_process;
    
    // init state machine
    conn->state = SOCKET_W4_HEADER;
    conn->bytes_read = 0;
    conn->bytes_to_read = 2;
    
	/* New connection coming in! */
	conn->ds.fd = accept(socket_ds->fd, NULL, NULL);
	if (conn->ds.fd < 0) {
		perror("accept");
		free(conn);
        return 0;
	}
    // non-blocking ?
	// socket_server_set_non_blocking(ds->fd);
    
    // store reference to this connection
    conn->next = NULL;
    connections = conn;
    
    // add this socket to the run_loop
    run_loop_add( &conn->ds );
    
    return 0;
}

/** 
 * create socket data_source for tcp socket
 *
 * @return data_source object. If null, check errno
 */
data_source_t * socket_server_create_tcp(int port){
    
    // create data_source_t
    data_source_t *ds = malloc( sizeof(data_source_t));
    if (ds == NULL) return ds;
    ds->fd = 0;
    ds->process = socket_server_accept;
    
	// create tcp socket
    struct sockaddr_in addr;
	if ((ds->fd = socket (PF_INET, SOCK_STREAM, 0)) < 0) {
		printf ("Error creating socket ...(%s)\n", strerror(errno));
		free(ds);
        return NULL;
	}
    
	printf ("Socket created\n");
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons (port);
	memset (&addr.sin_addr, 0, sizeof (addr.sin_addr));
	
	const int y = 1;
	setsockopt(ds->fd, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
	
	if (bind ( ds->fd, (struct sockaddr *) &addr, sizeof (addr) ) ) {
		printf ("Error on bind() ...(%s)\n", strerror(errno));
		free(ds);
        return NULL;
	}
	
	if (listen (ds->fd, MAX_PENDING_CONNECTIONS)) {
		printf ("Error on listen() ...(%s)\n", strerror(errno));
		free(ds);
        return NULL;
	}
    
	printf ("Server up and running ...\n");
    return ds;
}

/** 
 * create socket data_source for unix domain socket
 *
 * @TODO: implement socket_server_create_unix
 */
data_source_t * socket_server_create_unix(char *path){
    return 0;
}

/**
 * register data available callback
 * @todo: hack callback to allow data reception - replace with better architecture
 */
void socket_server_register_process_callback( int (*process_callback)(struct data_source *ds, int ready) ){
    socket_server_process = process_callback;
}