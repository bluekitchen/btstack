/*
 * Copyright (C) 2014 BlueKitchen GmbH
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
 * @title TX Power Service Server
 * 
 */

#ifndef TX_POWER_SERVICE_SERVER_H
#define TX_POWER_SERVICE_SERVER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/**
 * @text The TX Power service exposes a device’s current transmit power level when in a connection. 
 * There shall only be one instance of the Tx Power service on a device.
 * 
 * To use with your application, add `#import <tx_power_service.gatt>` to your .gatt file. 
 * After adding it to your .gatt file, you call *tx_power_service_server_init(value)* with the
 * device’s current transmit power level value. 
 *
 * If the power level value changes, you can call *tx_power_service_server_set_level(tx_power_level_dBm)*. 
 * The service does not support sending Notifications.
 */

/* API_START */

/**
 * @brief Init TX Power Service Server with ATT DB
 * @param tx_power_level
 */
void tx_power_service_server_init(int8_t tx_power_level);

/**
 * @brief Update TX power level
 * @param tx_power_level_dBm range [-100,20]
 */
void tx_power_service_server_set_level(int8_t tx_power_level_dBm);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

