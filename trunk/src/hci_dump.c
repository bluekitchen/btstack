/*
 *  hci_dump.c
 *
 *  Dump HCI trace in various formats:
 *
 *  - BlueZ's hcidump format
 *  - Apple's PacketLogger
 *  - stdout hexdump
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
#include <time.h>
#include <sys/time.h>     // for timestamps
#include <sys/stat.h>     // for mode flags

// BLUEZ hcidump
typedef struct {
	uint16_t	len;
	uint8_t		in;
	uint8_t		pad;
	uint32_t	ts_sec;
	uint32_t	ts_usec;
    uint8_t     packet_type;
} __attribute__ ((packed)) hcidump_hdr;

// APPLE PacketLogger
typedef struct {
	uint32_t	len;
	uint32_t	ts_sec;
	uint32_t	ts_usec;
	uint8_t		type;
} __attribute__ ((packed)) pktlog_hdr;

static int dump_file = -1;
static int dump_format;
static hcidump_hdr header_bluez;
static pktlog_hdr  header_packetlogger;
static char time_string[40];

void hci_dump_open(char *filename, hci_dump_format_t format){
    dump_format = format;
    if (dump_format == HCI_DUMP_STDOUT) {
        dump_file = fileno(stdout);
    } else {
        dump_file =  open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    }
}

void hci_dump_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len) {
    if (dump_file < 0) return; // not activated yet
    
    // get time
    struct timeval curr_time;
    struct tm* ptm;
    gettimeofday(&curr_time, NULL);
    
    switch (dump_format){
        case HCI_DUMP_STDOUT:
            /* Obtain the time of day, and convert it to a tm struct. */
            ptm = localtime (&curr_time.tv_sec);
            /* Format the date and time, down to a single second. */
            strftime (time_string, sizeof (time_string), "[%Y-%m-%d %H:%M:%S", ptm);
            /* Compute milliseconds from microseconds. */
            uint16_t milliseconds = curr_time.tv_usec / 1000;
            /* Print the formatted time, in seconds, followed by a decimal point
             and the milliseconds. */
            printf ("%s.%03u] ", time_string, milliseconds);
            switch (packet_type){
                case HCI_COMMAND_DATA_PACKET:
                    printf("CMD => ");
                    break;
                case HCI_EVENT_PACKET:
                    printf("EVT <= ");
                    break;
                case HCI_ACL_DATA_PACKET:
                    if (in) {
                        printf("ACL <= ");
                    } else {
                        printf("ACL => ");
                    }
                    break;
            }
            hexdump(packet, len);
            break;
        case HCI_DUMP_BLUEZ:
            bt_store_16( (uint8_t *) &header_bluez.len, 0, 1 + len);
            header_bluez.in  = in;
            header_bluez.pad = 0;
            bt_store_32( (uint8_t *) &header_bluez.ts_sec,  0, curr_time.tv_sec);
            bt_store_32( (uint8_t *) &header_bluez.ts_usec, 0, curr_time.tv_usec);
            header_bluez.packet_type = packet_type;
            write (dump_file, &header_bluez, sizeof(hcidump_hdr) );
            write (dump_file, packet, len );
            break;
        case HCI_DUMP_PACKETLOGGER:
            header_packetlogger.len = htonl( sizeof(pktlog_hdr) - 4 + len);
            header_packetlogger.ts_sec =  htonl(curr_time.tv_sec);
            header_packetlogger.ts_usec = htonl(curr_time.tv_usec);
            switch (packet_type){
                case HCI_COMMAND_DATA_PACKET:
                    header_packetlogger.type = 0x00;
                    break;
                case HCI_ACL_DATA_PACKET:
                    if (in) {
                        header_packetlogger.type = 0x03;
                    } else {
                        header_packetlogger.type = 0x02;
                    }
                    break;
                case HCI_EVENT_PACKET:
                    header_packetlogger.type = 0x01;
                    break;
                default:
                    return;
            }
            write (dump_file, &header_packetlogger, sizeof(pktlog_hdr) );
            write (dump_file, packet, len );
    }
}

void hci_dump_close(){
    close(dump_file);
    dump_file = -1;
}

