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
 * @title Bond Management Service Server
 * 
 */

#ifndef BOND_MANAGEMENT_SERVICE_SERVER_H
#define BOND_MANAGEMENT_SERVICE_SERVER_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

/**
 * @text The Bond Management service server defines how a peer Bluetooth device can manage the storage of bond information, 
 * especially the deletion of it, on the Bluetooth device supporting this service.
 *
 * To use with your application, add `#import <bond_management_service.gatt>` to your .gatt file.
 * After adding it to your .gatt file, you call *bond_management_service_server_init(supported_features)*. The input parameter "supported_features" 
 * is a bitmap, which defines how the bond information will be deleted, see BMF_DELETE_* flags below.
 */

/* API_START */
#define BOND_MANAGEMENT_CONTROL_POINT_OPCODE_NOT_SUPPORTED 0x80
#define BOND_MANAGEMENT_OPERATION_FAILED 0x81

#define BMF_DELETE_ACTIVE_BOND_CLASSIC_AND_LE                   0x00001
#define BMF_DELETE_ACTIVE_BOND_CLASSIC_AND_LE_WITH_AUTH         0x00002
#define BMF_DELETE_ACTIVE_BOND_CLASSIC                          0x00004
#define BMF_DELETE_ACTIVE_BOND_CLASSIC_WITH_AUTH                0x00008
#define BMF_DELETE_ACTIVE_BOND_LE                               0x00010
#define BMF_DELETE_ACTIVE_BOND_LE_WITH_AUTH                     0x00020
#define BMF_DELETE_ALL_BONDS_CLASSIC_AND_LE                     0x00040
#define BMF_DELETE_ALL_BONDS_CLASSIC_AND_LE_WITH_AUTH           0x00080
#define BMF_DELETE_ALL_BONDS_CLASSIC                            0x00100
#define BMF_DELETE_ALL_BONDS_CLASSIC_WITH_AUTH                  0x00200
#define BMF_DELETE_ALL_BONDS_LE                                 0x00400
#define BMF_DELETE_ALL_BONDS_LE_WITH_AUTH                       0x00800
#define BMF_DELETE_ALL_BUT_ACTIVE_BOND_CLASSIC_AND_LE           0x01000
#define BMF_DELETE_ALL_BUT_ACTIVE_BOND_CLASSIC_AND_LE_WITH_AUTH 0x02000
#define BMF_DELETE_ALL_BUT_ACTIVE_BOND_CLASSIC                  0x04000
#define BMF_DELETE_ALL_BUT_ACTIVE_BOND_CLASSIC_WITH_AUTH        0x08000
#define BMF_DELETE_ALL_BUT_ACTIVE_BOND_LE                       0x10000
#define BMF_DELETE_ALL_BUT_ACTIVE_BOND_LE_WITH_AUT              0x20000

typedef enum {
    BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_CLASSIC_AND_LE = 0x01,
    BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_CLASSIC,
    BOND_MANAGEMENT_CMD_DELETE_ACTIVE_BOND_LE,
    BOND_MANAGEMENT_CMD_DELETE_ALL_BONDS_CLASSIC_AND_LE,
    BOND_MANAGEMENT_CMD_DELETE_ALL_BONDS_CLASSIC,
    BOND_MANAGEMENT_CMD_DELETE_ALL_BONDS_LE,
    BOND_MANAGEMENT_CMD_DELETE_ALL_BUT_ACTIVE_BOND_CLASSIC_AND_LE,
    BOND_MANAGEMENT_CMD_DELETE_ALL_BUT_ACTIVE_BOND_CLASSIC,
    BOND_MANAGEMENT_CMD_DELETE_ALL_BUT_ACTIVE_BOND_LE
} bond_management_cmd_t;

/**
 * @brief Init Bond Management Service with ATT DB
 * @param supported_features
 */
void bond_management_service_server_init(uint32_t supported_features);

/**
 * @brief Set authorisation string
 * @note String is not copied
 * @param authorisation_string
 */
void bond_management_service_server_set_authorisation_string(const char * authorisation_string);

/* API_END */

#if defined __cplusplus
}
#endif

#endif

