/*
 * Copyright (C) 2020 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "hci_event.c"

/*
 *  hci_event.c
 */

#include "btstack_config.h"

#include "hci.h"
#include "hci_event.h"
#include "btstack_defines.h"
#include "btstack_debug.h"

#include <string.h>

/**
 * construct HCI Event based on template
 *
 * Format:
 *   1,2,3,4: one to four byte value
 *   H: HCI connection handle
 *   B: Bluetooth Baseband Address (BD_ADDR)
 *   D: 8 byte data block
 *   P: 16 byte data block.
 *   Q: 32 byte data block, e.g. for X and Y coordinates of P-256 public key
 *   J: 1-byte lenght of following variable-length data blob 'V'
 *   K: 1-byte length of following variable-length data blob 'V', length is not included in packet
 *   V: variable-length data blob of len provided in 'J' field
 */
uint16_t hci_event_create_from_template_and_arglist(uint8_t *hci_event_buffer, const hci_event_t *event, va_list argptr){
    
    hci_event_buffer[0] = event->event_code;
    uint16_t pos = 2;

    // store subevent code if set
    if (event->subevent_code != 0){
        hci_event_buffer[pos++] = event->subevent_code;
    }
    
    const char *format = event->format;
    uint16_t word;
    uint32_t longword;
    uint8_t * ptr;
    uint16_t length_j = 0xffff;
    while (*format) {
        switch(*format) {
            case '1': //  8 bit value
            case '2': // 16 bit value
            case 'H': // hci_handle
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                hci_event_buffer[pos++] = word & 0xff;
                if (*format != '1') {
                    hci_event_buffer[pos++] = word >> 8;
                }
                break;
            case '3':
            case '4':
                longword = va_arg(argptr, uint32_t);
                // longword = va_arg(argptr, int);
                hci_event_buffer[pos++] = longword;
                hci_event_buffer[pos++] = longword >> 8;
                hci_event_buffer[pos++] = longword >> 16;
                if (*format == '4'){
                    hci_event_buffer[pos++] = longword >> 24;
                }
                break;
            case 'B': // bt-addr
                ptr = va_arg(argptr, uint8_t *);
                hci_event_buffer[pos++] = ptr[5];
                hci_event_buffer[pos++] = ptr[4];
                hci_event_buffer[pos++] = ptr[3];
                hci_event_buffer[pos++] = ptr[2];
                hci_event_buffer[pos++] = ptr[1];
                hci_event_buffer[pos++] = ptr[0];
                break;
            case 'D': // 8 byte data block
                ptr = va_arg(argptr, uint8_t *);
                (void)memcpy(&hci_event_buffer[pos], ptr, 8);
                pos += 8;
                break;
            case 'P': // 16 byte block
                ptr = va_arg(argptr, uint8_t *);
                (void)memcpy(&hci_event_buffer[pos], ptr, 16);
                pos += 16;
                break;
            case 'Q':
                ptr = va_arg(argptr, uint8_t *);
                reverse_bytes(ptr, &hci_event_buffer[pos], 32);
                pos += 32;
                break;
            case 'J':
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                length_j = word &  0xff;
                hci_event_buffer[pos++] = length_j;
                break;
            case 'K':
                word = va_arg(argptr, int);  // minimal va_arg is int: 2 bytes on 8+16 bit CPUs
                length_j = word &  0xff;
                break;
            case 'V':
                btstack_assert(length_j < 0x100);
                ptr = va_arg(argptr, uint8_t *);
                (void)memcpy(&hci_event_buffer[pos], ptr, length_j);
                pos += length_j;
                break;
            default:
                btstack_assert(false);
                break;
        }
        format++;
    };
    hci_event_buffer[1] = pos - 2;
    return pos;
}

uint16_t hci_event_create_from_template_and_arguments(uint8_t *hci_buffer, const hci_event_t *event, ...){
    va_list argptr;
    va_start(argptr, event);
    uint16_t length = hci_event_create_from_template_and_arglist(hci_buffer, event, argptr);
    va_end(argptr);
    return length;
}

/* HCI Events */

const hci_event_t hci_event_hardware_error = {
    HCI_EVENT_HARDWARE_ERROR, 0, "1"
};

const hci_event_t hci_event_transport_packet_sent = {
        HCI_EVENT_TRANSPORT_PACKET_SENT, 0, ""
};

const hci_event_t hci_event_command_complete = {
    HCI_EVENT_COMMAND_COMPLETE, 0, "121KV"
};

const hci_event_t hci_event_disconnection_complete = {
    HCI_EVENT_DISCONNECTION_COMPLETE, 0, "1H1"
};

const hci_event_t hci_event_number_of_completed_packets_1 = {
    HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS, 0, "1H2"
};

/* LE Subevents */

const hci_event_t hci_subevent_le_connection_complete = {
    HCI_EVENT_LE_META, HCI_SUBEVENT_LE_CONNECTION_COMPLETE, "1H11B2221"
};
