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
 * @title Link Loss Service Server
 * 
 */

#ifndef LINK_LOSS_SERVICE_SERVER_H
#define LINK_LOSS_SERVICE_SERVER_H

#include <stdint.h>
#include "ble/gatt-service/link_loss_service_util.h"

#if defined __cplusplus
extern "C" {
#endif


/**
 * @ To use with your application, add `#import <link_loss_service.gatt>` to your .gatt file. 
 */

/* API_START */

/**
 * @brief Init Link Loss Service Server with ATT DB
 * @param mute_state
 */
void link_loss_service_server_init(void);

/**
 * @brief Register packet handler
 */
void link_loss_service_server_register_packet_handler(btstack_packet_handler_t packet_handler);

/**
 * @brief Set alert level. Default is no alert.
 * @param alert_level
 */
uint8_t link_loss_service_client_set_alert_level(lls_alert_level_t alert_level);

/**
 * @brief Set alert timeout. Default is 0 that equals to no timeout.
 * @param alert_timeout_ms
 */
uint8_t link_loss_service_server_set_alert_timeout(uint32_t alert_timeout_ms);


void link_loss_service_server_stop_alerting(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

