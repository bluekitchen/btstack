/*
 * Copyright (C) 2018 BlueKitchen GmbH
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

#ifndef __MESH_FOUNDATION_H
#define __MESH_FOUNDATION_H

#include <stdint.h>

#include "mesh/mesh_network.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MESH_TTL_MAX 0x7f
#define MESH_FOUNDATION_STATE_NOT_SUPPORTED 2

// Mesh Model Identifiers
#define MESH_SIG_MODEL_ID_CONFIGURATION_SERVER  0x0000u
#define MESH_SIG_MODEL_ID_CONFIGURATION_CLIENT  0x0001u
#define MESH_SIG_MODEL_ID_HEALTH_SERVER         0x0002u
#define MESH_SIG_MODEL_ID_HEALTH_CLIENT         0x0003u
#define MESH_SIG_MODEL_ID_GENERIC_ON_OFF_SERVER 0x1000u
#define MESH_SIG_MODEL_ID_GENERIC_ON_OFF_CLIENT 0x1001u

// Foundation Model Operations
#define MESH_FOUNDATION_OPERATION_APPKEY_ADD                                      0x00
#define MESH_FOUNDATION_OPERATION_APPKEY_UPDATE                                   0x01
#define MESH_FOUNDATION_OPERATION_COMPOSITION_DATA_STATUS                         0x02
#define MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_SET                           0x03
#define MESH_FOUNDATION_OPERATION_HEALTH_CURRENT_STATUS                           0x04
#define MESH_FOUNDATION_OPERATION_HEALTH_FAULT_STATUS                             0x05
#define MESH_FOUNDATION_OPERATION_HEARTBEAT_PUBLICATION_STATUS                    0x06
#define MESH_FOUNDATION_OPERATION_APPKEY_DELETE                                 0x8000
#define MESH_FOUNDATION_OPERATION_APPKEY_GET                                    0x8001
#define MESH_FOUNDATION_OPERATION_APPKEY_LIST                                   0x8002
#define MESH_FOUNDATION_OPERATION_APPKEY_STATUS                                 0x8003
#define MESH_FOUNDATION_OPERATION_HEALTH_ATTENTION_GET                          0x8004
#define MESH_FOUNDATION_OPERATION_HEALTH_ATTENTION_SET                          0x8005
#define MESH_FOUNDATION_OPERATION_HEALTH_ATTENTION_SET_UNACKNOWLEDGED           0x8006
#define MESH_FOUNDATION_OPERATION_HEALTH_ATTENTION_STATUS                       0x8007
#define MESH_FOUNDATION_OPERATION_COMPOSITION_DATA_GET                          0x8008
#define MESH_FOUNDATION_OPERATION_BEACON_GET                                    0x8009
#define MESH_FOUNDATION_OPERATION_BEACON_SET                                    0x800a
#define MESH_FOUNDATION_OPERATION_BEACON_STATUS                                 0x800b
#define MESH_FOUNDATION_OPERATION_DEFAULT_TTL_GET                               0x800c
#define MESH_FOUNDATION_OPERATION_DEFAULT_TTL_SET                               0x800d
#define MESH_FOUNDATION_OPERATION_DEFAULT_TTL_STATUS                            0x800e
#define MESH_FOUNDATION_OPERATION_FRIEND_GET                                    0x800f
#define MESH_FOUNDATION_OPERATION_FRIEND_SET                                    0x8010
#define MESH_FOUNDATION_OPERATION_FRIEND_STATUS                                 0x8011
#define MESH_FOUNDATION_OPERATION_GATT_PROXY_GET                                0x8012
#define MESH_FOUNDATION_OPERATION_GATT_PROXY_SET                                0x8013
#define MESH_FOUNDATION_OPERATION_GATT_PROXY_STATUS                             0x8014
#define MESH_FOUNDATION_OPERATION_KEY_REFRESH_PHASE_GET                         0x8015
#define MESH_FOUNDATION_OPERATION_KEY_REFRESH_PHASE_SET                         0x8016
#define MESH_FOUNDATION_OPERATION_KEY_REFRESH_PHASE_STATUS                      0x8017
#define MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_GET                         0x8018
#define MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_STATUS                      0x8019
#define MESH_FOUNDATION_OPERATION_MODEL_PUBLICATION_VIRTUAL_ADDRESS_SET         0x801a
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_ADD                        0x801b
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_DELETE                     0x801c
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_DELETE_ALL                 0x801d
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_OVERWRITE                  0x801e
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_STATUS                     0x801f
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_ADD        0x8020
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_DELETE     0x8021
#define MESH_FOUNDATION_OPERATION_MODEL_SUBSCRIPTION_VIRTUAL_ADDRESS_OVERWRITE  0x8022
#define MESH_FOUNDATION_OPERATION_NETWORK_TRANSMIT_GET                          0x8023
#define MESH_FOUNDATION_OPERATION_NETWORK_TRANSMIT_SET                          0x8024
#define MESH_FOUNDATION_OPERATION_NETWORK_TRANSMIT_STATUS                       0x8025
#define MESH_FOUNDATION_OPERATION_RELAY_GET                                     0x8026
#define MESH_FOUNDATION_OPERATION_RELAY_SET                                     0x8027
#define MESH_FOUNDATION_OPERATION_RELAY_STATUS                                  0x8028
#define MESH_FOUNDATION_OPERATION_SIG_MODEL_SUBSCRIPTION_GET                    0x8029
#define MESH_FOUNDATION_OPERATION_SIG_MODEL_SUBSCRIPTION_LIST                   0x802a
#define MESH_FOUNDATION_OPERATION_VENDOR_MODEL_SUBSCRIPTION_GET                 0x802b
#define MESH_FOUNDATION_OPERATION_VENDOR_MODEL_SUBSCRIPTION_LIST                0x802c
#define MESH_FOUNDATION_OPERATION_LOW_POWER_NODE_POLL_TIMEOUT_GET               0x802d
#define MESH_FOUNDATION_OPERATION_LOW_POWER_NODE_POLL_TIMEOUT_STATUS            0x802e
#define MESH_FOUNDATION_OPERATION_HEALTH_FAULT_CLEAR                            0x802f
#define MESH_FOUNDATION_OPERATION_HEALTH_FAULT_CLEAR_UNACKNOWLEDGED             0x8030
#define MESH_FOUNDATION_OPERATION_HEALTH_FAULT_GET                              0x8031
#define MESH_FOUNDATION_OPERATION_HEALTH_FAULT_TEST                             0x8032
#define MESH_FOUNDATION_OPERATION_HEALTH_FAULT_TEST_UNACKNOWLEDGED              0x8033
#define MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_GET                             0x8034
#define MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_SET                             0x8035
#define MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_SET_UNACKNOWLEDGED              0x8036
#define MESH_FOUNDATION_OPERATION_HEALTH_PERIOD_STATUS                          0x8037
#define MESH_FOUNDATION_OPERATION_HEARTBEAT_PUBLICATION_GET                     0x8038
#define MESH_FOUNDATION_OPERATION_HEARTBEAT_PUBLICATION_SET                     0x8039
#define MESH_FOUNDATION_OPERATION_HEARTBEAT_SUBSCRIPTION_GET                    0x803a
#define MESH_FOUNDATION_OPERATION_HEARTBEAT_SUBSCRIPTION_SET                    0x803b
#define MESH_FOUNDATION_OPERATION_HEARTBEAT_SUBSCRIPTION_STATUS                 0x803c
#define MESH_FOUNDATION_OPERATION_MODEL_APP_BIND                                0x803d
#define MESH_FOUNDATION_OPERATION_MODEL_APP_STATUS                              0x803e
#define MESH_FOUNDATION_OPERATION_MODEL_APP_UNBIND                              0x803f
#define MESH_FOUNDATION_OPERATION_NETKEY_ADD                                    0x8040
#define MESH_FOUNDATION_OPERATION_NETKEY_DELETE                                 0x8041
#define MESH_FOUNDATION_OPERATION_NETKEY_GET                                    0x8042
#define MESH_FOUNDATION_OPERATION_NETKEY_LIST                                   0x8043
#define MESH_FOUNDATION_OPERATION_NETKEY_STATUS                                 0x8044
#define MESH_FOUNDATION_OPERATION_NETKEY_UPDATE                                 0x8045
#define MESH_FOUNDATION_OPERATION_NODE_IDENTITY_GET                             0x8046
#define MESH_FOUNDATION_OPERATION_NODE_IDENTITY_SET                             0x8047
#define MESH_FOUNDATION_OPERATION_NODE_IDENTITY_STATUS                          0x8048
#define MESH_FOUNDATION_OPERATION_NODE_RESET                                    0x8049
#define MESH_FOUNDATION_OPERATION_NODE_RESET_STATUS                             0x804a
#define MESH_FOUNDATION_OPERATION_SIG_MODEL_APP_GET                             0x804b
#define MESH_FOUNDATION_OPERATION_SIG_MODEL_APP_LIST                            0x804c
#define MESH_FOUNDATION_OPERATION_VENDOR_MODEL_APP_GET                          0x804d
#define MESH_FOUNDATION_OPERATION_VENDOR_MODEL_APP_LIST                         0x804e

// Foundation Models Status Codes
#define MESH_FOUNDATION_STATUS_SUCCESS                                           0x00
#define MESH_FOUNDATION_STATUS_INVALID_ADDRESS                                   0x01
#define MESH_FOUNDATION_STATUS_INVALID_MODEL                                     0x02
#define MESH_FOUNDATION_STATUS_INVALID_APPKEY_INDEX                              0x03
#define MESH_FOUNDATION_STATUS_INVALID_NETKEY_INDEX                              0x04
#define MESH_FOUNDATION_STATUS_INSUFFICIENT_RESOURCES                            0x05
#define MESH_FOUNDATION_STATUS_KEY_INDEX_ALREADY_STORED                          0x06
#define MESH_FOUNDATION_STATUS_INVALID_PUBLISH_PARAMETER                         0x07
#define MESH_FOUNDATION_STATUS_NOT_A_SUBSCRIPTION_MODEL                          0x08
#define MESH_FOUNDATION_STATUS_STORAGE_FAILURE                                   0x09
#define MESH_FOUNDATION_STATUS_FEATURE_NOT_SUPPORTED                             0x0a
#define MESH_FOUNDATION_STATUS_CANNOT_UPDATE                                     0x0b
#define MESH_FOUNDATION_STATUS_CANNOT_REMOVE                                     0x0c
#define MESH_FOUNDATION_STATUS_CANNOT_BIND                                       0x0d
#define MESH_FOUNDATION_STATUS_TEMPORARILY_UNABLE_TO_CHANGE_STATE                0x0e
#define MESH_FOUNDATION_STATUS_CANNOT_SET                                        0x0f
#define MESH_FOUNDATION_STATUS_UNSPECIFIED_ERROR                                 0x10
#define MESH_FOUNDATION_STATUS_INVALID_BINDING                                   0x11

/**
 *
 * @param value on/off
 */
