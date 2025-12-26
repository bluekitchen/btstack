/*
 * Copyright (C) 2025 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hci_dump_js.c"

#include "hci_dump_js.h"

#include "btstack_bool.h"
#include "btstack_config.h"
#include "btstack_debug.h"
#include "btstack_run_loop.h"

#include <stdint.h>
#include <stdio.h>

#include <emscripten/emscripten.h>

#include "btstack_util.h"


static int  dump_format = HCI_DUMP_PACKETLOGGER;
static char log_message_buffer[256];

static void hci_dump_js_log_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len) {
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

    // pass header to JS
    EM_ASM({
        data = new Uint8Array(HEAPU8.buffer, $0, $1);
        hci_dump_js_write_blob(data);
    }, &header, header_len);

    // pass packet to JS
    EM_ASM({
        data = new Uint8Array(HEAPU8.buffer, $0, $1);
        hci_dump_js_write_blob(data);
    }, packet, len);
}

static void hci_dump_js_log_message(int log_level, const char * format, va_list argptr){
    UNUSED(log_level);
    if (dump_format == HCI_DUMP_INVALID) return;
    int len = vsnprintf(log_message_buffer, sizeof(log_message_buffer), format, argptr);
    hci_dump_js_log_packet(LOG_MESSAGE_PACKET, 0, (uint8_t*) log_message_buffer, len);
}

const hci_dump_t * hci_dump_js_get_instance(void){
    static const hci_dump_t hci_dump_instance = {
        // void (*reset)(void);
        NULL,
        // void (*log_packet)(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len);
        &hci_dump_js_log_packet,
        // void (*log_message)(int log_level, const char * format, va_list argptr);
        &hci_dump_js_log_message,
    };
    return &hci_dump_instance;
}

