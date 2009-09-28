/*
 *  socket_connection.h
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#pragma once

#include <btstack/run_loop.h>

#include <stdint.h>

/** TCP port for BTstack */
#define BTSTACK_PORT            13333

/** UNIX domain socket for BTstack */
#define BTSTACK_UNIX            "/tmp/BTstack"

/** opaque connection type */
typedef struct connection connection_t;

/** 
 * create socket for incoming tcp connections
 */
int socket_connection_create_tcp(int port);

/** 
 * create socket for incoming unix domain connections
 */
int socket_connection_create_unix(char *path);

/**
 * close socket connection to BTdaemon 
 */
int socket_connection_close_tcp(connection_t *connection);

/**
 * create TCP socket connection to BTdaemon 
 */
connection_t * socket_connection_open_tcp();

/**
 * close TCP socket connection to BTdaemon 
 */
int socket_connection_close_tcp(connection_t *connection);

/**
 * create unix socket connection to BTdaemon 
 */
connection_t * socket_connection_open_unix();

/**
 * close unix connection to BTdaemon 
 */
int socket_connection_close_unix(connection_t *connection);


/**
 * set packet handler for all auto-accepted connections 
 */
void socket_connection_register_packet_callback( int (*packet_callback)(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t length) );

/**
 * send HCI packet to single connection
 */
void socket_connection_send_packet(connection_t *connection, uint16_t packet_type, uint16_t channel, uint8_t *data, uint16_t size);

/**
 * send event data to all clients
 */
void socket_connection_send_packet_all(uint16_t type, uint16_t channel, uint8_t *packet, uint16_t size);
