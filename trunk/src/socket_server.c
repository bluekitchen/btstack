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
    data_source_t ds;       // used for run loop
    linked_item_t item;     // used for connection list
    SOCKET_STATE state;
    uint16_t bytes_read;
    uint16_t bytes_to_read;
    uint8_t  buffer[3+3+255]; // max HCI CMD + packet_header
} connection_t;

linked_list_t connections = NULL;

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

void socket_server_free_connection(connection_t *conn){
    // remove from run_loop 
    run_loop_remove(&conn->ds);
    
    // and from connection list
    linked_list_remove(&connections, &conn->item);
    
    // destroy
    free(conn);
}

static int socket_server_echo_process(struct data_source *ds, int ready) {
    int bytes_read = read(ds->fd, test_buffer, DATA_BUF_SIZE);
    
    // connection closed by client?
    if (bytes_read <= 0){
        // free
        socket_server_free_connection(  linked_item_get_user(&ds->item));
        return 0;
    }
    
    write(ds->fd, "echo: ", 5);
    write(ds->fd, test_buffer, bytes_read);
    
    return 0;
}

int socket_server_connection_process(struct data_source *ds, int ready) {
    connection_t *conn = (connection_t *) ds;
    int bytes_read = read(ds->fd, &conn->buffer[conn->bytes_read], conn->bytes_to_read);
    
    printf("socket_server_connection_process: state %u, new %u, read %u, toRead %u\n", conn->state,
           bytes_read, conn->bytes_read, conn->bytes_to_read);
    if (bytes_read <= 0){
        // free
        socket_server_free_connection(  linked_item_get_user(&ds->item));
        return 0;
    }
    conn->bytes_read += bytes_read;
    conn->bytes_to_read -= bytes_read;
    hexdump( conn->buffer, conn->bytes_read);
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
            switch (conn->buffer[2]){
                case HCI_COMMAND_DATA_PACKET:
                    hci_send_cmd_packet(&conn->buffer[sizeof(packet_header_t)], READ_BT_16( conn->buffer, 0));
                    break;
                case HCI_ACL_DATA_PACKET:
                    hci_send_acl_packet(&conn->buffer[sizeof(packet_header_t)], READ_BT_16( conn->buffer, 0));
                    break;
                default:
                    break;
            }
            
            // wait for next packet
            conn->state = SOCKET_W4_HEADER;
            conn->bytes_read = 0;
            conn->bytes_to_read = sizeof(packet_header_t);
            break;
    }
	return 0;
}

int socket_server_send_packet_all(uint8_t type, uint8_t *packet, uint16_t size){
    uint8_t length[2];
    bt_store_16( (uint8_t *) &length, 0, size);
    connection_t *next;
    connection_t *it;
    for (it = (connection_t *) connections; it != NULL ; it = next){
        next = (connection_t *) it->item.next; // cache pointer to next connection_t to allow for removal
        write(it->ds.fd, &length, 2);
        write(it->ds.fd, &type, 1);
        write(it->ds.fd, &type, 1); // padding for now
        write(it->ds.fd, packet, size);
    }
    return 0;
}

void socket_server_send_event_all(uint8_t *packet, uint16_t size){
    socket_server_send_packet_all( HCI_EVENT_PACKET, packet, size);
    return;
}

void socket_server_send_acl_all(uint8_t *packet, uint16_t size){
    socket_server_send_packet_all( HCI_ACL_DATA_PACKET, packet, size);
    return;
}


static int socket_server_accept(struct data_source *socket_ds, int ready) {
    
    // create data_source_t
    connection_t * conn = malloc( sizeof(connection_t));
    if (conn == NULL) return 0;
    conn->ds.fd = 0;
    conn->ds.process = socket_server_process;
    
    // init state machine
    conn->state = SOCKET_W4_HEADER;
    conn->bytes_read = 0;
    conn->bytes_to_read = sizeof(packet_header_t);
    
	/* New connection coming in! */
	conn->ds.fd = accept(socket_ds->fd, NULL, NULL);
	if (conn->ds.fd < 0) {
		perror("accept");
		free(conn);
        return 0;
	}
    // non-blocking ?
	// socket_server_set_non_blocking(ds->fd);
        
    printf("socket_server_accept new connection %u\n", conn->ds.fd);
    
    // add this socket to the run_loop
    linked_item_set_user( &conn->ds.item, conn);
    run_loop_add( &conn->ds );
    
    // and the connection list
    linked_item_set_user( &conn->item, conn);
    linked_list_add( &connections, &conn->item);
    
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
    if (ds == NULL) return NULL;
    ds->fd = 0;
    ds->process = socket_server_accept;
    
	// create tcp socket
	if ((ds->fd = socket (PF_INET, SOCK_STREAM, 0)) < 0) {
		printf ("Error creating socket ...(%s)\n", strerror(errno));
		free(ds);
        return NULL;
	}
    
	printf ("Socket created\n");
	
    struct sockaddr_in addr;
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
    
    run_loop_add(ds);
    
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