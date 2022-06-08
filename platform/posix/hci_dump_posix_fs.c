/*
 * Copyright (C) 2014-2020 BlueKitchen GmbH
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
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "hci_dump_posix_fs.c"

/*
 *  hci_dump_posix_fs.c
 *
 *  Dump HCI trace in various formats into a file:
 *
 *  - BlueZ's hcidump format
 *  - Apple's PacketLogger
 *  - stdout hexdump
 *
 */

#include "btstack_config.h"

// enable POSIX functions (needed for -std=c99)
#define _POSIX_C_SOURCE 200809

#include "hci_dump_posix_fs.h"

#include "btstack_debug.h"
#include "btstack_util.h"
#include "hci_cmd.h"

#include <time.h>
#include <stdio.h>        // printf
#include <fcntl.h>        // open
#include <unistd.h>       // write
#include <errno.h>        // errno
#include <sys/time.h>     // for timestamps
#include <sys/stat.h>     // file modes

static int  dump_file = -1;
static int  dump_format;
static char log_message_buffer[256];

static void hci_dump_posix_fs_reset(void){
    btstack_assert(dump_file >= 0);
    (void) lseek(dump_file, 0, SEEK_SET);
    int err = ftruncate(dump_file, 0);
    UNUSED(err);
}

// provide summary for ISO Data Packets if not supported by fileformat/viewer yet
static uint16_t hci_dump_iso_summary(uint8_t in,  uint8_t *packet, uint16_t len){
    UNUSED(len);
    uint16_t conn_handle = little_endian_read_16(packet, 0) & 0xfff;
    uint8_t pb = (packet[1] >> 4) & 3;
    uint8_t ts = (packet[1] >> 6) & 1;
    uint16_t pos = 4;
    uint32_t time_stamp = 0;
    if (ts){
        time_stamp = little_endian_read_32(packet, pos);
        pos += 4;
    }
    uint16_t packet_sequence = little_endian_read_16(packet, pos);
    pos += 2;
    uint16_t iso_sdu_len = little_endian_read_16(packet, pos);
    uint8_t packet_status_flag = packet[pos+1] >> 6;
    return snprintf(log_message_buffer,sizeof(log_message_buffer), "ISO %s, handle %04x, pb %u, ts 0x%08x, sequence 0x%04x, packet status %u, iso len %u",
                    in ? "IN" : "OUT", conn_handle, pb, time_stamp, packet_sequence, packet_status_flag, iso_sdu_len);
}

static void hci_dump_posix_fs_log_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len) {
    if (dump_file < 0) return;

    static union {
        uint8_t header_bluez[HCI_DUMP_HEADER_SIZE_BLUEZ];
        uint8_t header_packetlogger[HCI_DUMP_HEADER_SIZE_PACKETLOGGER];
        uint8_t header_btsnoop[HCI_DUMP_HEADER_SIZE_BTSNOOP+1];
    } header;

    uint32_t tv_sec = 0;
    uint32_t tv_us  = 0;
    uint64_t ts_usec;

    // get time
    struct timeval curr_time;
    gettimeofday(&curr_time, NULL);
    tv_sec = curr_time.tv_sec;
    tv_us  = curr_time.tv_usec;

    uint16_t header_len = 0;
    switch (dump_format){
        case HCI_DUMP_BLUEZ:
            // ISO packets not supported
            if (packet_type == HCI_ISO_DATA_PACKET){
                len = hci_dump_iso_summary(in, packet, len);
                packet_type = LOG_MESSAGE_PACKET;
                packet = (uint8_t*) log_message_buffer;
            }
            hci_dump_setup_header_bluez(header.header_bluez, tv_sec, tv_us, packet_type, in, len);
            header_len = HCI_DUMP_HEADER_SIZE_BLUEZ;
            break;
        case HCI_DUMP_PACKETLOGGER:
            // ISO packets not supported
            if (packet_type == HCI_ISO_DATA_PACKET){
                len = hci_dump_iso_summary(in, packet, len);
                packet_type = LOG_MESSAGE_PACKET;
                packet = (uint8_t*) log_message_buffer;
            }
            hci_dump_setup_header_packetlogger(header.header_packetlogger, tv_sec, tv_us, packet_type, in, len);
            header_len = HCI_DUMP_HEADER_SIZE_PACKETLOGGER;
            break;
        case HCI_DUMP_BTSNOOP:
            // log messages not supported
            if (packet_type == LOG_MESSAGE_PACKET) return;
            ts_usec = 0xdcddb30f2f8000LLU + 1000000LLU * curr_time.tv_sec + curr_time.tv_usec;
            // append packet type to pcap header
            hci_dump_setup_header_btsnoop(header.header_btsnoop, ts_usec >> 32, ts_usec & 0xFFFFFFFF, 0, packet_type, in, len+1);
            header.header_btsnoop[HCI_DUMP_HEADER_SIZE_BTSNOOP] = packet_type;
            header_len = HCI_DUMP_HEADER_SIZE_BTSNOOP + 1;
            break;
        default:
            btstack_unreachable();
            return;
    }

    ssize_t bytes_written;
    bytes_written = write(dump_file, &header, header_len);
    UNUSED(bytes_written);
    bytes_written = write(dump_file, packet, len );
    UNUSED(bytes_written);
}

static void hci_dump_posix_fs_log_message(int log_level, const char * format, va_list argptr){
    UNUSED(log_level);
    if (dump_file < 0) return;
    int len = vsnprintf(log_message_buffer, sizeof(log_message_buffer), format, argptr);
    hci_dump_posix_fs_log_packet(LOG_MESSAGE_PACKET, 0, (uint8_t*) log_message_buffer, len);
}

// returns system errno
int hci_dump_posix_fs_open(const char *filename, hci_dump_format_t format){
    btstack_assert(format == HCI_DUMP_BLUEZ || format == HCI_DUMP_PACKETLOGGER || format == HCI_DUMP_BTSNOOP);

    dump_format = format;
    int oflags = O_WRONLY | O_CREAT | O_TRUNC;
#ifdef _WIN32
    oflags |= O_BINARY;
#endif
    dump_file = open(filename, oflags, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
    if (dump_file < 0){
        printf("failed to open file %s, errno = %d\n", filename, errno);
        return errno;
    }

    if (format == HCI_DUMP_BTSNOOP){
        // write BTSnoop file header
        const uint8_t file_header[] = {
            // Identification Pattern: "btsnoop\0"
            0x62, 0x74, 0x73, 0x6E, 0x6F, 0x6F, 0x70, 0x00,
            // Version: 1
            0x00, 0x00, 0x00, 0x01,
            // Datalink Type: 1002 - H4
            0x00, 0x00, 0x03, 0xEA,
        };
        ssize_t bytes_written = write(dump_file, &file_header, sizeof(file_header));
        UNUSED(bytes_written);
    }
    return 0;
}

void hci_dump_posix_fs_close(void){
    close(dump_file);
    dump_file = -1;
}

const hci_dump_t * hci_dump_posix_fs_get_instance(void){
    static const hci_dump_t hci_dump_instance = {
        // void (*reset)(void);
        &hci_dump_posix_fs_reset,
        // void (*log_packet)(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len);
        &hci_dump_posix_fs_log_packet,
        // void (*log_message)(int log_level, const char * format, va_list argptr);
        &hci_dump_posix_fs_log_message,
    };
    return &hci_dump_instance;
}
