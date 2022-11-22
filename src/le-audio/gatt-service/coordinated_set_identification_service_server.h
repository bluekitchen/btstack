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

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @text This service specifies how devices can be identified and treated as part of a Coordinated Set.
 * 
 * To use with your application, add `#import <coordinated_set_identification_service.gatt>` to your .gatt file. 
 */

typedef enum {
    CSIS_SIRK_TYPE_ENCRYPTED = 0,
    CSIS_SIRK_TYPE_PLAIN_TEXT,
} csis_sirk_type_t;

typedef struct {
    uint16_t con_handle;

} csis_remote_client_t;
/**
 * @brief Init Coordinated Set Identification Service Server with ATT DB
 */
void coordinated_set_identification_service_server_init(void);

/**
 * @brief Register callback.
 * @param callback
 */
void coordinated_set_identification_service_server_register_packet_handler(btstack_packet_handler_t callback);


void coordinated_set_identification_service_server_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

