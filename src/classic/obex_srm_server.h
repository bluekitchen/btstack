/*
 * Copyright (C) 2023 BlueKitchen GmbH
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

#ifndef OBEX_SRM_H
#define OBEX_SRM_H

#include <stdint.h>

#if defined __cplusplus
extern "C" {
#endif

typedef enum {
    OBEX_SRM_STATE_DISABLED,
    OBEX_SRM_STATE_SEND_CONFIRM,
    OBEX_SRM_STATE_SEND_CONFIRM_WAIT,
    OBEX_SRM_STATE_ENABLED,
    OBEX_SRM_STATE_ENABLED_WAIT,
} obex_srm_server_state_t;

typedef struct {
    obex_srm_server_state_t srm_state;

    uint8_t srm_value;
    uint8_t srmp_value;
} obex_srm_server_t;

/**
 * Init SRM to disabled state
 * SRM needs to be disabled for each new GET/PUT operation
 * @param obex_srm
 */
void obex_srm_server_init (obex_srm_server_t    *obex_srm);

/**
 * Reset SRM/SRMP fields
 * Needs to be called before parsing request
 * @param obex_srm
 */
void obex_srm_server_reset_fields (obex_srm_server_t *obex_srm);

/**
 * Store SRM or SRMP header field
 * @param obex_srm
 * @param header_id
 * @param total_len
 * @param data_offset
 * @param data_buffer
 * @param data_len
 */
void obex_srm_server_header_store (obex_srm_server_t    *obex_srm,
                                     uint8_t        header_id,
                                     uint16_t       total_len,
                                     uint16_t       data_offset,
                                     const uint8_t *data_buffer,
                                     uint16_t       data_len);

/**
 * Update SRM state based on SRM headers
 * @param obex_srm
 */
void obex_srm_server_handle_headers (obex_srm_server_t *obex_srm);

/**
 * Add SRM headers to current response
 * @param obex_srm
 * @param goep_cid
 */
void obex_srm_server_add_srm_headers (obex_srm_server_t   *obex_srm,
                                      uint16_t             goep_cid);
/**
 * Check if SRM is active
 * @param obex_srm
 */
bool obex_srm_server_is_srm_active (obex_srm_server_t   *obex_srm);

#if defined __cplusplus
}
#endif

#endif
