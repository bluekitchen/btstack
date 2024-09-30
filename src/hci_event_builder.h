/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

/**
 *  @brief Builder for HCI Events
 */

#ifndef HCI_EVENT_BUILDER_H
#define HCI_EVENT_BUILDER_H

#include <stdint.h>

#include "bluetooth.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

typedef struct {
    uint8_t * buffer;
    uint16_t size;
    uint16_t pos;
} hci_event_builder_context_t;

/**
 * @brief Initialize HCI Event Builder with buffer, event and subevent type
 *
 * @param context
 * @param buffer
 * @param size
 * @param event_type
 * @param subevent_type, use 0 for regular events
 */
void hci_event_builder_init(hci_event_builder_context_t * context, uint8_t * buffer, uint16_t size, uint8_t event_type, uint8_t subevent_type);

/**
 * @brief Query remaining space in event buffer
 *
 * @param context
 * @return number of bytes that can be added
 */
uint16_t hci_event_builder_remaining_space(hci_event_builder_context_t * context);

/**
 * @brief Get constructed event length
 * @param context
 * @return number of bytes in event
 */
uint16_t hci_event_builder_get_length(hci_event_builder_context_t * context);

/**
 * @bbrief Add uint8_t to event
 * @param context
 * @param value
 */
void hci_event_builder_add_08(hci_event_builder_context_t * context, uint8_t value);

/**
 * @bbrief Add uint16_t value to event
 * @param context
 * @param value
 */
void hci_event_builder_add_16(hci_event_builder_context_t * context, uint16_t value);

/**
 * @bbrief Add 24 bit from uint32_t byte to event
 * @param context
 * @param value
 */
void hci_event_builder_add_24(hci_event_builder_context_t * context, uint32_t value);

/**
 * @bbrief Add uint32_t to event
 * @param context
 * @param value
 */
void hci_event_builder_add_32(hci_event_builder_context_t * context, uint32_t value);

/**
 * @bbrief Add uint8_t[8] in big_endian to event
 * @param context
 * @param value
 */
void hci_event_builder_add_64(hci_event_builder_context_t * context, const uint8_t * value);

/**
 * @bbrief Add uint8_t[16] in big_endian to event
 * @param context
 * @param value
 */
void hci_event_builder_add_128(hci_event_builder_context_t * context, const uint8_t * value);

/**
 * @bbrief Add Bluetooth address to event
 * @param context
 * @param value
 */
void hci_event_builder_add_bd_addr(hci_event_builder_context_t * context, bd_addr_t addr);

/**
 * @bbrief Add HCI Connection Handle to event
 * @param context
 * @param value
 */
void hci_event_builder_add_con_handle(hci_event_builder_context_t * context, hci_con_handle_t con_handle);

/**
 * @bbrief Add string to event using JV format: len, string, trailing zero
 * @param context
 * @param value
 */
void hci_event_builder_add_string(hci_event_builder_context_t * context, const char * text);

/**
 * @bbrief Add byte sequence to event
 * @param context
 * @param value
 */
void hci_event_builder_add_bytes(hci_event_builder_context_t * context, const uint8_t * data, uint16_t length);

/* API_END */

#if defined __cplusplus
}
#endif
#endif // HCI_EVENT_BUILDER_H
