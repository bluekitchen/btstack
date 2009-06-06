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
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>

#define MAX_PENDING_CONNECTIONS 10
#define DATA_BUF_SIZE           80

static char test_buffer[DATA_BUF_SIZE];


int socket_server_set_non_blocking(int fd)
{
    int err;
    int flags;
    // According to the man page, F_GETFL can't error!
    flags = fcntl(fd, F_GETFL, NULL);
    err = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return err;
}

static int socket_server_process(struct data_source *ds, int ready) {
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


static int socket_server_accept(struct data_source *socket_ds, int ready) {
    
    // create data_source_t
    data_source_t *ds = malloc( sizeof(data_source_t));
    if (ds == NULL) return 0;
    ds->fd = 0;
    ds->process = socket_server_process;
    
	/* We have a new connection coming in!  We'll
     try to find a spot for it in connectlist. */
	ds->fd = accept(socket_ds->fd, NULL, NULL);
	if (ds->fd < 0) {
		perror("accept");
		free(ds);
        return 0;
	}
    // non-blocking ?
	// socket_server_set_non_blocking(ds->fd);
    
    // add this socket to the run_loop
    run_loop_add( ds );
    
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

