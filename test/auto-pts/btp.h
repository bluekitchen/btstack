/*
 * Copyright (C) 2019 BlueKitchen GmbH
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


/*
 *  btp.h
 */

#ifndef BTP_H
#define BTP_H

#include "btstack_run_loop.h"
#include "btstack_defines.h"

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

// BTP Defines
#define BTP_INDEX_NON_CONTROLLER 0xff

#define BTP_ERROR_FAIL          0x01
#define BTP_ERROR_UNKNOWN_CMD   0x02
#define BTP_ERROR_NOT_READY     0x03
#define BTP_ERROR_INVALID_INDEX 0x04

// Service IDs
#define BTP_SERVICE_ID_CORE     0
#define BTP_SERVICE_ID_GAP      1
#define BTP_SERVICE_ID_GATT     2
#define BTP_SERVICE_ID_L2CAP    3
#define BTP_SERVICE_ID_MESH     4

#define BTP_OP_ERROR                            0x00


// CORE SERVICE

#define BTP_CORE_OP_READ_SUPPORTED_COMMANDS     0x01
#define BTP_CORE_OP_READ_SUPPORTED_SERVICES     0x02
#define BTP_CORE_OP_REGISTER                    0x03
#define BTP_CORE_OP_UNREGISTER                  0x04
#define BTP_CORE_OP_LOG_MESSAGE                 0x05

#define BTP_CORE_EV_READY                       0x80
    

// GAP SERVICE

#define BTP_GAP_OP_READ_SUPPORTED_COMMANDS      0x01
#define BTP_GAP_OP_READ_CONTROLLER_INDEX_LIST   0x02

#define BTP_GAP_SETTING_POWERED                 0x00000001
#define BTP_GAP_SETTING_CONNECTABLE             0x00000002
#define BTP_GAP_SETTING_FAST_CONNECTABLE        0x00000004
#define BTP_GAP_SETTING_DISCOVERABLE            0x00000008
#define BTP_GAP_SETTING_BONDABLE                0x00000010
#define BTP_GAP_SETTING_LL_SECURITY             0x00000020
#define BTP_GAP_SETTING_SSP                     0x00000040
#define BTP_GAP_SETTING_BREDR                   0x00000080
#define BTP_GAP_SETTING_HS                      0x00000100
#define BTP_GAP_SETTING_LE                      0x00000200
#define BTP_GAP_SETTING_ADVERTISING             0x00000400
#define BTP_GAP_SETTING_SC                      0x00000800
#define BTP_GAP_SETTING_DEBUG_KEYS              0x00001000
#define BTP_GAP_SETTING_PRIVACY                 0x00002000
#define BTP_GAP_SETTING_CONTROLLER_CONF         0x00004000
#define BTP_GAP_SETTING_STATIC_ADDRESS          0x00008000

#define BTP_GAP_OP_READ_CONTROLLER_INFO         0x03
#define BTP_GAP_OP_RESET                        0x04
#define BTP_GAP_OP_SET_POWERED                  0x05
#define BTP_GAP_OP_SET_CONNECTABLE              0x06
#define BTP_GAP_OP_SET_FAST_CONNECTABLE         0x07

#define BTP_GAP_DISCOVERABLE_NON                0x00
#define BTP_GAP_DISCOVERABLE_GENERAL            0x01
#define BTP_GAP_DISCOVERABLE_LIMITED            0x02

#define BTP_GAP_OP_SET_DISCOVERABLE             0x08
#define BTP_GAP_OP_SET_BONDABLE                 0x09
#define BTP_GAP_OP_START_ADVERTISING            0x0a

#define BTP_GAP_OWN_ADDR_TYPE_IDENTITY          0x00
#define BTP_GAP_OWN_ADDR_TYPE_RPA               0x01
#define BTP_GAP_OWN_ADDR_TYPE_NON_RPA           0x02

