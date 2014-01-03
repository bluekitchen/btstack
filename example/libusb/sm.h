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

// BTstack Security Manager API

