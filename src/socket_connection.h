/*
 *  socket_connection.h
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#pragma once

#include "run_loop.h"
#include <stdint.h>

/** opaque connection type */
typedef struct connection connection_t;

/** 
 * create socket data_source for tcp socket
 */
int socket_connection_create_tcp(int port);

/** 
 * create socket data_source for unix domain socket
 */
int socket_connection_create_unix(char *path);

/**
 * set packet handler for all auto-accepted connections 
 */
void socket_connection_register_packet_callback( int (*packet_callback)(connection_t *connection, uint8_t packet_type, uint8_t *data, uint16_t length) );

/**
 * send HCI packet to single connection
 */
void socket_connection_send_packet(connection_t *connection, uint8_t packet_type, uint8_t *data, uint16_t size);

/**
 * send event/acl data to all clients
 */
void socket_connection_send_event_all(uint8_t *packet, uint16_t size);
void socket_connection_send_acl_all(uint8_t *packet, uint16_t size);
