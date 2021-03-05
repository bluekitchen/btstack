/*
 * Copyright (C) 2021 BlueKitchen GmbH
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

#ifndef DEVICE_INFORMATION_SERVICE_CLIENT_H
#define DEVICE_INFORMATION_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "btstack_linked_list.h"
#include "ble/gatt_client.h"


#if defined __cplusplus
extern "C" {
#endif

typedef enum {
    DEVICE_INFORMATION_SERVICE_CLIENT_STATE_IDLE,
    DEVICE_INFORMATION_SERVICE_CLIENT_STATE_W2_QUERY_SERVICE,
    DEVICE_INFORMATION_SERVICE_CLIENT_STATE_W4_SERVICE_RESULT,
    DEVICE_INFORMATION_SERVICE_CLIENT_STATE_CONNECTED
} device_information_service_client_state_t;


typedef struct {
    // service
    uint16_t start_handle;
    uint16_t end_handle;
} device_information_service_t;


typedef struct {
    btstack_linked_item_t item;
    
    hci_con_handle_t  con_handle;
    device_information_service_client_state_t  state;
    btstack_packet_handler_t client_handler;
} device_information_service_client_t;

/* API_START */

/**
 * @brief Initialize Device Information Service. 
 */
void device_information_service_client_init(void);

/**
 * @brief Query Device Information Service. 
 * @param con_handle
 * @param packet_handler
 */
uint8_t device_information_service_client_query(hci_con_handle_t con_handle, btstack_packet_handler_t packet_handler);


/**
 * @brief De-initialize Device Information Service. 
 */
void device_information_service_client_deinit(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif
