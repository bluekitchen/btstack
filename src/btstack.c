/*
 *  btstack.c
 *
 *  Created by Matthias Ringwald on 7/1/09.
 *
 *  BTstack client API
 */

#include "btstack.h"

#include "run_loop.h"
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <stdio.h>

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

uint8_t hci_cmd_buffer[3+255];



static void dummy_handler(uint8_t *packet, int size);
static int btstack_socket_process(struct data_source *ds, int ready);

static int btstack_socket = -1;
static connection_t *btstack_connection = NULL;
static const int btstack_port = 1919;

/* callback to L2CAP layer */
static void (*event_packet_handler)(uint8_t *packet, int size) = dummy_handler;
static void (*acl_packet_handler)  (uint8_t *packet, int size) = dummy_handler;


// init BTstack library
int bt_open(){
    // TCP
    struct protoent* tcp = getprotobyname("tcp");
    
    btstack_socket = socket(PF_INET, SOCK_STREAM, tcp->p_proto);
	if(btstack_socket == -1){
		return -1;
	}
    // localhost
	struct sockaddr_in btdaemon_address;
	btdaemon_address.sin_family = AF_INET;
	btdaemon_address.sin_port = htons(btstack_port);
	struct hostent* localhost = gethostbyname("localhost");
	if(!localhost){
		return -1;
	}
    // connect
	char* addr = localhost->h_addr_list[0];
	memcpy(&btdaemon_address.sin_addr.s_addr, addr, sizeof addr);
	if(connect(btstack_socket, (struct sockaddr*)&btdaemon_address, sizeof btdaemon_address) == -1){
		return -1;
	}
    
    // register with run loop
    btstack_connection = malloc( sizeof(connection_t));
    if (btstack_connection == NULL) return -1;
    btstack_connection->ds.fd = btstack_socket;
    btstack_connection->ds.process = btstack_socket_process;
    run_loop_add(&btstack_connection->ds);

    // init state machine
    btstack_connection->state = SOCKET_W4_HEADER;
    btstack_connection->bytes_read = 0;
    btstack_connection->bytes_to_read = 2;
    
    return 0;
}


// stop using BTstack library
int bt_close(){
    if (btstack_socket < 0) return 0;
    shutdown(btstack_socket, SHUT_RDWR);
    if (btstack_connection) {
        run_loop_remove(&btstack_connection->ds);
        free( btstack_connection );
        btstack_connection = NULL;
    }
    return 0;
}

// run_loop based data handling
int btstack_socket_process(struct data_source *ds, int ready) {
    connection_t *conn = (connection_t *) ds;
    int bytes_read = read(ds->fd, &conn->buffer[conn->bytes_read],
                          sizeof(packet_header_t) - conn->bytes_read);
    if (bytes_read <= 0){
        // free
        run_loop_remove(&conn->ds);
        free(conn);
        // TODO notify client of cailed daemon connection!
        return 0;
    }
    conn->bytes_read += bytes_read;
    conn->bytes_to_read -= bytes_read;
    if (conn->bytes_to_read > 0) {
        return 0;
    }
    switch (conn->state){
        case SOCKET_W4_HEADER:
            conn->state = SOCKET_W4_DATA;
            conn->bytes_to_read = READ_BT_16( conn->buffer, 0) + sizeof(packet_header_t);
            break;
        case SOCKET_W4_DATA:
            // process packet !!!
            switch (conn->buffer[2]){
                case HCI_EVENT_PACKET:
                    (*event_packet_handler)(&conn->buffer[3], READ_BT_16( conn->buffer, 0));
                    break;
                case HCI_ACL_DATA_PACKET:
                    (*acl_packet_handler)(&conn->buffer[3], READ_BT_16( conn->buffer, 0));
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


// send hci cmd packet
int bt_send_cmd(hci_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint16_t len = hci_create_cmd_internal(hci_cmd_buffer, cmd, argptr);
    va_end(argptr);
    packet_header_t header;
    bt_store_16( (uint8_t*) &header.length, 0, len);
    header.type = HCI_COMMAND_DATA_PACKET;
    hexdump( (uint8_t*)&header, sizeof(header)); // TODO use packet instead of tweaking sizeof(header));
    write(btstack_socket, (uint8_t*)&header, sizeof(header));
    hexdump( hci_cmd_buffer, len);
    write(btstack_socket, hci_cmd_buffer, len);
    return len;
}

// send hci acl packet
int bt_send_acl_packet(uint8_t *packet, int size){
    return 0;
}

static void dummy_handler(uint8_t *packet, int size){
}

// register packet and event handler
void bt_register_event_packet_handler(void (*handler)(uint8_t *packet, int size)){
    event_packet_handler = handler;
}

void bt_register_acl_packet_handler  (void (*handler)(uint8_t *packet, int size)){
    acl_packet_handler = handler;
}

