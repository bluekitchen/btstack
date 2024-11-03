/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "obex_srm_client.c"

#include "btstack_debug.h"

#include "classic/goep_client.h"
#include "classic/obex.h"
#include "classic/obex_srm_client.h"

void obex_srm_client_init(obex_srm_client_t * obex_srm){
    obex_srm->srm_state = OBEX_SRM_CLIENT_STATE_DISABLED;
    obex_srm->srmp_waiting = false;
    obex_srm_client_reset_fields(obex_srm);
}

void obex_srm_client_set_waiting(obex_srm_client_t * obex_srm, bool waiting){
    log_info("Set SRMP Waiting: %u", (int) waiting);
    obex_srm->srmp_waiting = waiting;
}

void obex_srm_client_reset_fields(obex_srm_client_t * obex_srm){
    obex_srm->srm_value = OBEX_SRM_DISABLE;
    obex_srm->srmp_value = OBEX_SRMP_NEXT;
}

void obex_srm_client_handle_headers(obex_srm_client_t *obex_srm) {
    // Update SRM state based on SRM headers
    switch (obex_srm->srm_state){
        case OBEX_SRM_CLIENT_STATE_W4_CONFIRM:
            switch (obex_srm->srm_value){
                case OBEX_SRM_ENABLE:
                    switch (obex_srm->srmp_value){
                        case OBEX_SRMP_WAIT:
                            obex_srm->srm_state = OBEX_SRM_CLIENT_STATE_ENABLED_BUT_WAITING;
                            break;
                        default:
                            obex_srm->srm_state = OBEX_SRM_CLIENT_STATE_ENABLED;
                            break;
                    }
                    break;
                default:
                    obex_srm->srm_state = OBEX_SRM_CLIENT_STATE_DISABLED;
                    break;
            }
            break;
        case OBEX_SRM_CLIENT_STATE_ENABLED_BUT_WAITING:
            switch (obex_srm->srmp_value){
                case OBEX_SRMP_WAIT:
                    obex_srm->srm_state = OBEX_SRM_CLIENT_STATE_ENABLED_BUT_WAITING;
                    break;
                default:
                    obex_srm->srm_state = OBEX_SRM_CLIENT_STATE_ENABLED;
                    break;
            }
            break;
        default:
            break;
    }
    log_info("Handle SRM %u, SRMP %u -> new SRM state %u", obex_srm->srm_value, obex_srm->srmp_value,obex_srm->srm_state);
}

void obex_srm_client_prepare_header(obex_srm_client_t *obex_srm, uint16_t goep_cid){
    if (goep_client_version_20_or_higher(goep_cid)){
        // add SRM Enable Header if not enabled yet
        if (obex_srm->srm_state == OBEX_SRM_CLIENT_STATE_DISABLED){
            obex_srm->srm_state = OBEX_SRM_CLIENT_STATE_W4_CONFIRM;
            log_info("Prepare: add SRM Enable");
            goep_client_header_add_srm_enable(goep_cid);
        }
        // add SRMP Waiting header if waiting is active
        if (obex_srm->srmp_waiting){
            log_info("Prepare: add SRMP Waiting");
            goep_client_header_add_srmp_waiting(goep_cid);
        }
    }
}

bool obex_srm_client_is_srm_active(obex_srm_client_t *obex_srm){
    return (obex_srm->srm_state == OBEX_SRM_CLIENT_STATE_ENABLED) && (obex_srm->srmp_waiting == false);
}
