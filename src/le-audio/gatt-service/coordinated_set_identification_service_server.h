/*
 * Copyright (C) 2022 BlueKitchen GmbH
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
 * @title Coordinated Set Identification Service
 * 
 */

#ifndef COORDINATED_SET_IDENTIFICATION_SERVICE_SERVER_H
#define COORDINATED_SET_IDENTIFICATION_SERVICE_SERVER_H

#include <stdint.h>
#include "le-audio/le_audio.h"
#include "le-audio/gatt-service/coordinated_set_identification_service_util.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @text This service specifies how devices can be identified and treated as part of a Coordinated Set.
 * 
 * To use with your application, add `#import <coordinated_set_identification_service.gatt>` to your .gatt file. 
 */
// Coordinator Struct
typedef struct {
    uint16_t con_handle;

    uint16_t sirk_configuration;
    uint16_t member_lock_configuration;
    uint16_t coordinated_set_size_configuration;

    bool     is_lock_owner;
    btstack_timer_source_t lock_timer;

    uint8_t  scheduled_tasks;
    btstack_context_callback_registration_t  scheduled_tasks_callback;

    csis_sirk_calculation_state_t  encrypted_sirk_state;
    uint8_t encrypted_sirk[16];
} coordinated_set_identification_service_server_t;

/**
 * @brief Init Coordinated Set Identification Service Server with ATT DB
 */
void coordinated_set_identification_service_server_init(const uint8_t coordinators_num, coordinated_set_identification_service_server_t * coordinators,
                                                        uint8_t coordinated_set_size, uint8_t member_rank);

/**
 * @brief Register packet handler to receive events:
 * - GATTSERVICE_SUBEVENT_CSIS_COORDINATOR_DISCONNECTED
 * - GATTSERVICE_SUBEVENT_CSIS_COORDINATOR_CONNECTED
 * - GATTSERVICE_SUBEVENT_CSIS_COORDINATED_SET_MEMBER_LOCK
 * - GATTSERVICE_SUBEVENT_CSIS_COORDINATED_SET_SIZE
 * - GATTSERVICE_SUBEVENT_CSIS_RSI
 * @param packet_handler
 */
void coordinated_set_identification_service_server_register_packet_handler(btstack_packet_handler_t packet_handler);

/**
 * @brief Set SIRK. The SIRK associates a device with a Coordinated Set. 
 * @param sirk_type
 * @param sirk
 * @param exposed_via_oob
 */
void coordinated_set_identification_service_server_set_sirk(csis_sirk_type_t sirk_type, const uint8_t * sirk, bool exposed_via_oob);

/**
 * @brief Set the number of devices in the Coordinated Set to whom this server is part of. A size of zero is not allowed.
 * @param coordinated_set_size
 */
void coordinated_set_identification_service_server_set_size(uint8_t coordinated_set_size);

/**
 * @brief Set the order of the server in which the client should access it when performing procedures with the Coordinated Set.
 * The rank of the server should be greater then zero and unique within a Coordinated Set. 
 * @param coordinated_set_member_rank
 */
void coordinated_set_identification_service_server_set_rank(uint8_t coordinated_set_member_rank);

/**
 * @brief Calculate RSI based on SIRK and a random number. The result is reported via the GATTSERVICE_SUBEVENT_CSIS_RSI event.
 * @return ERROR_CODE_SUCCESS if successful, otherwise
 *                - ERROR_CODE_COMMAND_DISALLOWED if there is an ongoing RSI calculation
 */ 
uint8_t coordinated_set_identification_service_server_generate_rsi(void);

/**
 * @brief De-init CSIS server.
 */
void coordinated_set_identification_service_server_deinit(void);

/* API_END */

// PTS test only
uint8_t coordinated_set_identification_service_server_simulate_member_connected(hci_con_handle_t con_handle);
uint8_t coordinated_set_identification_service_server_simulate_set_lock(hci_con_handle_t con_handle, csis_member_lock_t coordinated_set_member_lock);

#if defined __cplusplus
}
#endif

#endif

