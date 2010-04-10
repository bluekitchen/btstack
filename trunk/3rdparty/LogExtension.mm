/*
 * Copyright (C) 2009 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#import <Foundation/Foundation.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdint.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/time.h>

// hooking part
#include "3rdparty/substrate.h"

// hci_dump.h
typedef enum {
    HCI_DUMP_BLUEZ = 0,
    HCI_DUMP_PACKETLOGGER,
	HCI_DUMP_RAW,
    HCI_DUMP_STDOUT
} hci_dump_format_t;

static void hci_dump_open(char *filename, hci_dump_format_t format);
static void hci_dump_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len);
static void hci_dump_close();

// H5 Slip Decoder

// h5 slip state machine
typedef enum {
	unknown = 1,
	decoding,
	x_c0,
	x_db
} state_t;

typedef struct h5_slip {
	state_t state;
	uint16_t length;
	uint8_t data[1000];
} h5_slip_t;

// Global State
static h5_slip_t read_sm;
static h5_slip_t write_sm;
static int bt_filedesc = 0;

void h5_slip_init( h5_slip_t * sm){
	sm->state = unknown;
}

void h5_slip_process( h5_slip_t * sm, uint8_t input, uint8_t in){
	uint8_t type;
	switch (sm->state) {
		case unknown:
			if (input == 0xc0){
				sm->state = x_c0;
			}
			break;
		case decoding:
			switch (input ){
				case 0xc0:
					// packet done
					type = sm->data[1] & 0x0f;
					if (type >= 1 && type <= 4 && sm->length >= 6){
						hci_dump_packet( type, in, &sm->data[4], sm->length-4-2); // -4 header, -2 crc for reliable
					}
					sm->state = unknown;
					break;
				case 0xdb:
					sm->state = x_db;
					break;
				default:
					sm->data[sm->length] = input;
					++sm->length;
					break;
			}
			break;
		case x_c0:
			switch (input ){
				case 0xc0:
					break;
				case 0xdb:
					sm->state = x_db;
					break;
				default:
					sm->data[0] =input;
					sm->length  = 1;
					sm->state   = decoding;
					break;
			}
			break;
		case x_db:
			switch (input) {
				case 0xdc:
					sm->data[sm->length] = 0xc0;
					++sm->length;
					sm->state = decoding;
					break;
				case 0xdd:
					sm->data[sm->length] = 0xdb;
					++sm->length;
					sm->state = decoding;
					break;
				default:
					sm->state = unknown;
					break;
			}
			break;
		default:
			break;
	}
}

MSHook(int, socket, int domain, int type, int protocol){
	// call orig
	int res = _socket(domain, type, protocol);
	
	// socket(0x20, 0x1, 0x2);
	if (domain == 0x20 && type == 0x01 && protocol == 0x02){
		printf("Opening BT device\n");
		bt_filedesc = res;
		hci_dump_open( "/tmp/BTServer.pklg", HCI_DUMP_PACKETLOGGER);
		h5_slip_init(&read_sm);
		h5_slip_init(&write_sm);
	}
	return res;
}

MSHook( ssize_t, write, int fildes, const void *buf, size_t nbyte){
	if (fildes && fildes == bt_filedesc && nbyte > 0){
		int16_t i;
		for (i=0;i<nbyte;i++){
			h5_slip_process( &write_sm, ((uint8_t *)buf)[i], 0);
		}
	}
	return _write( fildes, buf, nbyte);
}

MSHook( ssize_t, read, int fildes, void *buf, size_t nbyte){
	ssize_t res = _read( fildes, buf, nbyte);
	if (fildes && fildes == bt_filedesc && res > 0){
		int16_t i;
		for (i=0;i<res;i++){
			h5_slip_process( &read_sm, ((uint8_t *)buf)[i], 1);
		}
	}
	return res;
}

MSHook( int, close, int fildes){
	int res = _close( fildes);
	if (fildes && fildes == bt_filedesc){
		printf("Closing BT device\n");
		hci_dump_close();
	}
	return res;
}


extern "C" void LogExtensionInitialize() {
	NSLog(@"LogExtensionInitialize!");
	
	MSHookFunction(&socket, MSHake(socket));
	MSHookFunction(&read, MSHake(read));
	MSHookFunction(&write, MSHake(write));
	MSHookFunction(&close, MSHake(close));
}

// HCI_DUMP

/**
 * packet types - used in BTstack and over the H4 UART interface
 */
#define HCI_COMMAND_DATA_PACKET	0x01
#define HCI_ACL_DATA_PACKET	    0x02
#define HCI_SCO_DATA_PACKET	    0x03
#define HCI_EVENT_PACKET	    0x04

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

static void bt_store_16(uint8_t *buffer, uint16_t pos, uint16_t value){
    buffer[pos++] = value;
    buffer[pos++] = value >> 8;
}

static void bt_store_32(uint8_t *buffer, uint16_t pos, uint32_t value){
    buffer[pos++] = value;
    buffer[pos++] = value >> 8;
    buffer[pos++] = value >> 16;
    buffer[pos++] = value >> 24;
}

static void hexdump(void *data, int size){
    int i;
    for (i=0; i<size;i++){
        printf("%02X ", ((uint8_t *)data)[i]);
    }
    printf("\n");
}

static void hexdump2(void *data, int size){
    int i;
	char buf[10];
	buf[2] = ' ';
    for (i=0; i<size;i++){
        sprintf(buf, "%02X ", ((uint8_t *)data)[i]);
		_write(dump_file, buf, 3);
    }
    _write(dump_file, "\n", 1);
}

static void hci_dump_open(char *filename, hci_dump_format_t format){
    dump_format = format;
    if (dump_format == HCI_DUMP_STDOUT) {
        dump_file = fileno(stdout);
    } else {
        // dump_file =  open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        dump_file =  open(filename, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    }
}

static void hci_dump_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len) {
    if (dump_file < 0) return; // not activated yet
    
    // get time
    struct timeval curr_time;
    struct tm* ptm;
    gettimeofday(&curr_time, NULL);
    char type = '0'+packet_type;
	
    switch (dump_format){
			
		case HCI_DUMP_RAW:
			if (in) {
				_write (dump_file, "READ: ", 6);
			} else {
				_write (dump_file, "WRITE: ", 7);
			}
			_write(dump_file, &type, 1);
			_write(dump_file, " ", 1);
			hexdump2(packet, len);
			break;
			
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
            _write (dump_file, &header_bluez, sizeof(hcidump_hdr) );
            _write (dump_file, packet, len );
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
            _write (dump_file, &header_packetlogger, sizeof(pktlog_hdr) );
            _write (dump_file, packet, len );
    }
}

static void hci_dump_close(){
    _close(dump_file);
    dump_file = -1;
}
