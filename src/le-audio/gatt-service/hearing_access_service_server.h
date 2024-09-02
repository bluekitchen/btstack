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
 * @title Hearing Access Service Server
 * 
 */

#ifndef HEARING_ACCESS_SERVICE_SERVER_H
#define HEARING_ACCESS_SERVICE_SERVER_H

#include <stdint.h>
#include "le-audio/le_audio.h"
#include "le-audio/gatt-service/hearing_access_service_util.h"

#if defined __cplusplus
extern "C" {
#endif

typedef struct {
    hci_con_handle_t con_handle;

    btstack_context_callback_registration_t  scheduled_tasks_callback;
    bool scheduled_preset_record_change_notification;
    uint8_t scheduled_control_point_notification_tasks;

    // used for read presets
    uint8_t start_index;
    uint8_t current_position;
    uint8_t num_presets_to_read;
    uint8_t num_presets_already_read;

    uint8_t param_size;

} has_server_connection_t;


/**
 * @text The Hearing Access Service (HAS) enables a device to expose the controls and state of its audio volume.
 * 
 * To use with your application, add `#import <hearing_access_service.gatt>` to your .gatt file. 
 * After adding it to your .gatt file, you call *hearing_access_service_server_init()* 
 * 
 * HAS may include zero or more instances of VOCS and zero or more instances of AICS
 * 
 */

/* API_START */

/**
 * @brief Init Hearing Access Service Server with ATT DB
 */
void hearing_access_service_server_init(uint8_t hearing_aid_features, uint8_t preset_records_num, has_preset_record_t * preset_records, uint8_t clients_num, has_server_connection_t * clients);

/**
 * @brief Register packet handler to receive updates:
 * 
 * @param packet_handler
 */
void hearing_access_service_server_register_packet_handler(btstack_packet_handler_t packet_handler);

uint8_t hearing_access_service_server_add_preset(uint8_t properties, char * name);
uint8_t hearing_access_service_server_delete_preset(uint8_t index);

uint8_t hearing_access_service_server_preset_record_set_active(uint8_t index);
uint8_t hearing_access_service_server_preset_record_set_available(uint8_t index);
uint8_t hearing_access_service_server_preset_record_set_unavailable(uint8_t index);
uint8_t hearing_access_service_server_preset_record_set_name(uint8_t index, const char * name);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

