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

#ifndef GOEP_SERVER_H
#define GOEP_SERVER_H

#if defined __cplusplus
extern "C" {
#endif
 
#include "btstack_config.h"
#include <stdint.h>

/* API_START */

typedef enum {
    GOEP_SERVER_IDLE,
    GOEP_SERVER_W4_RFCOMM_CONNECTED,
    GOEP_SERVER_RFCOMM_CONNECTED
} goep_server_state_t;

typedef enum {
    GOEP_L2CAP_CONNECTION = 0,
    GOEP_RFCOMM_CONNECTION
} goep_connection_type_t;

typedef struct {
    // linked list - assert: first field
    btstack_linked_item_t    item;

    btstack_packet_handler_t callback;
    uint8_t  rfcomm_channel;
    uint16_t l2cap_psm;
} goep_server_service_t;

// type
typedef struct {
    // linked list - assert: first field
    btstack_linked_item_t    item;
    uint16_t bearer_cid;
    uint16_t goep_cid;
    goep_connection_type_t  type;
    goep_server_service_t * service;
    goep_server_state_t     state;
} goep_server_connection_t;
/* API_END */

#if defined __cplusplus
}
#endif
#endif
