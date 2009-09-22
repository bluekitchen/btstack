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

#include "../config.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifdef USE_LAUNCHD
#include "../3rdparty/launch.h"
#endif

#define MAX_PENDING_CONNECTIONS 10

/** prototypes */
static int socket_connection_hci_process(struct data_source *ds);
static int socket_connection_dummy_handler(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t length);

/** globals */

/** packet header used over socket connections, in front of the HCI packet */
typedef struct packet_header {
    uint16_t type;
    uint16_t channel;
    uint16_t length;
    uint8_t  data[0];
} packet_header_t;

typedef enum {
    SOCKET_W4_HEADER,
    SOCKET_W4_DATA,
} SOCKET_STATE;

struct connection {
    data_source_t ds;       // used for run loop
    linked_item_t item;     // used for connection list
    SOCKET_STATE state;
    uint16_t bytes_read;
    uint16_t bytes_to_read;
    uint8_t  buffer[3+3+255]; // max HCI CMD + packet_header
};

/** list of socket connections */
static linked_list_t connections = NULL;


/** client packet handler */

static int (*socket_connection_packet_callback)(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t length) = socket_connection_dummy_handler;

static int socket_connection_dummy_handler(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t length){
    return 0;
}


int socket_connection_set_non_blocking(int fd)
{
    int err;
    int flags;
    // According to the man page, F_GETFL can't error!
    flags = fcntl(fd, F_GETFL, NULL);
    err = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return err;
}

void socket_connection_free_connection(connection_t *conn){
    // remove from run_loop 
    run_loop_remove_data_source(&conn->ds);
    
    // and from connection list
    linked_list_remove(&connections, &conn->item);
    
    // destroy
    free(conn);
}

void socket_connection_init_statemachine(connection_t *connection){
    // wait for next packet
    connection->state = SOCKET_W4_HEADER;
    connection->bytes_read = 0;
    connection->bytes_to_read = sizeof(packet_header_t);
}

connection_t * socket_connection_register_new_connection(int fd){
    // create connection objec 
    connection_t * conn = malloc( sizeof(connection_t));
    if (conn == NULL) return 0;
    linked_item_set_user( &conn->ds.item, conn);
    linked_item_set_user( &conn->item, conn);
    conn->ds.fd = fd;
    conn->ds.process = socket_connection_hci_process;
    
    // prepare state machine and
    socket_connection_init_statemachine(conn);
    
    // add this socket to the run_loop
    run_loop_add_data_source( &conn->ds );
    
    // and the connection list
    linked_list_add( &connections, &conn->item);
    
    return conn;
}

void static socket_connection_emit_nr_connections(){
    linked_item_t *it;
    uint8_t nr_connections = 0;
    for (it = (linked_item_t *) connections; it != NULL ; it = it->next, nr_connections++);
    
    uint8_t event[2];
    event[0] = DAEMON_NR_CONNECTIONS_CHANGED;
    event[1] = nr_connections;
    (*socket_connection_packet_callback)(NULL, DAEMON_EVENT_PACKET, 0, (uint8_t *) &event, 2);
    // printf("Nr connections changed,.. new %u\n", nr_connections); 
}

int socket_connection_hci_process(struct data_source *ds) {
    connection_t *conn = (connection_t *) ds;
    int bytes_read = read(ds->fd, &conn->buffer[conn->bytes_read], conn->bytes_to_read);
    if (bytes_read <= 0){
        // connection broken (no particular channel, no date yet)
        uint8_t event[1];
        event[0] = DAEMON_CONNECTION_CLOSED;
        (*socket_connection_packet_callback)(conn, DAEMON_EVENT_PACKET, 0, (uint8_t *) &event, 1);
        
        // free connection
        socket_connection_free_connection(linked_item_get_user(&ds->item));
        
        socket_connection_emit_nr_connections();
        return 0;
    }
    conn->bytes_read += bytes_read;
    conn->bytes_to_read -= bytes_read;
    // hexdump( conn->buffer, conn->bytes_read);
    if (conn->bytes_to_read > 0) {
        return 0;
    }
    switch (conn->state){
        case SOCKET_W4_HEADER:
            conn->state = SOCKET_W4_DATA;
            conn->bytes_to_read = READ_BT_16( conn->buffer, 4);
            break;
        case SOCKET_W4_DATA:
            // dispatch packet !!! connection, type, channel, data, size
            (*socket_connection_packet_callback)(conn, READ_BT_16( conn->buffer, 0), READ_BT_16( conn->buffer, 2),
                                    &conn->buffer[sizeof(packet_header_t)], READ_BT_16( conn->buffer, 4));
            // reset state machine
            socket_connection_init_statemachine(conn);
            break;
    }
	return 0;
}