void mesh_foundation_gatt_proxy_set(uint8_t value);

/**
 *
 * @return
 */
uint8_t mesh_foundation_gatt_proxy_get(void);

/**
 *
 * @param value on/off
 */
void mesh_foundation_beacon_set(uint8_t value);

/**
 *
 * @return
 */
uint8_t mesh_foundation_beacon_get(void);

/**
 *
 * @param ttl
 */
void mesh_foundation_default_ttl_set(uint8_t ttl);

/**
 *
 * @return
 */
uint8_t mesh_foundation_default_ttl_get(void);

/**
 *
 * @param value on/off
 */
void mesh_foundation_friend_set(uint8_t value);

/**
 *
 * @return
 */
uint8_t mesh_foundation_friend_get(void);

/**
 *
 * @param value on/off
 */
void mesh_foundation_low_power_set(uint8_t value);

/**
 *
 * @return
 */
uint8_t mesh_foundation_low_power_get(void);

/**
 *
 * @param network_transmit
 */
void mesh_foundation_network_transmit_set(uint8_t network_transmit);

/**
 *
 * @return
 */
uint8_t mesh_foundation_network_transmit_get(void);
/**
 *
 * @param relay
 */
void mesh_foundation_relay_set(uint8_t relay);

/**
 *
 * @return
 */
uint8_t mesh_foundation_relay_get(void);

/**
 *
 * @param relay_retransmit
 */
void mesh_foundation_relay_retransmit_set(uint8_t relay_retransmit);

/**
 *
 * @return
 */
uint8_t mesh_foundation_relay_retransmit_get(void);

/**
 * @brief Get Features map (Relay, Proxy, Friend, Low Power)
 */
uint16_t mesh_foundation_get_features(void);

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
