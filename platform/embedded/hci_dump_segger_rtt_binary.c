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
 * Please inquire about commercial licensing options at
 * contact@bluekitchen-gmbh.com
 *
 */

#define BTSTACK_FILE__ "hci_dump_segger_rtt_binary.c"

/**
 * Dump HCI trace on SEGGER RTT Logging Channel #1
 */
#include "btstack_config.h"

#include "hci_dump_segger_rtt_binary.h"

#include "btstack_debug.h"
#include "hci.h"

#include "SEGGER_RTT.h"

// allow to configure mode, channel, up buffer size in btstack_config.h
#ifndef SEGGER_RTT_PACKETLOG_MODE
#define SEGGER_RTT_PACKETLOG_MODE SEGGER_RTT_MODE_DEFAULT
#endif
#ifndef SEGGER_RTT_PACKETLOG_BUFFER_SIZE
#define SEGGER_RTT_PACKETLOG_BUFFER_SIZE 1024
#endif
#ifndef SEGGER_RTT_PACKETLOG_CHANNEL
#define SEGGER_RTT_PACKETLOG_CHANNEL 1
#endif

static char segger_rtt_packetlog_buffer[SEGGER_RTT_PACKETLOG_BUFFER_SIZE];

static int  dump_format;
static char log_message_buffer[256];

static void hci_dump_segger_rtt_binary_log_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len) {
    if (dump_format == HCI_DUMP_INVALID) return;

    static union {
        uint8_t header_bluez[HCI_DUMP_HEADER_SIZE_BLUEZ];
        uint8_t header_packetlogger[HCI_DUMP_HEADER_SIZE_PACKETLOGGER];
    } header;

    uint32_t time_ms = btstack_run_loop_get_time_ms();
    uint32_t tv_sec  = time_ms / 1000u;
    uint32_t tv_us   = (time_ms - (tv_sec * 1000)) * 1000;
    // Saturday, January 1, 2000 12:00:00
    tv_sec += 946728000UL;

#if (SEGGER_RTT_PACKETLOG_MODE == SEGGER_RTT_MODE_NO_BLOCK_SKIP)
    static const char rtt_warning[] = "RTT buffer full - packet(s) skipped";
    static bool rtt_packet_skipped = false;
    if (rtt_packet_skipped){
        // try to write warning log message
        rtt_packet_skipped = false;
        packet_type = LOG_MESSAGE_PACKET;
        packet      = (uint8_t *) &rtt_warning[0];
        len         = sizeof(rtt_warning)-1;
    }
#endif

    uint16_t header_len = 0;
    switch (dump_format){
        case HCI_DUMP_BLUEZ:
            hci_dump_setup_header_bluez(header.header_bluez, tv_sec, tv_us, packet_type, in, len);
            header_len = HCI_DUMP_HEADER_SIZE_BLUEZ;
            break;
        case HCI_DUMP_PACKETLOGGER:
            hci_dump_setup_header_packetlogger(header.header_packetlogger, tv_sec, tv_us, packet_type, in, len);
            header_len = HCI_DUMP_HEADER_SIZE_PACKETLOGGER;
            break;
        default:
            return;
    }

#if (SEGGER_RTT_PACKETLOG_MODE == SEGGER_RTT_MODE_NO_BLOCK_SKIP)
    // check available space in buffer to avoid writing header but not packet
    unsigned space_free = SEGGER_RTT_GetAvailWriteSpace(SEGGER_RTT_PACKETLOG_CHANNEL);
    if ((header_len + len) > space_free) {
        rtt_packet_skipped = true;
        return;
    }
#endif

    // write complete header and payload
    SEGGER_RTT_Write(SEGGER_RTT_PACKETLOG_CHANNEL, &header, header_len);
    SEGGER_RTT_Write(SEGGER_RTT_PACKETLOG_CHANNEL, packet, len);
}

static void hci_dump_segger_rtt_binary_log_message(const char * format, va_list argptr){
    if (dump_format == HCI_DUMP_INVALID) return;
    int len = vsnprintf(log_message_buffer, sizeof(log_message_buffer), format, argptr);
    hci_dump_segger_rtt_binary_log_packet(LOG_MESSAGE_PACKET, 0, (uint8_t*) log_message_buffer, len);
}

//

void hci_dump_segger_rtt_binary_open(hci_dump_format_t format){
    btstack_assert(format == HCI_DUMP_BLUEZ || format == HCI_DUMP_PACKETLOGGER);
    dump_format = format;
    SEGGER_RTT_ConfigUpBuffer(SEGGER_RTT_PACKETLOG_CHANNEL, "hci_dump", &segger_rtt_packetlog_buffer[0], SEGGER_RTT_PACKETLOG_BUFFER_SIZE, SEGGER_RTT_PACKETLOG_MODE);
}

const hci_dump_t * hci_dump_segger_rtt_binary_get_instance(void){
    static const hci_dump_t hci_dump_instance = {
        // void (*reset)(void);
        NULL,
        // void (*log_packet)(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len);
        &hci_dump_segger_rtt_binary_log_packet,
        // void (*log_message)(int log_level, const char * format, va_list argptr);
        &hci_dump_segger_rtt_binary_log_message,
    };
    return &hci_dump_instance;
}