#define BTP_GAP_OP_STOP_ADVERTISING             0x0b
#define BTP_GAP_DISCOVERY_FLAG_LE               0x01
#define BTP_GAP_DISCOVERY_FLAG_BREDR            0x02
#define BTP_GAP_DISCOVERY_FLAG_LIMITED          0x04
#define BTP_GAP_DISCOVERY_FLAG_ACTIVE           0x08
#define BTP_GAP_DISCOVERY_FLAG_OBSERVATION      0x10
#define BTP_GAP_OP_START_DISCOVERY              0x0c
#define BTP_GAP_OP_STOP_DISCOVERY               0x0d
#define BTP_GAP_ADDR_PUBLIC                     0x00
#define BTP_GAP_ADDR_RANDOM                     0x01
#define BTP_GAP_OP_CONNECT                      0x0e
#define BTP_GAP_OP_DISCONNECT                   0x0f
#define BTP_GAP_IOCAPA_DISPLAY_ONLY             0x00
#define BTP_GAP_IOCAPA_DISPLAY_YESNO            0x01
#define BTP_GAP_IOCAPA_KEYBOARD_ONLY            0x02
#define BTP_GAP_IOCAPA_NO_INPUT_NO_OUTPUT       0x03
#define BTP_GAP_IOCAPA_KEYBOARD_DISPLAY         0x04
#define BTP_GAP_OP_SET_IO_CAPA                  0x10
#define BTP_GAP_OP_PAIR                         0x11
#define BTP_GAP_OP_UNPAIR                       0x12
#define BTP_GAP_OP_PASSKEY_ENTRY_RSP            0x13
#define BTP_GAP_OP_PASSKEY_CONFIRM_RSP          0x14
#define BTP_GAP_OP_START_DIRECTED_ADVERTISING   0x15
#define BTP_GAP_OP_CONNECTION_PARAM_UPDATE      0x16
#define BTP_GAP_OP_PAIRING_CONSENT_RSP          0x17

#define BTP_GAP_EV_NEW_SETTINGS                 0x80

#define BTP_GAP_EV_DEVICE_FOUND                 0x81
#define BTP_GAP_EV_DEVICE_FOUND_FLAG_RSSI       0x01
#define BTP_GAP_EV_DEVICE_FOUND_FLAG_AD         0x02
#define BTP_GAP_EV_DEVICE_FOUND_FLAG_SR         0x04
    
#define BTP_GAP_EV_DEVICE_CONNECTED             0x82
#define BTP_GAP_EV_DEVICE_DISCONNECTED          0x83
#define BTP_GAP_EV_PASSKEY_DISPLAY              0x84
#define BTP_GAP_EV_PASSKEY_REQUEST              0x85
#define BTP_GAP_EV_PASSKEY_CONFIRM              0x86
#define BTP_GAP_EV_IDENTITY_RESOLVED            0x87


// GATT Service

#define BTP_GATT_OP_READ_SUPPORTED_COMMANDS     0x01

#define BTP_GATT_SERVICE_TYPE_PRIMARY           0x00
#define BTP_GATT_SERVICE_TYPE_SECONDARY         0x01
#define BTP_GATT_OP_ADD_SERVICE                 0x02

#define BTP_GATT_PERM_READ                      0x01
#define BTP_GATT_PERM_WRITE                     0x02
#define BTP_GATT_PERM_READ_ENC                  0x04
#define BTP_GATT_PERM_WRITE_ENC                 0x08
#define BTP_GATT_PERM_READ_AUTHN                0x10
#define BTP_GATT_PERM_WRITE_AUTHN               0x20
#define BTP_GATT_PERM_READ_AUTHZ                0x40
#define BTP_GATT_PERM_WRITE_AUTHZ               0x80

#define BTP_GATT_OP_ADD_CHARACTERISTIC          0x03
#define BTP_GATT_OP_ADD_DESCRIPTOR              0x04
#define BTP_GATT_OP_ADD_INCLUDED_SERVICE        0x05
#define BTP_GATT_OP_SET_VALUE                   0x06
#define BTP_GATT_OP_START_SERVER                0x07
#define BTP_GATT_OP_RESET_SERVER                0x08
#define BTP_GATT_OP_SET_ENC_KEY_SIZE            0x09
#define BTP_GATT_OP_EXCHANGE_MTU                0x0a
#define BTP_GATT_OP_DISC_ALL_PRIM               0x0b
#define BTP_GATT_OP_DISC_PRIM_UUID              0x0c
#define BTP_GATT_OP_FIND_INCLUDED               0x0d
#define BTP_GATT_OP_DISC_ALL_CHRC               0x0e
#define BTP_GATT_OP_DISC_CHRC_UUID              0x0f
#define BTP_GATT_OP_DISC_ALL_DESC               0x10
#define BTP_GATT_OP_READ                        0x11
#define BTP_GATT_OP_READ_UUID                   0x12
#define BTP_GATT_OP_READ_LONG                   0x13
#define BTP_GATT_OP_READ_MULTIPLE               0x14
#define BTP_GATT_OP_WRITE_WITHOUT_RSP           0x15
#define BTP_GATT_OP_SIGNED_WRITE_WITHOUT_RSP    0x16
#define BTP_GATT_OP_WRITE                       0x17
#define BTP_GATT_OP_WRITE_LONG                  0x18
#define BTP_GATT_OP_WRITE_RELIABLE              0x19
#define BTP_GATT_OP_CFG_NOTIFY                  0x1a
#define BTP_GATT_OP_CFG_INDICATE                0x1b
#define BTP_GATT_OP_GET_ATTRIBUTES              0x1c
#define BTP_GATT_OP_GET_ATTRIBUTE_VALUE         0x1d

