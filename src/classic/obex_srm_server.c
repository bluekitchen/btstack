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

#define BTSTACK_FILE__ "opp_srm.c"

#include "btstack_config.h"

#include <stdint.h>

#include "btstack_event.h"

#include "classic/obex.h"
#include "classic/obex_parser.h"
#include "classic/goep_server.h"

#include "classic/obex_srm_server.h"

void obex_srm_server_init (obex_srm_server_t *obex_srm){
    obex_srm->srm_state = OBEX_SRM_STATE_DISABLED;
    obex_srm_server_reset_fields(obex_srm);
}

void obex_srm_server_reset_fields (obex_srm_server_t *obex_srm){
    obex_srm->srm_value = OBEX_SRM_DISABLE;
    obex_srm->srmp_value = OBEX_SRMP_NEXT;
}

void obex_srm_server_header_store (obex_srm_server_t    *obex_srm,
                                   uint8_t        header_id,
                                   uint16_t       total_len,
                                   uint16_t       data_offset,
                                   const uint8_t *data_buffer,
                                   uint16_t       data_len){
    switch (header_id) {
        case OBEX_HEADER_SINGLE_RESPONSE_MODE:
            obex_parser_header_store(&obex_srm->srm_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        case OBEX_HEADER_SINGLE_RESPONSE_MODE_PARAMETER:
            obex_parser_header_store(&obex_srm->srmp_value, 1, total_len, data_offset, data_buffer, data_len);
            break;
        default:
            break;
    }
}

void obex_srm_server_handle_headers (obex_srm_server_t *obex_srm){
    switch (obex_srm->srm_state) {
        case OBEX_SRM_STATE_DISABLED:
            if (obex_srm->srm_value == OBEX_SRM_ENABLE) {
                if (obex_srm->srmp_value == OBEX_SRMP_WAIT) {
                    obex_srm->srm_state = OBEX_SRM_STATE_SEND_CONFIRM_WAIT;
                } else {
                    obex_srm->srm_state = OBEX_SRM_STATE_SEND_CONFIRM;
                }
            }
            break;
        case OBEX_SRM_STATE_ENABLED_WAIT:
            if (obex_srm->srmp_value == OBEX_SRMP_NEXT) {
                obex_srm->srm_state = OBEX_SRM_STATE_ENABLED;
            }
            break;
        default:
            break;
    }
}

void obex_srm_server_add_srm_headers (obex_srm_server_t *obex_srm,
                                      uint16_t    goep_cid){
    switch (obex_srm->srm_state) {
        case OBEX_SRM_STATE_SEND_CONFIRM:
            goep_server_header_add_srm_enable (goep_cid);
            obex_srm->srm_state = OBEX_SRM_STATE_ENABLED;
            break;
        case OBEX_SRM_STATE_SEND_CONFIRM_WAIT:
            goep_server_header_add_srm_enable (goep_cid);
            obex_srm->srm_state = OBEX_SRM_STATE_ENABLED_WAIT;
            break;
        default:
            break;
    }
}

bool obex_srm_server_is_srm_active (obex_srm_server_t *obex_srm){
    return obex_srm->srm_state == OBEX_SRM_STATE_ENABLED;
}

