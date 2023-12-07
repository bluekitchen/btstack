/*
 * Copyright (C) 2017 BlueKitchen GmbH
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


#ifndef ADV_BEARER_H
#define ADV_BEARER_H

#include <stdint.h>

#include "btstack_defines.h"
#include "bluetooth.h"

#if defined __cplusplus
extern "C" {
#endif

typedef struct {
	void * next;
	uint8_t adv_length;
	uint8_t adv_data[31];
} adv_bearer_connectable_advertisement_data_item_t;

/**
 * Initialize Advertising Bearer
 */
void adv_bearer_init(void);

//
// Mirror gap.h advertisement API for use with ADV Bearer
//
// Advertisements are interleaved with ADV Bearer Messages

/**
 * Add Connectable Advertisement Data Item
 * @param item
 * @note item is not copied, pointer has to stay valid
 * @note '00:00:00:00:00:00' in advertising_data will be replaced with actual bd addr
 */
void adv_bearer_advertisements_add_item(adv_bearer_connectable_advertisement_data_item_t * item);

/**
 * Remove Connectable Advertisement Data Item
 * @param item
 * @note item is not copied, pointer has to stay valid
 * @note '00:00:00:00:00:00' in advertising_data will be replaced with actual bd addr
 */
void adv_bearer_advertisements_remove_item(adv_bearer_connectable_advertisement_data_item_t * item);

/**
 * @brief Set Advertisement Paramters
 * @param adv_int_min
 * @param adv_int_max
 * @param adv_type
 * @param direct_address_type
 * @param direct_address
 * @param channel_map
 * @param filter_policy
 * @note own_address_type is used from gap_random_address_set_mode
 */
void adv_bearer_advertisements_set_params(uint16_t adv_int_min, uint16_t adv_int_max, uint8_t adv_type,
	uint8_t direct_address_typ, bd_addr_t direct_address, uint8_t channel_map, uint8_t filter_policy);

/** 
 * @brief Enable/Disable Advertisements. OFF by default.
 * @param enabled
 */
void adv_bearer_advertisements_enable(int enabled);

/**
 * Register listener for particular message types: Mesh Message, Mesh Beacon, PB-ADV
 */
void adv_bearer_register_for_network_pdu(btstack_packet_handler_t packet_handler);
void adv_bearer_register_for_beacon(btstack_packet_handler_t packet_handler);
void adv_bearer_register_for_provisioning_pdu(btstack_packet_handler_t packet_handler);

/**
 * Request can send now event for particular message type: Mesh Message, Mesh Beacon, PB-ADV
 */
void adv_bearer_request_can_send_now_for_network_pdu(void);
void adv_bearer_request_can_send_now_for_beacon(void);
void adv_bearer_request_can_send_now_for_provisioning_pdu(void);

/**
 * Send Mesh Message
 * @param data to send
 * @param data_len max 29 bytes
 * @param count number of transmissions
 * @param interval between transmission
 */
void adv_bearer_send_network_pdu(const uint8_t * network_pdu, uint16_t size, uint8_t count, uint16_t interval);

/**
 * Send particular message type:Mesh Beacon, PB-ADV
 * @param data to send
 * @param data_len max 29 bytes
 */
void adv_bearer_send_beacon(const uint8_t * beacon_update, uint16_t size);
void adv_bearer_send_provisioning_pdu(const uint8_t * pb_adv_pdu, uint16_t size); 
 

#if defined __cplusplus
}
#endif

#endif // __ADV_BEARER_H
