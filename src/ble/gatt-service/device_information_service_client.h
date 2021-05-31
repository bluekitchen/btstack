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

/**
 * @title Device Information Service Client
 * 
 */

#ifndef DEVICE_INFORMATION_SERVICE_CLIENT_H
#define DEVICE_INFORMATION_SERVICE_CLIENT_H

#include <stdint.h>
#include "btstack_defines.h"
#include "bluetooth.h"
#include "ble/gatt_client.h"

#if defined __cplusplus
extern "C" {
#endif

/** 
 * @text The Device Information Service Client retrieves the following information from a remote device:
 * - manufacturer name
 * - model number     
 * - serial number    
 * - hardware revision
 * - firmware revision
 * - software revision
 * - system ID        
 * - IEEE regulatory certification
 * - PNP ID  
 */

/* API_START */

/**
 * @brief Initialize Device Information Service. 
 */
void device_information_service_client_init(void);

/**
 * @brief Query Device Information Service. The client will query the remote service and emit events: 
 *
 * - GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_MANUFACTURER_NAME
 * - GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_MODEL_NUMBER     
 * - GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_SERIAL_NUMBER    
 * - GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_HARDWARE_REVISION
 * - GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_FIRMWARE_REVISION
 * - GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_SOFTWARE_REVISION
 * - GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_SYSTEM_ID        
 * - GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_IEEE_REGULATORY_CERTIFICATION
 * - GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_PNP_ID  
 *
 * Event GATTSERVICE_SUBEVENT_DEVICE_INFORMATION_DONE is received when all queries are done, of if service was not found.
 * The status field of this event indicated ATT errors (see bluetooth.h). 
 *
 * @param con_handle
 * @param packet_handler
 * @return status ERROR_CODE_SUCCESS on success, otherwise GATT_CLIENT_IN_WRONG_STATE if query is already in progress
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
