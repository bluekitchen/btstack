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

#define BTSTACK_FILE__ "hci_dump_embedded_stdout.c"

/*
 *  Dump HCI trace on stdout
 */

#include "btstack_config.h"

#include "hci_dump_embedded_stdout.h"
#include "btstack_run_loop.h"
#include "hci.h"

#include <stdio.h>

#ifndef ENABLE_PRINTF_HEXDUMP
#error "HCI Dump on stdout requires ENABLE_PRINTF_HEXDUMP to be defined. Use different hci dump implementation or add ENABLE_PRINTF_HEXDUMP to btstack_config.h"
#endif

static char log_message_buffer[256];

static void hci_dump_embedded_stdout_timestamp(void){
    uint32_t time_ms = btstack_run_loop_get_time_ms();
    int      seconds = time_ms / 1000u;
    int      minutes = seconds / 60;
    unsigned int hours = minutes / 60;

    uint16_t p_ms      = time_ms - (seconds * 1000u);
    uint16_t p_seconds = seconds - (minutes * 60);
    uint16_t p_minutes = minutes - (hours   * 60u);     
    printf("[%02u:%02u:%02u.%03u] ", hours, p_minutes, p_seconds, p_ms);
}

static void hci_dump_embedded_stdout_packet(uint8_t packet_type, uint8_t in, uint8_t * packet, uint16_t len){

    hci_log(packet_type, in, packet, len);

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
        case HCI_SCO_DATA_PACKET:
            if (in) {
                printf("SCO <= ");
            } else {
                printf("SCO => ");
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

extern void hal_uart_dma_send_block_for_hci(const uint8_t *data, uint16_t size);

void hci_log(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len)
{
typedef enum {
    BT_HCI_LOG_CMD     = 0x01,
    BT_HCI_LOG_ACL_OUT = 0x02,
    BT_HCI_LOG_ACL_IN  = 0x04,
    BT_HCI_LOG_EVT     = 0x08
} bt_hci_log_type_t;

#define BT_HCI_LOG_HEADER_LEGNTH 5 //header + direction + sizeof(log_length)
#define BT_UART_CMD 0x01
#define	BT_UART_ACL 0x02
#define	BT_UART_EVT 0x04

    bt_hci_log_type_t type;
    uint16_t data_tatal_length = 0;
    uint16_t index = 0, i = 0;
    uint8_t *buf = NULL;
    uint8_t check_sum = 0;

    if (packet_type == BT_UART_EVT && packet[0] > 0x3e && packet[0] != 0xFF) {
        return;
    }

    if (in == 0) {
        if (packet_type == BT_UART_CMD) {
            type = BT_HCI_LOG_CMD;
        } else if (packet_type == BT_UART_ACL) {
            type = BT_HCI_LOG_ACL_OUT;
        } else {
            return;
        }
    } else if (in == 1) {
        if (packet_type == BT_UART_ACL) {
            type = BT_HCI_LOG_ACL_IN;
        } else if (packet_type == BT_UART_EVT) {
            type = BT_HCI_LOG_EVT;
        } else {
            return;
        }
    }

    data_tatal_length = BT_HCI_LOG_HEADER_LEGNTH + len + 1;//1:check sum
    buf = (uint8_t *)malloc(data_tatal_length);
    //BT_ASSERT(buf);

    buf[index++] = 0xF5;
    buf[index++] = 0x5A;
    buf[index++] = type;
    buf[index++] = len & 0xFF;
    buf[index++] = (len >> 8) & 0xFF;
    for (i = 0; i < len; index++, i++) {
        buf[index] = packet[i];
    }
    for (i = 0; i < data_tatal_length - 1; i++) {
        check_sum += buf[i];
    }
    buf[index] = check_sum;

    hal_uart_dma_send_block_for_hci(buf, data_tatal_length);

    free(buf);
}

static void hci_dump_embedded_stdout_log_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len) {
    hci_dump_embedded_stdout_timestamp();
    hci_dump_embedded_stdout_packet(packet_type, in, packet, len);
}

static void hci_dump_embedded_stdout_log_message(const char * format, va_list argptr){
    int len = vsnprintf(log_message_buffer, sizeof(log_message_buffer), format, argptr);
    hci_dump_embedded_stdout_log_packet(LOG_MESSAGE_PACKET, 0, (uint8_t*) log_message_buffer, len);
}

#ifdef __AVR__
void hci_dump_embedded_stdout_log_message_P(int log_level, PGM_P * format, va_list argptr){
    hci_dump_embedded_stdout_timestamp();
    printf_P(PSTR("LOG -- "));
    vfprintf_P(stdout, format, argptr);
    printf_P(PSTR("\n"));
}
#endif

const hci_dump_t * hci_dump_embedded_stdout_get_instance(void){
    static const hci_dump_t hci_dump_instance = {
        // void (*reset)(void);
        NULL,
        // void (*log_packet)(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len);
        &hci_dump_embedded_stdout_log_packet,
        // void (*log_message)(int log_level, const char * format, va_list argptr);
        &hci_dump_embedded_stdout_log_message,
#ifdef __AVR__ \
        // void (*log_message_P)(int log_level, PGM_P * format, va_list argptr);
        &hci_dump_embedded_stdout_log_message_P,
#endif
    };
    return &hci_dump_instance;
}