#define BTP_GATT_EV_NOTIFICATION                0x80
#define BTP_GATT_EV_ATTR_VALUE_CHANGED          0x81


// L2CAP Service

#define BTP_L2CAP_OP_READ_SUPPORTED_COMMANDS    0x01
#define BTP_L2CAP_OP_CONNECT                    0x02
#define BTP_L2CAP_OP_DISCONNECT                 0x03
#define BTP_L2CAP_OP_SEND_DATA                  0x04
#define BTP_L2CAP_OP_TRANSPORT_BREDR            0x00
#define BTP_L2CAP_OP_TRANSPORT_LE               0x01
#define BTP_L2CAP_OP_LISTEN                     0x05
#define BTP_L2CAP_OP_ACCEPT_CONNECTION          0x06

#define BTP_L2CAP_EV_CONNECTION_REQ             0x80
#define BTP_L2CAP_EV_CONNECTED                  0x81
#define BTP_L2CAP_EV_DISCONNECTED               0x82
#define BTP_L2CAP_EV_DATA_RECEIVED              0x83


// MESH Service

#define BTP_MESH_OP_READ_SUPPORTED_COMMANDS     0x01
#define BTP_MESH_OP_CONFIG_PROVISIONING         0x02
#define BTP_MESH_OP_OUT_BLINK                   (1U <<0)
#define BTP_MESH_OP_OUT_BEEP                    (1U <<1)
#define BTP_MESH_OP_OUT_VIBRATE                 (1U <<2)
#define BTP_MESH_OP_OUT_DISPLAY_NUMBER          (1U <<3)
#define BTP_MESH_OP_OUT_DISPLAY_STRING          (1U <<4)
#define BTP_MESH_OP_IN_PUSH                     (1U <<0)
#define BTP_MESH_OP_IN_TWIST                    (1U <<1)
#define BTP_MESH_OP_IN_ENTER_NUMBER             (1U <<2)
#define BTP_MESH_OP_IN_ENTER_STRING             (1U <<3)
#define BTP_MESH_OP_PROVISION_NODE              0x03
#define BTP_MESH_OP_INIT                        0x04
#define BTP_MESH_OP_RESET                       0x05
#define BTP_MESH_OP_INPUT_NUMBER                0x06
#define BTP_MESH_OP_INPUT_STRING                0x07
#define BTP_MESH_OP_IVU_TEST_MODE               0x08
#define BTP_MESH_OP_IVU_TOGGLE_STATE            0x09
#define BTP_MESH_OP_NET_SEND                    0x0a
#define BTP_MESH_OP_HEALTH_GENERATE_FAULTS      0x0b
#define BTP_MESH_OP_HEALTH_CLEAR_FAULTS         0x0c
#define BTP_MESH_OP_LPN                         0x0d
#define BTP_MESH_OP_LPN_POLL                    0x0e
#define BTP_MESH_OP_MODEL_SEND                  0x0f
#define BTP_MESH_OP_LPN_SUBSCRIBE               0x10
#define BTP_MESH_OP_LPN_UNSUBSCRIBE             0x11
#define BTP_MESH_OP_RPL_CLEAR                   0x12
#define BTP_MESH_OP_PROXY_IDENTITY              0x13
#define BTP_MESH_EV_OUT_NUMBER_ACTION           0x80
#define BTP_MESH_EV_OUT_STRING_ACTION           0x81
#define BTP_MESH_EV_IN_ACTION                   0x82
#define BTP_MESH_EV_PROVISIONED                 0x83
#define BTP_MESH_EV_PROV_LINK_OPEN              0x84
#define BTP_MESH_EV_PROV_LINK_OPEN_BEARER_ADV   0x00
#define BTP_MESH_EV_PROV_LINK_OPEN_BEARER_GATT  0x01
#define BTP_MESH_EV_PROV_LINK_CLOSED            0x85
#define BTP_MESH_EV_NET_RECV                    0x86
#define BTP_MESH_EV_INVALID_BEARER              0x87
#define BTP_MESH_EV_INCOMP_TIMER_EXP            0x88

#endif
