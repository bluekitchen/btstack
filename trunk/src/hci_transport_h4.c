/*
 *  hci_h4_transport.c
 *  project
 *
 *  Created by Matthias Ringwald on 4/29/09.
 *  Copyright 2009 Dybuster AG. All rights reserved.
 *
 */

#include "hci_h4_transport.h"

// prototypes
static int    open(void *transport_config){
    return 0;
}

static int    close(){
    return 0;
}

static int    send_cmd_packet(void *packet, int size){
    return 0;
}

static int    send_acl_packet(void *packet, int size){
    return 0;
}

static void   register_event_packet_handler(void (*handler)(void *packet, int size)){
}
static void   register_acl_packet_handler  (void (*handler)(void *packet, int size)){
}

static int    get_fd() {
    return -1;
}

static int    handle_data() {
    return 0;
}

static const char * get_transport_name(){
    return "H4";
}

// private data

typedef struct {
} hci_h4_transport_private_t;

// single instance
hci_transport_t hci_h4_transport = {
    open, 
    close,
    send_cmd_packet,
    send_acl_packet,
    register_acl_packet_handler,
    register_event_packet_handler,
    get_fd,
    handle_data,
    get_transport_name
};
