/*
 * Copyright (C) 2014 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hci_dump.c"

/*
 *  hci_dump.c
 *
 *  Dump HCI trace in various formats based on platform-specific implementation
 */

#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_bool.h"
#include "btstack_util.h"

static const hci_dump_t * hci_dump_implementation;
static int  max_nr_packets;
static int  nr_packets;
static bool packet_log_enabled;

// levels: debug, info, error
static bool log_level_enabled[3] = { 1, 1, 1};

static bool hci_dump_log_level_active(int log_level){
    if (hci_dump_implementation == NULL) return false;
    if (log_level < HCI_DUMP_LOG_LEVEL_DEBUG) return false;
    if (log_level > HCI_DUMP_LOG_LEVEL_ERROR) return false;
    return log_level_enabled[log_level];
}

void hci_dump_init(const hci_dump_t * hci_dump_impl){
    max_nr_packets = -1;
    nr_packets = 0;
    hci_dump_implementation = hci_dump_impl;
    packet_log_enabled = true;
}

void hci_dump_set_max_packets(int packets){
    max_nr_packets = packets;
}

void hci_dump_enable_packet_log(bool enabled){
    packet_log_enabled = enabled;
}

void hci_dump_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len) {
    if (hci_dump_implementation == NULL) {
        return;
    }
    if (packet_log_enabled == false) {
        return;
    }

    if (max_nr_packets > 0){
        if ((nr_packets >= max_nr_packets) && (hci_dump_implementation->reset != NULL)) {
            nr_packets = 0;
            (*hci_dump_implementation->reset)();
        }
        nr_packets++;
    }
    (*hci_dump_implementation->log_packet)(packet_type, in, packet, len);
}

void hci_dump_log(int log_level, const char * format, ...){
    if (!hci_dump_log_level_active(log_level)) return;

    va_list argptr;
    va_start(argptr, format);
    (*hci_dump_implementation->log_message)(log_level, format, argptr);
    va_end(argptr);
}

#ifdef __AVR__
void hci_dump_log_P(int log_level, PGM_P format, ...){
    if (!hci_dump_log_level_active(log_level)) return;

    va_list argptr;
    va_start(argptr, format);
    (*hci_dump_implementation->log_message_P)(log_level, format, argptr);
    va_end(argptr);
}
#endif

void hci_dump_enable_log_level(int log_level, int enable){
    if (log_level < HCI_DUMP_LOG_LEVEL_DEBUG) return;
    if (log_level > HCI_DUMP_LOG_LEVEL_ERROR) return;
    log_level_enabled[log_level] = enable != 0;
}

void hci_dump_setup_header_packetlogger(uint8_t * buffer, uint32_t tv_sec, uint32_t tv_us, uint8_t packet_type, uint8_t in, uint16_t len){
    big_endian_store_32( buffer, 0, HCI_DUMP_HEADER_SIZE_PACKETLOGGER - 4 + len);
    big_endian_store_32( buffer, 4, tv_sec);
    big_endian_store_32( buffer, 8, tv_us);
    uint8_t packet_logger_type = 0;
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            packet_logger_type = 0x00;
            break;
        case HCI_ACL_DATA_PACKET:
            packet_logger_type = in ? 0x03 : 0x02;
            break;
        case HCI_SCO_DATA_PACKET:
            packet_logger_type = in ? 0x09 : 0x08;
            break;
        case HCI_EVENT_PACKET:
            packet_logger_type = 0x01;
            break;
        case LOG_MESSAGE_PACKET:
            packet_logger_type = 0xfc;
            break;
        default:
            return;
    }
    buffer[12] = packet_logger_type;
}

void hci_dump_setup_header_bluez(uint8_t * buffer, uint32_t tv_sec, uint32_t tv_us, uint8_t packet_type, uint8_t in, uint16_t len){
    little_endian_store_16( buffer, 0u, 1u + len);
    buffer[2] = in;
    buffer[3] = 0;
    little_endian_store_32( buffer, 4, tv_sec);
    little_endian_store_32( buffer, 8, tv_us);
    buffer[12] = packet_type;
}

// From https://fte.com/webhelpii/hsu/Content/Technical_Information/BT_Snoop_File_Format.htm
void hci_dump_setup_header_btsnoop(uint8_t * buffer, uint32_t ts_usec_high, uint32_t ts_usec_low, uint32_t cumulative_drops, uint8_t packet_type, uint8_t in, uint16_t len) {
    uint32_t packet_flags = 0;
    if (in){
        packet_flags |= 1;
    }
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
        case HCI_EVENT_PACKET:
            packet_flags |= 2;
        default:
            break;
    }
    big_endian_store_32(buffer,  0, len);               // Original Length
    big_endian_store_32(buffer,  4, len);               // Included Length
    big_endian_store_32(buffer,  8, packet_flags);      // Packet Flags
    big_endian_store_32(buffer, 12, cumulative_drops);  // Cumulativ Drops
    big_endian_store_32(buffer, 16, ts_usec_high);            // Timestamp Microseconds High
    big_endian_store_32(buffer, 20, ts_usec_low);             // Timestamp Microseconds Low
}
