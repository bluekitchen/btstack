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

#define BTSTACK_FILE__ "hci_dump_embedded_stdout.c"

/*
 *  Dump HCI trace on stdout
 */

#include "btstack_config.h"

#ifdef ENABLE_SEGGER_RTT

#include "hci_dump.h"
#include "btstack_util.h"
#include "hci.h"
#include "hci_dump_segger_rtt_stdout.h"

#include "SEGGER_RTT.h"

// [00:00:37.514] LOG -- xxx\n
#define ADDITIONAL_CHARS_PER_LINE (14+1+6+1+1)

static void hci_dump_segger_rtt_stdout_timestamp(void);

#if SEGGER_RTT_MODE_DEFAULT != SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL

// avoid trunkated log lines by calculating requires space in rtt buffer
// if message needs to be skipped, report number skipped messages

static const char * message_packets_skipped = "RTT buffer full, %3u packet(s) skipped";

static bool hci_dump_segger_prepare_message(uint16_t message_size){

    static uint32_t hci_dump_rtt_num_skipped = 0;

    // calculate num chars required for line
    uint16_t required_space = ADDITIONAL_CHARS_PER_LINE + message_size;

    // if we have dropped packets before, we want to print a warning
    if (hci_dump_rtt_num_skipped > 0){
        required_space += ADDITIONAL_CHARS_PER_LINE + strlen(message_packets_skipped);
    }

    // not enough space yet, drop message
    if (required_space > SEGGER_RTT_GetAvailWriteSpace(0)){
        hci_dump_rtt_num_skipped++;
        return false;
    }

    // print message if dropped packets
    if (hci_dump_rtt_num_skipped > 0){
        hci_dump_segger_rtt_stdout_timestamp();
        SEGGER_RTT_printf(0, "LOG -- ");
        SEGGER_RTT_printf(0, message_packets_skipped, hci_dump_rtt_num_skipped);
        SEGGER_RTT_printf(0, "\n");
        hci_dump_rtt_num_skipped = 0;
    }
    return true;
}
#endif

static void hci_dump_segger_rtt_hexdump(const void *data, int size){
    char buffer[4];
    buffer[2] = ' ';
    buffer[3] =  0;
    const uint8_t * ptr = (const uint8_t *) data;
    while (size > 0){
        uint8_t byte = *ptr++;
        buffer[0] = char_for_nibble(byte >> 4);
        buffer[1] = char_for_nibble(byte & 0x0f);
        SEGGER_RTT_Write(0, buffer, 3);
        size--;
    }
    SEGGER_RTT_PutChar(0, '\n');
}

static void hci_dump_segger_rtt_stdout_timestamp(void){
    uint32_t time_ms = btstack_run_loop_get_time_ms();
    int      seconds = time_ms / 1000u;
    int      minutes = seconds / 60;
    unsigned int hours = minutes / 60;

    uint16_t p_ms      = time_ms - (seconds * 1000u);
    uint16_t p_seconds = seconds - (minutes * 60);
    uint16_t p_minutes = minutes - (hours   * 60u);
    SEGGER_RTT_printf(0, "[%02u:%02u:%02u.%03u] ", hours, p_minutes, p_seconds, p_ms);
}

static void hci_dump_segger_rtt_stdout_packet(uint8_t packet_type, uint8_t in, uint8_t * packet, uint16_t len){
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            SEGGER_RTT_printf(0, "CMD => ");
            break;
        case HCI_EVENT_PACKET:
            SEGGER_RTT_printf(0, "EVT <= ");
            break;
        case HCI_ACL_DATA_PACKET:
            if (in) {
                SEGGER_RTT_printf(0, "ACL <= ");
            } else {
                SEGGER_RTT_printf(0, "ACL => ");
            }
            break;
        case HCI_SCO_DATA_PACKET:
            if (in) {
                SEGGER_RTT_printf(0, "SCO <= ");
            } else {
                SEGGER_RTT_printf(0, "SCO => ");
            }
            break;
        case HCI_ISO_DATA_PACKET:
            if (in) {
                SEGGER_RTT_printf(0, "ISO <= ");
            } else {
                SEGGER_RTT_printf(0, "ISO => ");
            }
            break;
        default:
            return;
    }
    hci_dump_segger_rtt_hexdump(packet, len);
}

static void hci_dump_segger_rtt_stdout_log_packet(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len) {
#if SEGGER_RTT_MODE_DEFAULT != SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL
    uint16_t message_len = len * 3;
    bool ready = hci_dump_segger_prepare_message(message_len);
    if (!ready) return;
#endif

    // print packet
    hci_dump_segger_rtt_stdout_timestamp();
    hci_dump_segger_rtt_stdout_packet(packet_type, in, packet, len);
}

static void hci_dump_segger_rtt_stdout_log_message(int log_level, const char * format, va_list argptr){
    UNUSED(log_level);
#if SEGGER_RTT_MODE_DEFAULT != SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL
    // to avoid using snprintf for this, we cheat and assume that the messages is less then HCI_DUMP_MAX_MESSAGE_LEN
    bool ready = hci_dump_segger_prepare_message(HCI_DUMP_MAX_MESSAGE_LEN);
    if (!ready) return;
#endif

    // print message
    hci_dump_segger_rtt_stdout_timestamp();
    SEGGER_RTT_printf(0, "LOG -- ");
    SEGGER_RTT_vprintf(0, format, &argptr);
    SEGGER_RTT_printf(0, "\n");
}

const hci_dump_t * hci_dump_segger_rtt_stdout_get_instance(void){
    static const hci_dump_t hci_dump_instance = {
        // void (*reset)(void);
        NULL,
        // void (*log_packet)(uint8_t packet_type, uint8_t in, uint8_t *packet, uint16_t len);
        &hci_dump_segger_rtt_stdout_log_packet,
        // void (*log_message)(int log_level, const char * format, va_list argptr);
        &hci_dump_segger_rtt_stdout_log_message,
    };
    return &hci_dump_instance;
}

#endif
