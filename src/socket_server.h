/*
 *  socket_server.h
 *
 *  Created by Matthias Ringwald on 6/6/09.
 */

#pragma once

#include "run_loop.h"

/** 
 * create socket data_source for tcp socket
 */
data_source_t * socket_server_create_tcp(int port);

/** 
 * create socket data_source for unix domain socket
 */
data_source_t * socket_server_create_unix(char *path);