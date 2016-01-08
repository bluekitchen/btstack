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
    CALL_ACTIVE,
    CALL_HELD
} hfp_gsm_call_status_t;


typedef struct {
    hfp_gsm_call_status_t status;
} hfp_gsm_call_t;


// 
static hfp_gsm_call_t gsm_calls[HFP_GSM_MAX_NR_CALLS]; 

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

// static inline int get_number_none_calls(){
//     return get_number_calls_with_status(CALL_NONE);
// }

static inline int get_number_active_calls(){
    return get_number_calls_with_status(CALL_ACTIVE);
}

static inline int get_number_held_calls(){
    return get_number_calls_with_status(CALL_HELD);
}

hfp_call_status_t hfp_gsm_call_status(){
    if (get_number_active_calls() + get_number_held_calls()){
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

void hfp_gsm_handle_event(hfp_ag_call_event_t event){
    int next_free_slot = get_none_call_index();
    int current_call_index = get_active_call_index();
                
    switch (event){
        case HFP_AG_OUTGOING_CALL_INITIATED:
        case HFP_AG_OUTGOING_REDIAL_INITIATED:

            if (next_free_slot == -1){
                log_error("max nr gsm call exceeded");
                return;
            }

            if (current_call_index != -1){
                gsm_calls[current_call_index].status = CALL_HELD;
            }
            gsm_calls[next_free_slot].status = CALL_ACTIVE;
            callsetup_status = HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_DIALING_STATE;
            break;
        case HFP_AG_OUTGOING_CALL_REJECTED:
            break;
        case HFP_AG_OUTGOING_CALL_ACCEPTED:
            break;
        case HFP_AG_OUTGOING_CALL_RINGING:
            break;
        case HFP_AG_OUTGOING_CALL_ESTABLISHED:
            break;
        default:
            break;
    }
}