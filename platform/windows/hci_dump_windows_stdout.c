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

#define BTSTACK_FILE__ "hci_dump_windows_stdout.c"

/*
 *  Dump HCI trace on stdout
 */

#include "hci_dump.h"
#include "btstack_config.h"
#include "hci.h"
#include "hci_cmd.h"
#include <stdio.h>
#include <Windows.h>

#include "hci_dump_windows_stdout.h"

#ifndef ENABLE_PRINTF_HEXDUMP
#error "HCI Dump on stdout requires ENABLE_PRINTF_HEXDUMP to be defined. Use different hci dump implementation or add ENABLE_PRINTF_HEXDUMP to btstack_config.h"
#endif

static char log_message_buffer[HCI_DUMP_MAX_MESSAGE_LEN];

static void hci_dump_windows_stdout_timestamp(void){
	SYSTEMTIME lt;
	GetLocalTime(&lt);
	printf("[%04u-%02u-%02u %02u:%02u:%02u.%3u] ", lt.wYear, lt.wMonth, lt.wDay, lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds);
}

static void hci_dump_windows_stdout_packet(uint8_t packet_type, uint8_t in, uint8_t * packet, uint16_t len){
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            printf("CMD => ");
            break;
        case HCI_EVENT_PACKET:
            printf("EVT <= ");
            break;
        case HCI_ACL_DATA_PACKET:
#ifdef HCI_DUMP_STDOUT_MAX_SIZE_ACL
            if (len > HCI_DUMP_STDOUT_MAX_SIZE_ACL){
                printf("LOG -- ACL %s, size %u\n", in ? "in" : "out", len);
                return;
            }
#endif
            if (in) {
                printf("ACL <= ");
            } else {
                printf("ACL => ");
            }
            break;
        case HCI_SCO_DATA_PACKET:
#ifdef HCI_DUMP_STDOUT_MAX_SIZE_SCO
            if (len > HCI_DUMP_STDOUT_MAX_SIZE_SCO){
                printf("LOG -- SCO %s, size %u\n", in ? "in" : "out", len);
                return;
            }
#endif
            if (in) {
                printf("SCO <= ");
            } else {
                printf("SCO => ");
            }
            break;
        case HCI_ISO_DATA_PACKET:
#ifdef HCI_DUMP_STDOUT_MAX_SIZE_ISO
            if (len > HCI_DUMP_STDOUT_MAX_SIZE_ISO){
                printf("LOG -- ISO %s, size %u\n", in ? "in" : "out", len);
                return;
            }
#endif
            if (in) {
                printf("ISO <= ");
            } else {
                printf("ISO => ");
            }
            break;
        case LOG_MESSAGE_PACKET:
            printf("LOG -- %s\n", (char*) packet);
            return;
        default:
            return;
    }
    printf_hexdump(packet, len);
}

static void hci_dump_windows_stdout_log_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len) {
    hci_dump_windows_stdout_timestamp();
    hci_dump_windows_stdout_packet(packet_type, in, packet, len);
}

static void hci_dump_windows_stdout_log_message(int log_level, const char * format, va_list argptr){
    UNUSED(log_level);
    int len = vsnprintf(log_message_buffer, sizeof(log_message_buffer), format, argptr);
    hci_dump_windows_stdout_log_packet(LOG_MESSAGE_PACKET, 0, (uint8_t*) log_message_buffer, len);
}

const hci_dump_t * hci_dump_windows_stdout_get_instance(void){
    static const hci_dump_t hci_dump_instance = {
        // void (*reset)(void);
        NULL,
        // void (*log_packet)(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len);
        &hci_dump_windows_stdout_log_packet,
        // void (*log_message)(int log_level, const char * format, va_list argptr);
        &hci_dump_windows_stdout_log_message,
    };
    return &hci_dump_instance;
}
