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
 
// *****************************************************************************
//
// Minimal setup for HFP Audio Gateway (AG) unit (!! UNDER DEVELOPMENT !!)
//
// *****************************************************************************

#include "btstack-config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <btstack/hci_cmds.h>
#include <btstack/run_loop.h>

#include "hci.h"
#include "btstack_memory.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "sdp_query_rfcomm.h"
#include "sdp.h"
#include "debug.h"
#include "hfp.h"
#include "hfp_gsm_model.h"

#define HFP_GSM_MAX_NR_CALLS 3

typedef enum{
    CALL_NONE,
    CALL_INITIATED,
    CALL_RESPONSE_HOLD,
    CALL_ACTIVE,
    CALL_HELD
} hfp_gsm_call_status_t;


typedef struct {
    hfp_gsm_call_status_t status;
    uint8_t clip_type;
    char    clip_number[25];
} hfp_gsm_call_t;


// 
static hfp_gsm_call_t gsm_calls[HFP_GSM_MAX_NR_CALLS]; 

void hfp_gsm_module_init(void){
    memset(gsm_calls, 0, sizeof(gsm_calls));
}   
//
static hfp_callsetup_status_t callsetup_status = HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;


static int get_number_calls_with_status(hfp_gsm_call_status_t status){
    int i, count = 0;
    for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
        if (gsm_calls[i].status == status) count++;
    }
    return count;
}

static int get_call_index_with_status(hfp_gsm_call_status_t status){
    int i ;
    for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
        if (gsm_calls[i].status == status) return i;
    }
    return -1;
}

static inline int get_none_call_index(){
    return get_call_index_with_status(CALL_NONE);
}

static inline int get_active_call_index(){
    return get_call_index_with_status(CALL_ACTIVE);
}

static inline int get_initiated_call_index(){
    return get_call_index_with_status(CALL_INITIATED);
}

static inline int get_held_call_index(){
    return get_call_index_with_status(CALL_HELD);
}

static inline int get_response_held_call_index(){
    return get_call_index_with_status(CALL_RESPONSE_HOLD);
}

static inline int get_number_none_calls(){
    return get_number_calls_with_status(CALL_NONE);
}

static inline int get_number_active_calls(){
    return get_number_calls_with_status(CALL_ACTIVE);
}

static inline int get_number_held_calls(){
    return get_number_calls_with_status(CALL_HELD);
}

static inline int get_number_response_held_calls(){
    return get_number_calls_with_status(CALL_RESPONSE_HOLD);
}

hfp_call_status_t hfp_gsm_call_status(){
    if (get_number_active_calls() + get_number_held_calls() + get_number_response_held_calls()){
        return HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT;
    }
    return HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS;
}

hfp_callheld_status_t hfp_gsm_callheld_status(){
    // @note: order is important
    if (get_number_held_calls() == 0){
        return HFP_CALLHELD_STATUS_NO_CALLS_HELD;
    }
    if (get_number_active_calls() == 0) {
        return HFP_CALLHELD_STATUS_CALL_ON_HOLD_AND_NO_ACTIVE_CALLS;
    }
    return HFP_CALLHELD_STATUS_CALL_ON_HOLD_OR_SWAPPED;
}

hfp_callsetup_status_t hfp_gsm_callsetup_status(){
    return callsetup_status;
}

int hfp_gsm_response_held_active(){
    return get_response_held_call_index() != -1 ;
}

int hfp_gsm_call_possible(void){
    return get_number_none_calls() > 0;
}

