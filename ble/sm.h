/*
 * Copyright (C) 2011-2012 BlueKitchen GmbH
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
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 * Please inquire about commercial licensing options at contact@bluekitchen-gmbh.com
 *
 */

#pragma once

#include <btstack/utils.h>
#include <btstack/btstack.h>
#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif


// Bluetooth Spec definitions
typedef enum {
    SM_CODE_PAIRING_REQUEST = 0X01,
    SM_CODE_PAIRING_RESPONSE,
    SM_CODE_PAIRING_CONFIRM,
    SM_CODE_PAIRING_RANDOM,
    SM_CODE_PAIRING_FAILED,
    SM_CODE_ENCRYPTION_INFORMATION,
    SM_CODE_MASTER_IDENTIFICATION,
    SM_CODE_IDENTITY_INFORMATION,
    SM_CODE_IDENTITY_ADDRESS_INFORMATION,
    SM_CODE_SIGNING_INFORMATION,
    SM_CODE_SECURITY_REQUEST
} SECURITY_MANAGER_COMMANDS;

// IO Capability Values
typedef enum {
    IO_CAPABILITY_DISPLAY_ONLY = 0,
    IO_CAPABILITY_DISPLAY_YES_NO,
    IO_CAPABILITY_KEYBOARD_ONLY,
    IO_CAPABILITY_NO_INPUT_NO_OUTPUT,
    IO_CAPABILITY_KEYBOARD_DISPLAY, // not used by secure simple pairing
    IO_CAPABILITY_UNKNOWN = 0xff
} io_capability_t;

// Authentication requirement flags
#define SM_AUTHREQ_NO_BONDING 0x00
#define SM_AUTHREQ_BONDING 0x01
#define SM_AUTHREQ_MITM_PROTECTION 0x04

// Key distribution flags used by spec
#define SM_KEYDIST_ENC_KEY 0X01
#define SM_KEYDIST_ID_KEY  0x02
#define SM_KEYDIST_SIGN    0x04

// Key distribution flags used internally
#define SM_KEYDIST_FLAG_ENCRYPTION_INFORMATION       0x01
#define SM_KEYDIST_FLAG_MASTER_IDENTIFICATION        0x02
#define SM_KEYDIST_FLAG_IDENTITY_INFORMATION         0x04
#define SM_KEYDIST_FLAG_IDENTITY_ADDRESS_INFORMATION 0x08
#define SM_KEYDIST_FLAG_SIGNING_IDENTIFICATION       0x10

// STK Generation Methods
#define SM_STK_GENERATION_METHOD_JUST_WORKS 0x01
#define SM_STK_GENERATION_METHOD_OOB        0x02
#define SM_STK_GENERATION_METHOD_PASSKEY    0x04

// Pairing Failed Reasons
#define SM_REASON_RESERVED                     0x00
#define SM_REASON_PASSKEYT_ENTRY_FAILED        0x01
#define SM_REASON_OOB_NOT_AVAILABLE            0x02
#define SM_REASON_AUTHENTHICATION_REQUIREMENTS 0x03
#define SM_REASON_CONFIRM_VALUE_FAILED         0x04
#define SM_REASON_PAIRING_NOT_SUPPORTED        0x05
#define SM_REASON_ENCRYPTION_KEY_SIZE          0x06
#define SM_REASON_COMMAND_NOT_SUPPORTED        0x07
#define SM_REASON_UNSPECIFIED_REASON           0x08
#define SM_REASON_REPEATED_ATTEMPTS            0x09
// also, invalid parameters
// and reserved

// Security Manager Events
typedef struct sm_event_bonding {
    uint8_t   type;   // see <btstack/hci_cmds.h> SM_...
    uint8_t   addr_type;
    bd_addr_t address;
    uint32_t  passkey;  // only used for SM_PASSKEY_DISPLAY_NUMBER 
} sm_event_bonding_t;

typedef struct sm_event_identity_resolving {
    uint8_t   type;   // see <btstack/hci_cmds.h> SM_IDENTITY_RESOLVING_
    uint16_t  central_device_db_index;
} sm_event_identity_resolving_t;

//
// Security Manager Client API
//

void sm_init();
void sm_set_er(sm_key_t er);
void sm_set_ir(sm_key_t ir);
void sm_register_oob_data_callback( int (*get_oob_data_callback)(uint8_t addres_type, bd_addr_t * addr, uint8_t * oob_data));
void sm_register_packet_handler(btstack_packet_handler_t handler);

void sm_set_accepted_stk_generation_methods(uint8_t accepted_stk_generation_methods);
void sm_set_encrypted_key_size_range(uint8_t min_size, uint8_t max_size);
void sm_set_authentication_requirements(uint8_t auth_req);
void sm_set_io_capabilities(io_capability_t io_capability);
void sm_set_request_security(int enable);

void sm_bonding_decline(uint8_t addr_type, bd_addr_t address);
void sm_just_works_confirm(uint8_t addr_type, bd_addr_t address);
void sm_passkey_input(uint8_t addr_type, bd_addr_t address, uint32_t passkey);

// Support for signed writes
int  sm_cmac_ready();
void sm_cmac_start(sm_key_t k, uint16_t message_len, uint8_t * message, void (*done_handler)(uint8_t hash[8]));
#if defined __cplusplus
}
#endif
