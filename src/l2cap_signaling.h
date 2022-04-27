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
 * l2cap_signaling.h
 *
 */

#ifndef L2CAP_SIGNALING_H
#define L2CAP_SIGNALING_H

#include <stdint.h>
#include <stdarg.h>
#include "btstack_util.h"
#include "hci_cmd.h"

#if defined __cplusplus
extern "C" {
#endif

typedef enum {
    COMMAND_REJECT = 1,
    CONNECTION_REQUEST,
    CONNECTION_RESPONSE,
    CONFIGURE_REQUEST,
    CONFIGURE_RESPONSE,
    DISCONNECTION_REQUEST,
    DISCONNECTION_RESPONSE,
    ECHO_REQUEST,
    ECHO_RESPONSE,
    INFORMATION_REQUEST,
    INFORMATION_RESPONSE,
    /* 0x0c - 0x11 used for AMP */
    CONNECTION_PARAMETER_UPDATE_REQUEST = 0x12,
    CONNECTION_PARAMETER_UPDATE_RESPONSE,
    LE_CREDIT_BASED_CONNECTION_REQUEST,
    LE_CREDIT_BASED_CONNECTION_RESPONSE,
    L2CAP_FLOW_CONTROL_CREDIT_INDICATION,
    L2CAP_CREDIT_BASED_CONNECTION_REQUEST,
    L2CAP_CREDIT_BASED_CONNECTION_RESPONSE,
    L2CAP_CREDIT_BASED_RECONFIGURE_REQUEST,
    L2CAP_CREDIT_BASED_RECONFIGURE_RESPONSE,
#ifdef UNIT_TEST
    COMMAND_WITH_INVALID_FORMAT,
#endif
    // internal to BTstack
    SM_PAIRING_FAILED = 0x1E,
    COMMAND_REJECT_LE = 0x1F
} L2CAP_SIGNALING_COMMANDS;

typedef enum {
    L2CAP_CHANNEL_MODE_BASIC                   = 0,
    L2CAP_CHANNEL_MODE_RETRANSMISSION          = 1,
    L2CAP_CHANNEL_MODE_FLOW_CONTROL            = 2,
    L2CAP_CHANNEL_MODE_ENHANCED_RETRANSMISSION = 3,
    L2CAP_CHANNEL_MODE_STREAMING_MODE          = 4,
} l2cap_channel_mode_t;

/**
 * @brief Create L2CAP signaling packet based on template and va_args
 * @param acl_buffer to create packet
 * @param handle
 * @param pb_flags
 * @param cid
 * @param cmd
 * @param identifier
 * @param argptr
 * @return
 */
uint16_t l2cap_create_signaling_packet(uint8_t * acl_buffer, hci_con_handle_t handle, uint8_t pb_flags, uint16_t cid, L2CAP_SIGNALING_COMMANDS cmd, uint8_t identifier, va_list argptr);

#if defined __cplusplus
}
#endif

#endif // L2CAP_SIGNALING_H
