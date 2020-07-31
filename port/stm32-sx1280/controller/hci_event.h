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

/*
 *  hci_event.h
 */

#ifndef HCI_EVENT_H
#define HCI_EVENT_H

#include "bluetooth.h"

#include <stdint.h>
#include <stdarg.h>

#if defined __cplusplus
extern "C" {
#endif

/** 
 * compact HCI Event packet description
 * no subevent_code field -> subevnt_code == 0
 */
 typedef struct {
    uint8_t     event_code;
    uint8_t     subevent_code;
    const char *format;
} hci_event_t;

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
 *   J: 1-byte length of following variable-length data blob 'V', length is included in packet
 *   K: 1-byte length of following variable-length data blob 'V', length is not included in packet
 *   V: variable-length data blob of len provided in 'J' field
 */
uint16_t hci_event_create_from_template_and_arglist(uint8_t *hci_buffer, const hci_event_t *event, va_list argptr);


uint16_t hci_event_create_from_template_and_arguments(uint8_t *hci_buffer, const hci_event_t *event, ...);

const hci_event_t hci_event_hardware_error;
const hci_event_t hci_event_command_complete;
const hci_event_t hci_event_disconnection_complete;
const hci_event_t hci_event_number_of_completed_packets_1;
const hci_event_t hci_event_transport_packet_sent;

/* LE Subevents */

const hci_event_t hci_subevent_le_connection_complete;

    
#if defined __cplusplus
}
#endif

#endif // HCI_EVENT_H
