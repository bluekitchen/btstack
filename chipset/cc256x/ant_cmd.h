/*
 * This software is subject to the ANT+ Shared Source License
 * www.thisisant.com/developer/ant/licensing
 * 
 * Copyright © Dynastream Innovations,
 * Inc. 2012 All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer. Redistributions in binary
 * form must reproduce the above copyright notice, this list of conditions and
 * the following disclaimer in the documentation and/or other materials provided
 * with the distribution. Neither the name of Dynastream nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission. The following actions are
 * prohibited:
 * 
 * Redistribution of source code containing the ANT+ Network Key. The ANT+
 * Network Key is available to ANT+ Adopters. Please refer to
 * http://thisisant.com to become an ANT+ Adopter and access the key. Reverse
 * engineering, decompilation, and/or disassembly of software provided in binary
 * form under this license. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef ANT_CMDS_H
#define ANT_CMDS_H

#include <stdint.h>
#include "hci_cmd.h"

#if defined __cplusplus
extern "C" {
#endif

typedef uint8_t UCHAR;

//////////////////////////////////////////////
// Message IDs from antmessage.h
//////////////////////////////////////////////
#define MESG_INVALID_ID                      ((UCHAR)0x00)
#define MESG_EVENT_ID                        ((UCHAR)0x01)

#define MESG_VERSION_ID                      ((UCHAR)0x3E)
#define MESG_RESPONSE_EVENT_ID               ((UCHAR)0x40)

#define MESG_UNASSIGN_CHANNEL_ID             ((UCHAR)0x41)
#define MESG_ASSIGN_CHANNEL_ID               ((UCHAR)0x42)
#define MESG_CHANNEL_MESG_PERIOD_ID          ((UCHAR)0x43)
#define MESG_CHANNEL_SEARCH_TIMEOUT_ID       ((UCHAR)0x44)
#define MESG_CHANNEL_RADIO_FREQ_ID           ((UCHAR)0x45)
#define MESG_NETWORK_KEY_ID                  ((UCHAR)0x46)
#define MESG_RADIO_TX_POWER_ID               ((UCHAR)0x47)
#define MESG_RADIO_CW_MODE_ID                ((UCHAR)0x48)
#define MESG_SYSTEM_RESET_ID                 ((UCHAR)0x4A)
#define MESG_OPEN_CHANNEL_ID                 ((UCHAR)0x4B)
#define MESG_CLOSE_CHANNEL_ID                ((UCHAR)0x4C)
#define MESG_REQUEST_ID                      ((UCHAR)0x4D)

#define MESG_BROADCAST_DATA_ID               ((UCHAR)0x4E)
#define MESG_ACKNOWLEDGED_DATA_ID            ((UCHAR)0x4F)
#define MESG_BURST_DATA_ID                   ((UCHAR)0x50)

#define MESG_CHANNEL_ID_ID                   ((UCHAR)0x51)
#define MESG_CHANNEL_STATUS_ID               ((UCHAR)0x52)
#define MESG_RADIO_CW_INIT_ID                ((UCHAR)0x53)
#define MESG_CAPABILITIES_ID                 ((UCHAR)0x54)

#define MESG_STACKLIMIT_ID                   ((UCHAR)0x55)

#define MESG_SCRIPT_DATA_ID                  ((UCHAR)0x56)
#define MESG_SCRIPT_CMD_ID                   ((UCHAR)0x57)

#define MESG_ID_LIST_ADD_ID                  ((UCHAR)0x59)
#define MESG_ID_LIST_CONFIG_ID               ((UCHAR)0x5A)
#define MESG_OPEN_RX_SCAN_ID                 ((UCHAR)0x5B)

#define MESG_EXT_CHANNEL_RADIO_FREQ_ID       ((UCHAR)0x5C)  // OBSOLETE: (for 905 radio)
#define MESG_EXT_BROADCAST_DATA_ID           ((UCHAR)0x5D)
#define MESG_EXT_ACKNOWLEDGED_DATA_ID        ((UCHAR)0x5E)
#define MESG_EXT_BURST_DATA_ID               ((UCHAR)0x5F)

#define MESG_CHANNEL_RADIO_TX_POWER_ID       ((UCHAR)0x60)
#define MESG_GET_SERIAL_NUM_ID               ((UCHAR)0x61)
#define MESG_GET_TEMP_CAL_ID                 ((UCHAR)0x62)
#define MESG_SET_LP_SEARCH_TIMEOUT_ID        ((UCHAR)0x63)
#define MESG_SET_TX_SEARCH_ON_NEXT_ID        ((UCHAR)0x64)
#define MESG_SERIAL_NUM_SET_CHANNEL_ID_ID    ((UCHAR)0x65)
#define MESG_RX_EXT_MESGS_ENABLE_ID          ((UCHAR)0x66)  
#define MESG_RADIO_CONFIG_ALWAYS_ID          ((UCHAR)0x67)
#define MESG_ENABLE_LED_FLASH_ID             ((UCHAR)0x68)

#define MESG_XTAL_ENABLE_ID                  ((UCHAR)0x6D)

#define MESG_STARTUP_MESG_ID                 ((UCHAR)0x6F)
#define MESG_AUTO_FREQ_CONFIG_ID             ((UCHAR)0x70)
#define MESG_PROX_SEARCH_CONFIG_ID           ((UCHAR)0x71)

#define MESG_SET_SEARCH_CH_PRIORITY_ID       ((UCHAR)0x75)

#define MESG_CUBE_CMD_ID                     ((UCHAR)0x80)

#define MESG_GET_PIN_DIODE_CONTROL_ID        ((UCHAR)0x8D)
#define MESG_PIN_DIODE_CONTROL_ID            ((UCHAR)0x8E)
#define MESG_FIT1_SET_AGC_ID                 ((UCHAR)0x8F)

#define MESG_FIT1_SET_EQUIP_STATE_ID         ((UCHAR)0x91)  // *** CONFLICT: w/ Sensrcore, Fit1 will never have sensrcore enabled

// Sensrcore Messages
#define MESG_SET_CHANNEL_INPUT_MASK_ID       ((UCHAR)0x90)
#define MESG_SET_CHANNEL_DATA_TYPE_ID        ((UCHAR)0x91)
#define MESG_READ_PINS_FOR_SECT_ID           ((UCHAR)0x92)
#define MESG_TIMER_SELECT_ID                 ((UCHAR)0x93)
#define MESG_ATOD_SETTINGS_ID                ((UCHAR)0x94)
#define MESG_SET_SHARED_ADDRESS_ID           ((UCHAR)0x95)
#define MESG_ATOD_EXTERNAL_ENABLE_ID         ((UCHAR)0x96)
#define MESG_ATOD_PIN_SETUP_ID               ((UCHAR)0x97)
#define MESG_SETUP_ALARM_ID                  ((UCHAR)0x98)
#define MESG_ALARM_VARIABLE_MODIFY_TEST_ID   ((UCHAR)0x99)
#define MESG_PARTIAL_RESET_ID                ((UCHAR)0x9A)
#define MESG_OVERWRITE_TEMP_CAL_ID           ((UCHAR)0x9B)
#define MESG_SERIAL_PASSTHRU_SETTINGS_ID     ((UCHAR)0x9C)

#define MESG_BIST_ID                         ((UCHAR)0xAA)
#define MESG_UNLOCK_INTERFACE_ID             ((UCHAR)0xAD)
#define MESG_SERIAL_ERROR_ID                 ((UCHAR)0xAE)
#define MESG_SET_ID_STRING_ID                ((UCHAR)0xAF)

#define MESG_PORT_GET_IO_STATE_ID            ((UCHAR)0xB4)
#define MESG_PORT_SET_IO_STATE_ID            ((UCHAR)0xB5)

#define MESG_SLEEP_ID                        ((UCHAR)0xC5)
#define MESG_GET_GRMN_ESN_ID                 ((UCHAR)0xC6)
#define MESG_SET_USB_INFO_ID                 ((UCHAR)0xC7)
	
// ANT HCI Commands - see ant_cmds.c for info on parameters
extern const hci_cmd_t btstack_get_state;

/** 
 * compact ANT HCI Command packet description
 */
 typedef struct {
    const uint8_t    message_id;
    const char *format;
} ant_cmd_t;

uint16_t ant_create_cmd(uint8_t *hci_cmd_buffer, const ant_cmd_t *cmd, ...);
int ant_send_cmd(const ant_cmd_t *cmd, ...);

const ant_cmd_t ant_reset;
const ant_cmd_t ant_assign_channel;
const ant_cmd_t ant_un_assign_channel;
const ant_cmd_t ant_search_timeout;
const ant_cmd_t ant_lp_search_timeout;
const ant_cmd_t ant_network_key;
const ant_cmd_t ant_channel_id;
const ant_cmd_t ant_channel_power;
const ant_cmd_t ant_channel_period;
const ant_cmd_t ant_prox_search_config;
const ant_cmd_t ant_broadcast;
const ant_cmd_t ant_acknowledged;
const ant_cmd_t ant_burst_packet;
const ant_cmd_t ant_open_channel;
const ant_cmd_t ant_close_channel;
const ant_cmd_t ant_request_message;

#if defined __cplusplus
}
#endif

#endif // ANT_CMDS_H
