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
 
#ifndef OBEX_H
#define OBEX_H

// From IR OBEX V1.5 - some opcodes have high bit always set
#define OBEX_OPCODE_CONNECT                0x80
#define OBEX_OPCODE_DISCONNECT             0x81
#define OBEX_OPCODE_PUT                    0x02
#define OBEX_OPCODE_GET                    0x03
#define OBEX_OPCODE_SETPATH                0x85
#define OBEX_OPCODE_ACTION                 0x06
#define OBEX_OPCODE_SESSION                0x87
#define OBEX_OPCODE_ABORT                  0xFF

#define OBEX_OPCODE_FINAL_BIT_MASK         0x80

#define OBEX_RESP_SUCCESS                  0xA0
#define OBEX_RESP_CONTINUE                 0x90
#define OBEX_RESP_BAD_REQUEST              0xC0
#define OBEX_RESP_UNAUTHORIZED             0xC1
#define OBEX_RESP_FORBIDDEN                0xC3
#define OBEX_RESP_NOT_FOUND                0xC4
#define OBEX_RESP_NOT_ACCEPTABLE           0xC6

#define OBEX_HEADER_BODY                           0x48
#define OBEX_HEADER_END_OF_BODY                    0x49
#define OBEX_HEADER_COUNT                          0xC0
#define OBEX_HEADER_NAME                           0x01
#define OBEX_HEADER_TYPE                           0x42
#define OBEX_HEADER_LENGTH                         0xC3
#define OBEX_HEADER_TIME_ISO_8601                  0x44
#define OBEX_HEADER_TIME_4_BYTE                    0xC4
#define OBEX_HEADER_DESCRIPTION                    0x05
#define OBEX_HEADER_TARGET                         0x46
#define OBEX_HEADER_HTTP                           0x47
#define OBEX_HEADER_WHO                            0x4A
#define OBEX_HEADER_OBJECT_CLASS                   0x4F
#define OBEX_HEADER_APPLICATION_PARAMETERS         0x4C
#define OBEX_HEADER_CONNECTION_ID                  0xCB
#define OBEX_HEADER_AUTHENTICATION_CHALLENGE       0x4D
#define OBEX_HEADER_AUTHENTICATION_RESPONSE        0x4E
#define OBEX_HEADER_SINGLE_RESPONSE_MODE           0x97
#define OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER 0x98


#define OBEX_VERSION                       0x14

#define OBEX_PACKET_HEADER_SIZE            3
#define OBEX_PACKET_OPCODE_OFFSET          0
#define OBEX_PACKET_LENGTH_OFFSET          1
#define OBEX_MAX_PACKETLEN_DEFAULT         0xffff

#define OBEX_CONNECTION_ID_INVALID         0xFFFFFFFF

/* SRM header values */
#define OBEX_SRM_DISABLE                            0x00
#define OBEX_SRM_ENABLE                             0x01
#define OBEX_SRM_INDICATE                           0x02

/* SRMP header values */
#define OBEX_SRMP_NEXT                              0x00
#define OBEX_SRMP_WAIT                              0x01
#define OBEX_SRMP_NEXT_WAIT                         0x02


/**
 * PBAP
 */

// PBAP Application Parameters Tag IDs

// Order - 0x01 - 1 byte: 0x00 = indexed 0x01 = alphanumeric 0x02 = phonetic
#define PBAP_APPLICATION_PARAMETER_ORDER 0x01
// SearchValue - 0x02 - variable - Text
#define PBAP_APPLICATION_PARAMETER_SEARCH_VALUE 0x02
// SearchProperty - 0x03 - 1 byte - 0x00= Name 0x01= Number 0x02= Sound
#define PBAP_APPLICATION_PARAMETER_SEARCH_PROPERTY 0x03
// MaxListCount - 0x04 - 2 bytes - 0x0000 to 0xFFFF
#define PBAP_APPLICATION_PARAMETER_MAX_LIST_COUNT 0x04
// ListStartOffset - 0x05 - 2 bytes - 0x0000 to 0xFFFF
#define PBAP_APPLICATION_PARAMETER_LIST_START_OFFSET 0x05
// PropertySelector - 0x06 - 8 bytes - 64 bits mask
#define PBAP_APPLICATION_PARAMETER_PROPERTY_SELECTOR 0x06
// Format - 0x07 - 1 byte - 0x00 = 2.1 0x01 = 3.0
#define PBAP_APPLICATION_PARAMETER_FORMAT 0x07
// PhonebookSize - 0x08 - 2 bytes - 0x0000 to 0xFFFF
#define PBAP_APPLICATION_PARAMETER_PHONEBOOK_SIZE 0x08
// NewMissedCalls - 0x09 - 1 byte - 0x00 to 0xFF
#define PBAP_APPLICATION_PARAMETER_NEW_MISSED_CALLS 0x09
// PrimaryVersionCounter - 0x0A - 16 bytes - 0 to (2128 – 1)
#define PBAP_APPLICATION_PARAMETER_PRIMARY_VERSION_COUNTER 0x0A
// SecondaryVersionCounter - 0x0B - 16 bytes - 0 to (2128 – 1)
#define PBAP_APPLICATION_PARAMETER_SECONDARY_VERSION_COUNTER 0x0B
// vCardSelector - 0x0C - 8 bytes - 64 bits mask
#define PBAP_APPLICATION_PARAMETER_VCARD_SELECTOR 0x0C
// DatabaseIdentifier - 0x0D - 16 bytes - 0 to (2128 – 1)
#define PBAP_APPLICATION_PARAMETER_DATABASE_IDENTIFIER 0x0D
// vCardSelectorOperator - 0x0E - 1 byte - 0x00 = OR 0x01 = AND
#define PBAP_APPLICATION_PARAMETER_VCARD_SELECTOR_OPERATOR 0x0E
// ResetNewMissedCalls -   0x0F -  1 byte
#define PBAP_APPLICATION_PARAMETER_RESET_NEW_MISSED_CALLS 0x0F
// PbapSupportedFeatures - 0x10 - 4 bytes
#define PBAP_APPLICATION_PARAMETER_PBAP_SUPPORTED_FEATURES 0x10

#endif