static int socket_connection_accept(struct data_source *socket_ds) {
    
	/* New connection coming in! */
	int fd = accept(socket_ds->fd, NULL, NULL);
	if (fd < 0) {
		perror("accept");
        return 0;
	}
    // non-blocking ?
	// socket_connection_set_non_blocking(ds->fd);
        
    printf("socket_connection_accept new connection %u\n", fd);
    
    socket_connection_register_new_connection(fd);
    socket_connection_emit_nr_connections();
    
    return 0;
}

/** 
 * create socket data_source for tcp socket
 *
 * @return data_source object. If null, check errno
 */
int socket_connection_create_tcp(int port){
    
    // create data_source_t
    data_source_t *ds = malloc( sizeof(data_source_t));
    if (ds == NULL) return -1;
    ds->fd = 0;
    ds->process = socket_connection_accept;
    
	// create tcp socket
	if ((ds->fd = socket (PF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf (stderr, "Error creating socket ...(%s)\n", strerror(errno));
		free(ds);
        return -1;
	}
    
	printf ("Socket created for port %u\n", port);
	
    struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons (port);
	memset (&addr.sin_addr, 0, sizeof (addr.sin_addr));
	
	const int y = 1;
	setsockopt(ds->fd, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
	
	if (bind ( ds->fd, (struct sockaddr *) &addr, sizeof (addr) ) ) {
		fprintf(stderr, "Error on bind() ...(%s)\n", strerror(errno));
		free(ds);
        return -1;
	}
	
	if (listen (ds->fd, MAX_PENDING_CONNECTIONS)) {
		fprintf (stderr, "Error on listen() ...(%s)\n", strerror(errno));
		free(ds);
        return -1;
	}
    
    run_loop_add_data_source(ds);
    
	printf ("Server up and running ...\n");
    return 0;
}

/** 
 * create socket data_source for unix domain socket
 * */
int socket_connection_create_unix(char *path){
    
#ifdef USE_LAUNCHD
    
    launch_data_t sockets_dict, checkin_response;
	launch_data_t checkin_request;
    launch_data_t listening_fd_array;
	size_t i;
    
	/*
	 * Register ourselves with launchd.
	 * 
	 */
	if ((checkin_request = launch_data_new_string(LAUNCH_KEY_CHECKIN)) == NULL) {
		fprintf(stderr, "launch_data_new_string(\"" LAUNCH_KEY_CHECKIN "\") Unable to create string.");
		return -1;
	}
    
	if ((checkin_response = launch_msg(checkin_request)) == NULL) {
		fprintf(stderr, "launch_msg(\"" LAUNCH_KEY_CHECKIN "\") IPC failure: %u", errno);
		return -1;
	}
    
	if (LAUNCH_DATA_ERRNO == launch_data_get_type(checkin_response)) {
		errno = launch_data_get_errno(checkin_response);
		fprintf(stderr, "Check-in failed: %u", errno);
		return -1;
	}
    
    launch_data_t the_label = launch_data_dict_lookup(checkin_response, LAUNCH_JOBKEY_LABEL);
	if (NULL == the_label) {
		fprintf(stderr, "No label found");
		return -1;
	}
	
	/*
	 * Retrieve the dictionary of Socket entries in the config file
	 */
	sockets_dict = launch_data_dict_lookup(checkin_response, LAUNCH_JOBKEY_SOCKETS);
	if (NULL == sockets_dict) {
		fprintf(stderr,"No sockets found to answer requests on!");
		return -1;
	}
    
	if (launch_data_dict_get_count(sockets_dict) > 1) {
		fprintf(stderr,"Some sockets will be ignored!");
	}
    
	/*
	 * Get the dictionary value from the key "Listeners"
	 */
	listening_fd_array = launch_data_dict_lookup(sockets_dict, "Listeners");
	if (NULL == listening_fd_array) {
		fprintf(stderr,"No known sockets found to answer requests on!");
		return -1;
	}
    
	/*
	 * Register listening sockets with our run loop
	 */
	for (i = 0; i < launch_data_array_get_count(listening_fd_array); i++) {
        // get fd
        launch_data_t tempi = launch_data_array_get_index (listening_fd_array, i);
        int listening_fd = launch_data_get_fd(tempi);
        launch_data_free (tempi);
		printf("%u. file descriptor = %u",(unsigned int) i+1, listening_fd);
        
        // create data_source_t for fd
        data_source_t *ds = malloc( sizeof(data_source_t));
        if (ds == NULL) return -1;
        ds->process = socket_connection_accept;
        ds->fd = listening_fd;
        run_loop_add_data_source(ds);
	}
    
	launch_data_free(checkin_response);

#else
    
    // create data_source_t
    data_source_t *ds = malloc( sizeof(data_source_t));
    if (ds == NULL) return -1;
    ds->fd = 0;
    ds->process = socket_connection_accept;

	// create unix socket
	if ((ds->fd = socket (AF_UNIX, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Error creating socket ...(%s)\n", strerror(errno));
		free(ds);
        return -1;
	}
    
	printf ("Socket created\n");
	
    struct sockaddr_un addr;
    bzero(&addr, sizeof(addr));
	addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    unlink(path);
    
	const int y = 1;
	setsockopt(ds->fd, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(int));
    
	if (bind ( ds->fd, (struct sockaddr *) &addr, sizeof (addr) ) ) {
		fprintf(stderr, "Error on bind() ...(%s)\n", strerror(errno));
		free(ds);
        return -1;
	}
	
	if (listen (ds->fd, MAX_PENDING_CONNECTIONS)) {
		fprintf(stderr, "Error on listen() ...(%s)\n", strerror(errno));
		free(ds);
        return -1;
	}
    
    run_loop_add_data_source(ds);

#endif
    
	printf ("Server up and running ...\n");
    return 0;
}

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
    bt_store_16(header, 0, type);
    bt_store_16(header, 2, channel);
    bt_store_16(header, 4, size);
    write(conn->ds.fd, header, 6);
    write(conn->ds.fd, packet, size);
}

/**
 * send HCI packet to all connections 
 */
void socket_connection_send_packet_all(uint16_t type, uint16_t channel, uint8_t *packet, uint16_t size){
    linked_item_t *next;
    linked_item_t *it;
    for (it = (linked_item_t *) connections; it != NULL ; it = next){
        next = it->next; // cache pointer to next connection_t to allow for removal
        socket_connection_send_packet( (connection_t *) linked_item_get_user(it), type, channel, packet, size);
    }
}

/**
 * create socket connection to BTdaemon 
 */
connection_t * socket_connection_open_tcp(){
    // TCP
    struct protoent* tcp = getprotobyname("tcp");
    
    int btsocket = socket(PF_INET, SOCK_STREAM, tcp->p_proto);
	if(btsocket == -1){
		return NULL;
	}
    // localhost
	struct sockaddr_in btdaemon_address;
	btdaemon_address.sin_family = AF_INET;
	btdaemon_address.sin_port = htons(BTSTACK_PORT);
	struct hostent* localhost = gethostbyname("localhost");
	if(!localhost){
		return NULL;
	}
    // connect
	char* addr = localhost->h_addr_list[0];
	memcpy(&btdaemon_address.sin_addr.s_addr, addr, sizeof addr);
	if(connect(btsocket, (struct sockaddr*)&btdaemon_address, sizeof btdaemon_address) == -1){
		return NULL;
	}
    
    return socket_connection_register_new_connection(btsocket);
}


/**
 * close socket connection to BTdaemon 
 */
int socket_connection_close_tcp(connection_t * connection){
    if (!connection) return -1;
    shutdown(connection->ds.fd, SHUT_RDWR);
    run_loop_remove_data_source(&connection->ds);
    free( connection );
    return 0;
}


/**
 * create socket connection to BTdaemon 
 */
connection_t * socket_connection_open_unix(){
    
    int btsocket = socket(AF_UNIX, SOCK_STREAM, 0);
	if(btsocket == -1){
		return NULL;
	}

    struct sockaddr_un server;
    bzero(&server, sizeof(server));
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
    shutdown(connection->ds.fd, SHUT_RDWR);
    run_loop_remove_data_source(&connection->ds);
    free( connection );
    return 0;
}

