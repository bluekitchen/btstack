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
 *  ll.h
 */

#ifndef LL_H
#define LL_H

#include "btstack_bool.h"
#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

// num tx buffers for use by host stack
#define HCI_NUM_TX_BUFFERS_STACK                    16

/* API_START */

void ll_init(void);

void ll_execute_once(void);

void ll_radio_on(void);

void ll_set_scan_parameters(uint8_t le_scan_type, uint16_t le_scan_interval, uint16_t le_scan_window,
        uint8_t own_address_type, uint8_t scanning_filter_policy);

uint8_t ll_set_scan_enable(uint8_t le_scan_enable, uint8_t filter_duplicates);

uint8_t ll_set_advertise_enable(uint8_t le_adv_enable);

uint8_t ll_set_advertising_parameters(uint16_t advertising_interval_min, uint16_t advertising_interval_max,
        uint8_t advertising_type, uint8_t own_address_type, uint8_t peer_address_types, uint8_t * peer_address,
        uint8_t advertising_channel_map, uint8_t advertising_filter_policy);

uint8_t ll_set_advertising_data(uint8_t adv_len, const uint8_t * adv_data);

uint8_t ll_set_scan_response_data(uint8_t adv_len, const uint8_t * adv_data);

bool ll_reserve_acl_packet(void);


void ll_queue_acl_packet(const uint8_t * packet, uint16_t size);

void ll_register_packet_handler(void (*packet_handler)(uint8_t packet_type, uint8_t * packet, uint16_t size));

/* API_END */

#if defined __cplusplus
}
#endif

#endif // LL_H
