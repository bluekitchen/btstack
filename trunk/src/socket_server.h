/*
 *  socket_server.h
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#pragma once

#include "run_loop.h"
#include <stdint.h>

/** 
 * create socket data_source for tcp socket
 */
data_source_t * socket_server_create_tcp(int port);

/** 
 * create socket data_source for unix domain socket
 */
data_source_t * socket_server_create_unix(char *path);

/**
 * register data available callback
 * @todo: hack callback to allow data reception - replace with better architecture
 */
void socket_server_register_process_callback( int (*process_callback)(struct data_source *ds, int ready) );
int socket_server_connection_process(struct data_source *ds, int ready);

void socket_server_send_event_all(uint8_t *packet, uint16_t size);
void socket_server_send_acl_all(uint8_t *packet, uint16_t size);

// send ACL packet
int hci_send_acl_packet(uint8_t *packet, int size);