void hfp_gsm_handle_event(hfp_ag_call_event_t event){
    int next_free_slot = get_none_call_index();
    int current_call_index = get_active_call_index();
    int initiated_call_index = get_initiated_call_index();
    int held_call_index = get_held_call_index();

    switch (event){
        case HFP_AG_OUTGOING_CALL_INITIATED:
        case HFP_AG_OUTGOING_REDIAL_INITIATED:
            if (next_free_slot == -1){
                log_error("gsm: max call nr exceeded");
                return;
            }
            break;

        case HFP_AG_OUTGOING_CALL_REJECTED:
            if (current_call_index != -1){
                gsm_calls[current_call_index].status = CALL_NONE;
            }
            callsetup_status = HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;
            break;

        case HFP_AG_OUTGOING_CALL_ACCEPTED:
            if (current_call_index != -1){
                gsm_calls[current_call_index].status = CALL_HELD;
            }
            gsm_calls[next_free_slot].status = CALL_INITIATED;
            callsetup_status = HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_DIALING_STATE;
            break;
        
        case HFP_AG_OUTGOING_CALL_RINGING:
            if (current_call_index == -1){
                log_error("gsm: no active call");
                return;
            }
            callsetup_status = HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_ALERTING_STATE;
            break;
        case HFP_AG_OUTGOING_CALL_ESTABLISHED:
            callsetup_status = HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;
            gsm_calls[initiated_call_index].status = CALL_ACTIVE;
            break;

        case HFP_AG_INCOMING_CALL:
            printf("HFP_AG_INCOMING_CALL \n");
            if (hfp_gsm_callsetup_status() != HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS) break;
            printf("HFP_AG_INCOMING_CALL 1: HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS -> HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS, CALL_INITIATED %d\n", next_free_slot);
            callsetup_status = HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS;
            gsm_calls[next_free_slot].status = CALL_INITIATED;
            gsm_calls[next_free_slot].clip_type = 0;
            break;
        
        case HFP_AG_INCOMING_CALL_ACCEPTED_BY_AG:
            printf("HFP_AG_INCOMING_CALL_ACCEPTED_BY_AG \n");
            if (hfp_gsm_callsetup_status() != HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS) break;
            callsetup_status = HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;
            
            if (hfp_gsm_call_status() == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT){
                printf("HFP_AG_INCOMING_CALL_ACCEPTED_BY_AG 1: HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT,  CALL_HELD %d\n", current_call_index);
                gsm_calls[current_call_index].status = CALL_HELD;
            }
            printf("HFP_AG_INCOMING_CALL_ACCEPTED_BY_AG 2: HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT,  CALL_ACTIVE %d\n", initiated_call_index);
                
            gsm_calls[initiated_call_index].status = CALL_ACTIVE;
            break;

        case HFP_AG_HELD_CALL_JOINED_BY_AG:
            if (hfp_gsm_callsetup_status() != HFP_CALLHELD_STATUS_CALL_ON_HOLD_OR_SWAPPED) break;
            if (hfp_gsm_call_status() != HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT) break;
            gsm_calls[held_call_index].status = CALL_ACTIVE;
            break;

        case HFP_AG_INCOMING_CALL_ACCEPTED_BY_HF:
            printf("HFP_AG_INCOMING_CALL_ACCEPTED_BY_HF \n");
            if (hfp_gsm_callsetup_status() != HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS) break;
            printf("HFP_AG_INCOMING_CALL_ACCEPTED_BY_HF 1: HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS\n");
            if (hfp_gsm_call_status() != HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS) break;
            printf("HFP_AG_INCOMING_CALL_ACCEPTED_BY_HF 2\n");
            callsetup_status = HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;
            gsm_calls[initiated_call_index].status = CALL_ACTIVE;
            break;

        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_AG:
        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_HF:
            if (hfp_gsm_callsetup_status() != HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS) break;
            if (hfp_gsm_call_status() != HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS) break;
            callsetup_status = HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;
            gsm_calls[initiated_call_index].status = CALL_RESPONSE_HOLD; 
            break;

        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_AG:
        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_HF:
            if (!hfp_gsm_response_held_active()) break;
            gsm_calls[get_response_held_call_index()].status = CALL_ACTIVE;
            break;

        case HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_AG:
        case HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_HF:
            if (!hfp_gsm_response_held_active()) break;
            gsm_calls[get_response_held_call_index()].status = CALL_NONE;
            break;

            
        case HFP_AG_TERMINATE_CALL_BY_HF:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    callsetup_status = HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;
                    break;
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    gsm_calls[current_call_index].status = CALL_NONE;
                    break;
            }
            break;

        case HFP_AG_TERMINATE_CALL_BY_AG:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    if (hfp_gsm_callsetup_status() != HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS) break;
                    callsetup_status = HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;
                    break;
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    callsetup_status = HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;
                    gsm_calls[current_call_index].status = CALL_NONE;
                    break;
                default:
                    break;
            }
            break;

        case HFP_AG_CALL_DROPPED:{
                callsetup_status = HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;
                if (hfp_gsm_call_status() != HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT) break;
                
                int i ;
                for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
                    gsm_calls[i].status = CALL_NONE;
                    gsm_calls[i].clip_type = 0;
                    gsm_calls[i].clip_number[0] = '\0';
                }
            }
            break;
        
        default:
            break;
    }
}