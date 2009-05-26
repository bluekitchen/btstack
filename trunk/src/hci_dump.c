/*
 *  hci_dump.c
 *
 *  Dump HCI trace in BlueZ's hcidump format
 * 
 *  Created by Matthias Ringwald on 5/26/09.
 */

#include "hci_dump.h"
#include "hci.h"
#include "hci_transport_h4.h"

#include <fcntl.h>        // open
#include <arpa/inet.h>    // hton..
#include <strings.h>      // bzero
#include <unistd.h>       // write 
#include <stdio.h>

static int dump_file;

void hci_dump_open(char *filename){
    dump_file =  open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
}

void hci_dump_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len) {
    static hcidump_hdr header;
    bzero( &header, sizeof(hcidump_hdr));
    bt_store_16( (uint8_t *) &header, 0, 1 + len);
    header.in  = in;
    header.packet_type = packet_type;
    write (dump_file, &header, sizeof(hcidump_hdr) );
    write (dump_file, packet, len );
}

void hci_dump_close(){
    close(dump_file);
}

