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
 * @title Media Control Service Server
 * 
 */

#ifndef OBJECT_TRANSFER_SERVICE_UTIL_H
#define OBJECT_TRANSFER_SERVICE_UTIL_H

#include <stdint.h>
#include "le-audio/le_audio.h"

#if defined __cplusplus
extern "C" {
#endif

#define ATT_ERROR_RESPONSE_OTS_WRITE_REQUEST_REJECTED                   0x80
#define ATT_ERROR_RESPONSE_OTS_OBJECT_NOT_SELECTED                      0x81
#define ATT_ERROR_RESPONSE_OTS_CONCURRENCY_LIMIT_EXCEEDED               0x82
#define ATT_ERROR_RESPONSE_OTS_OBJECT_NAME_ALREADY_EXISTS               0x83


// OACP (Object Action Control Point) feature masks
#define OACP_FEATURE_MASK_CREATE                                        0x0001
#define OACP_FEATURE_MASK_DELETE                                        0x0002
#define OACP_FEATURE_MASK_CALCULATE_CHECKSUM                            0x0004
#define OACP_FEATURE_MASK_EXECUTE                                       0x0008
#define OACP_FEATURE_MASK_READ                                          0x0010
#define OACP_FEATURE_MASK_WRITE                                         0x0020
#define OACP_FEATURE_MASK_APPENDING_ADDITIONAL_DATA_TO_OBJECTS          0x0040
#define OACP_FEATURE_MASK_TRUNCATION_OF_OBJECTS                         0x0080
#define OACP_FEATURE_MASK_PATCHING_OF_OBJECTS                           0x0100
#define OACP_FEATURE_MASK_ABORT                                         0x0200

// OLCP (Object List Control Point) feature masks
#define OLCP_FEATURE_MASK_GO_TO                                         0x0001
#define OLCP_FEATURE_MASK_ORDER                                         0x0002
#define OLCP_FEATURE_MASK_REQUEST_NUMBER_OF_OBJECTS                     0x0004
#define OLCP_FEATURE_MASK_CLEAR_MARKING                                 0x0008


#define OTS_MAX_NAME_LENGHT         32
/* API_START */

typedef struct {
    // metadata
    char name[OTS_MAX_NAME_LENGHT];

    uint16_t type_uuid16;
    uint8_t  type_uuid128[16];

    uint32_t allocated_size;
} ots_object_t;


/* API_END */

#if defined __cplusplus
}
#endif

#endif

