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

#define BTSTACK_FILE__ "hfp_gsm_model.c"
 
// *****************************************************************************
//
// GSM Model
//
// *****************************************************************************

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btstack_memory.h"
#include "classic/core.h"
#include "classic/hfp.h"
#include "classic/hfp_gsm_model.h"
#include "classic/sdp_server.h"
#include "classic/sdp_client_rfcomm.h"
#include "btstack_debug.h"
#include "hci.h"
#include "hci_cmd.h"
#include "hci_dump.h"
#include "l2cap.h"
#include "btstack_run_loop.h"

#define HFP_GSM_MAX_NR_CALLS 3
#define HFP_GSM_MAX_CALL_NUMBER_SIZE 25

static hfp_gsm_call_t gsm_calls[HFP_GSM_MAX_NR_CALLS]; 
static hfp_callsetup_status_t callsetup_status = HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS;

static uint8_t clip_type;
static char clip_number[HFP_GSM_MAX_CALL_NUMBER_SIZE];
static char last_dialed_number[HFP_GSM_MAX_CALL_NUMBER_SIZE];

static void hfp_gsm_handler(hfp_ag_call_event_t event, uint8_t index, uint8_t type, const char * number);
static inline int get_number_active_calls(void);

static void set_callsetup_status(hfp_callsetup_status_t status){
    callsetup_status = status;
    if (callsetup_status != HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_ALERTING_STATE) return;
    
    int i ;
    for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
        if (gsm_calls[i].direction == HFP_ENHANCED_CALL_DIR_OUTGOING){
            gsm_calls[i].enhanced_status = HFP_ENHANCED_CALL_STATUS_OUTGOING_ALERTING;
        }
    }
}

static inline void set_enhanced_call_status_active(int index_in_table){
    gsm_calls[index_in_table].enhanced_status = HFP_ENHANCED_CALL_STATUS_ACTIVE;
    gsm_calls[index_in_table].used_slot = 1;
}

static inline void set_enhanced_call_status_held(int index_in_table){
    gsm_calls[index_in_table].enhanced_status = HFP_ENHANCED_CALL_STATUS_HELD;
    gsm_calls[index_in_table].used_slot = 1;
}

static inline void set_enhanced_call_status_response_hold(int index_in_table){
    gsm_calls[index_in_table].enhanced_status = HFP_ENHANCED_CALL_STATUS_CALL_HELD_BY_RESPONSE_AND_HOLD;
    gsm_calls[index_in_table].used_slot = 1;
}

static inline void set_enhanced_call_status_initiated(int index_in_table){
    if (gsm_calls[index_in_table].direction == HFP_ENHANCED_CALL_DIR_OUTGOING){
        gsm_calls[index_in_table].enhanced_status = HFP_ENHANCED_CALL_STATUS_OUTGOING_DIALING;
    } else {
        if (get_number_active_calls() > 0){
            gsm_calls[index_in_table].enhanced_status = HFP_ENHANCED_CALL_STATUS_INCOMING_WAITING;
        } else {
            gsm_calls[index_in_table].enhanced_status = HFP_ENHANCED_CALL_STATUS_INCOMING;
        }
    } 
    gsm_calls[index_in_table].used_slot = 1;
}

static int get_enhanced_call_status(int index_in_table){
    if (!gsm_calls[index_in_table].used_slot) return -1;
    return gsm_calls[index_in_table].enhanced_status;
}

static inline int is_enhanced_call_status_active(int index_in_table){
    return get_enhanced_call_status(index_in_table) == HFP_ENHANCED_CALL_STATUS_ACTIVE;
}

static inline int is_enhanced_call_status_initiated(int index_in_table){
    switch (get_enhanced_call_status(index_in_table)){
        case HFP_ENHANCED_CALL_STATUS_OUTGOING_DIALING:
        case HFP_ENHANCED_CALL_STATUS_OUTGOING_ALERTING:
        case HFP_ENHANCED_CALL_STATUS_INCOMING:
        case HFP_ENHANCED_CALL_STATUS_INCOMING_WAITING:
            return 1;
        default:
            return 0; 
    }
}

static void free_call_slot(int index_in_table){
    gsm_calls[index_in_table].used_slot = 0;
}

void hfp_gsm_init(void){
    set_callsetup_status(HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS);
    clip_type = 0;
    memset(clip_number, 0, sizeof(clip_number));
    memset(last_dialed_number, 0, sizeof(last_dialed_number));
    memset(gsm_calls, 0, sizeof(gsm_calls));
    int i;
    for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
        free_call_slot(i);
    }
} 

static int get_number_calls_with_enhanced_status(hfp_enhanced_call_status_t enhanced_status){
    int i, count = 0;
    for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
        if (get_enhanced_call_status(i) == (int) enhanced_status) count++;
    }
    return count;
}

static int get_call_index_with_enhanced_status(hfp_enhanced_call_status_t enhanced_status){
    int i ;
    for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
        if (get_enhanced_call_status(i) == (int) enhanced_status) return i;
    }
    return -1;
}

static inline int get_initiated_call_index(void){
    int i ;
    for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
        if (is_enhanced_call_status_initiated(i)) return i;
    }
    return -1;
}

static inline int get_next_free_slot(void){
    int i ;
    for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
        if (!gsm_calls[i].used_slot) return i;
    }
    return -1;
}

static inline int get_active_call_index(void){
    return get_call_index_with_enhanced_status(HFP_ENHANCED_CALL_STATUS_ACTIVE);
}

static inline int get_held_call_index(void){
    return get_call_index_with_enhanced_status(HFP_ENHANCED_CALL_STATUS_HELD);
}

static inline int get_response_held_call_index(void){
    return get_call_index_with_enhanced_status(HFP_ENHANCED_CALL_STATUS_CALL_HELD_BY_RESPONSE_AND_HOLD);
}

static inline int get_number_none_calls(void){
    int i, count = 0;
    for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
        if (!gsm_calls[i].used_slot) count++;
    }
    return count;
}

static inline int get_number_active_calls(void){
    return get_number_calls_with_enhanced_status(HFP_ENHANCED_CALL_STATUS_ACTIVE);
}

static inline int get_number_held_calls(void){
    return get_number_calls_with_enhanced_status(HFP_ENHANCED_CALL_STATUS_HELD);
}

static inline int get_number_response_held_calls(void){
    return get_number_calls_with_enhanced_status(HFP_ENHANCED_CALL_STATUS_CALL_HELD_BY_RESPONSE_AND_HOLD);
}

static int next_call_index(void){
    return HFP_GSM_MAX_NR_CALLS + 1 - get_number_none_calls();
}

static void hfp_gsm_set_clip(int index_in_table, uint8_t type, const char * number){
    if (strlen(number) == 0) return;
    
    gsm_calls[index_in_table].clip_type = type;

    int clip_number_size = strlen(number) < HFP_GSM_MAX_CALL_NUMBER_SIZE ? (int) strlen(number) : HFP_GSM_MAX_CALL_NUMBER_SIZE-1;
    strncpy(gsm_calls[index_in_table].clip_number, number, clip_number_size);
    gsm_calls[index_in_table].clip_number[clip_number_size] = '\0';
    strncpy(last_dialed_number, number, clip_number_size);
    last_dialed_number[clip_number_size] = '\0';
    
    clip_type = 0;
    memset(clip_number, 0, sizeof(clip_number));
}

static void delete_call(int delete_index_in_table){
    int i ;
    for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
        if (gsm_calls[i].index > gsm_calls[delete_index_in_table].index){
            gsm_calls[i].index--;
        }
    }
    free_call_slot(delete_index_in_table);
    
    gsm_calls[delete_index_in_table].clip_type = 0;
    gsm_calls[delete_index_in_table].index = 0;
    gsm_calls[delete_index_in_table].clip_number[0] = '\0';
    gsm_calls[delete_index_in_table].mpty = HFP_ENHANCED_CALL_MPTY_NOT_A_CONFERENCE_CALL;
}


static void create_call(hfp_enhanced_call_dir_t direction){
    int next_free_slot = get_next_free_slot();
    gsm_calls[next_free_slot].direction = direction;
    gsm_calls[next_free_slot].index = next_call_index();
    set_enhanced_call_status_initiated(next_free_slot);
    gsm_calls[next_free_slot].clip_type = 0;
    gsm_calls[next_free_slot].clip_number[0] = '\0';
    gsm_calls[next_free_slot].mpty = HFP_ENHANCED_CALL_MPTY_NOT_A_CONFERENCE_CALL;
    
    hfp_gsm_set_clip(next_free_slot, clip_type, clip_number);
}


int hfp_gsm_get_number_of_calls(void){
    return HFP_GSM_MAX_NR_CALLS - get_number_none_calls();
}

void hfp_gsm_clear_last_dialed_number(void){
    memset(last_dialed_number, 0, sizeof(last_dialed_number));
}

char * hfp_gsm_last_dialed_number(void){
    return &last_dialed_number[0];
}

hfp_gsm_call_t * hfp_gsm_call(int call_index){
    int i;

    for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
        hfp_gsm_call_t * call = &gsm_calls[i];
        if (call->index != call_index) continue;
        return call;
    }
    return NULL;
}

uint8_t hfp_gsm_clip_type(void){
    if (clip_type != 0) return clip_type;

    int initiated_call_index = get_initiated_call_index();
    if (initiated_call_index != -1){
        if (gsm_calls[initiated_call_index].clip_type != 0) {
            return gsm_calls[initiated_call_index].clip_type;
        } 
    }

    int active_call_index = get_active_call_index();
    if (active_call_index != -1){
        if (gsm_calls[active_call_index].clip_type != 0) {
            return gsm_calls[active_call_index].clip_type;
        } 
    }
    return 0;
}

char *  hfp_gsm_clip_number(void){
    if (strlen(clip_number) != 0) return clip_number;
    
    int initiated_call_index = get_initiated_call_index();
    if (initiated_call_index != -1){
        if (gsm_calls[initiated_call_index].clip_type != 0) {
            return gsm_calls[initiated_call_index].clip_number;
        } 
    }

    int active_call_index = get_active_call_index();
    if (active_call_index != -1){
        if (gsm_calls[active_call_index].clip_type != 0) {
            return gsm_calls[active_call_index].clip_number;
        } 
    }
    clip_number[0] = 0;
    return clip_number;
}

hfp_call_status_t hfp_gsm_call_status(void){
    if (get_number_active_calls() + get_number_held_calls() + get_number_response_held_calls()){
        return HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT;
    }
    return HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS;
}

hfp_callheld_status_t hfp_gsm_callheld_status(void){
    // @note: order is important
    if (get_number_held_calls() == 0){
        return HFP_CALLHELD_STATUS_NO_CALLS_HELD;
    }
    if (get_number_active_calls() == 0) {
        return HFP_CALLHELD_STATUS_CALL_ON_HOLD_AND_NO_ACTIVE_CALLS;
    }
    return HFP_CALLHELD_STATUS_CALL_ON_HOLD_OR_SWAPPED;
}

hfp_callsetup_status_t hfp_gsm_callsetup_status(void){
    return callsetup_status;
}

static int hfp_gsm_response_held_active(void){
    return get_response_held_call_index() != -1 ;
}

int hfp_gsm_call_possible(void){
    return get_number_none_calls() > 0;
}

void hfp_gsm_handle_event(hfp_ag_call_event_t event){
    hfp_gsm_handler(event, 0, 0, NULL);
}

void hfp_gsm_handle_event_with_clip(hfp_ag_call_event_t event, uint8_t type, const char * number){
    hfp_gsm_handler(event, 0, type, number);
}

void hfp_gsm_handle_event_with_call_index(hfp_ag_call_event_t event, uint8_t index){
    hfp_gsm_handler(event, index, 0, NULL);
}

void hfp_gsm_handle_event_with_call_number(hfp_ag_call_event_t event, const char * number){
    hfp_gsm_handler(event, 0, 0, number);
}

static void hfp_gsm_handler(hfp_ag_call_event_t event, uint8_t index, uint8_t type, const char * number){
    int next_free_slot = get_next_free_slot();
    int current_call_index = get_active_call_index();
    int initiated_call_index = get_initiated_call_index();
    int held_call_index = get_held_call_index();
    int i;

    switch (event){
        case HFP_AG_OUTGOING_CALL_INITIATED:
        case HFP_AG_OUTGOING_REDIAL_INITIATED:
            if (next_free_slot == -1){
                log_error("gsm: max call nr exceeded");
                return;
            }
            create_call(HFP_ENHANCED_CALL_DIR_OUTGOING);
            break;
            
        case HFP_AG_OUTGOING_CALL_REJECTED:
            if (current_call_index != -1){
                delete_call(current_call_index);
            }
            set_callsetup_status(HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS);
            break;

        case HFP_AG_OUTGOING_CALL_ACCEPTED:
            if (current_call_index != -1){
                set_enhanced_call_status_held(current_call_index);
            }
            set_callsetup_status(HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_DIALING_STATE);
            break;
        
        case HFP_AG_OUTGOING_CALL_RINGING:
            if (current_call_index == -1){
                log_error("gsm: no active call");
                return;
            }
            set_callsetup_status(HFP_CALLSETUP_STATUS_OUTGOING_CALL_SETUP_IN_ALERTING_STATE);
            break;
        case HFP_AG_OUTGOING_CALL_ESTABLISHED:
            set_callsetup_status(HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS);
            set_enhanced_call_status_active(initiated_call_index);
            break;

        case HFP_AG_INCOMING_CALL:
            if (hfp_gsm_callsetup_status() != HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS) break;
            set_callsetup_status(HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS);
            create_call(HFP_ENHANCED_CALL_DIR_INCOMING);
            break;
        
        case HFP_AG_INCOMING_CALL_ACCEPTED_BY_AG:
            if (hfp_gsm_callsetup_status() != HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS) break;
            set_callsetup_status(HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS);
            
            if (hfp_gsm_call_status() == HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT){
                set_enhanced_call_status_held(current_call_index);
            }
            set_enhanced_call_status_active(initiated_call_index);
            break;

        case HFP_AG_HELD_CALL_JOINED_BY_AG:
            if (hfp_gsm_call_status() != HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT) break;
            
            // TODO: is following condition correct? Can we join incoming call before it is answered?
            if (callsetup_status == HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS){
                set_enhanced_call_status_active(initiated_call_index);
                set_callsetup_status(HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS);
            } else if (hfp_gsm_callheld_status() == HFP_CALLHELD_STATUS_CALL_ON_HOLD_OR_SWAPPED) {
                set_enhanced_call_status_active(held_call_index);
            } 

            for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
                if (is_enhanced_call_status_active(i)){
                    gsm_calls[i].mpty = HFP_ENHANCED_CALL_MPTY_CONFERENCE_CALL;
                }
            }
            break;

        case HFP_AG_INCOMING_CALL_ACCEPTED_BY_HF:
            if (hfp_gsm_callsetup_status() != HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS) break;
            if (hfp_gsm_call_status() != HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS) break;
            set_callsetup_status(HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS);
            set_enhanced_call_status_active(initiated_call_index);
            break;

        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_AG:
        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_INCOMING_CALL_BY_HF:
            if (hfp_gsm_callsetup_status() != HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS) break;
            if (hfp_gsm_call_status() != HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS) break;
            set_callsetup_status(HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS);
            set_enhanced_call_status_response_hold(initiated_call_index); 
            break;

        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_AG:
        case HFP_AG_RESPONSE_AND_HOLD_ACCEPT_HELD_CALL_BY_HF:
            if (!hfp_gsm_response_held_active()) break;
            set_enhanced_call_status_active(get_response_held_call_index());
            break;

        case HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_AG:
        case HFP_AG_RESPONSE_AND_HOLD_REJECT_HELD_CALL_BY_HF:
            if (!hfp_gsm_response_held_active()) break;
            delete_call(get_response_held_call_index());
            break;

            
        case HFP_AG_TERMINATE_CALL_BY_HF:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    set_callsetup_status(HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS);
                    break;
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    delete_call(current_call_index);
                    break;
            }
            break;

        case HFP_AG_TERMINATE_CALL_BY_AG:
            switch (hfp_gsm_call_status()){
                case HFP_CALL_STATUS_NO_HELD_OR_ACTIVE_CALLS:
                    if (hfp_gsm_callsetup_status() != HFP_CALLSETUP_STATUS_INCOMING_CALL_SETUP_IN_PROGRESS) break;
                    set_callsetup_status(HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS);
                    break;
                case HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT:
                    set_callsetup_status(HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS);
                    delete_call(current_call_index);
                    break;
                default:
                    break;
            }
            break;

        case HFP_AG_CALL_DROPPED:
            set_callsetup_status(HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS);
            if (hfp_gsm_call_status() != HFP_CALL_STATUS_ACTIVE_OR_HELD_CALL_IS_PRESENT) break;
            
            for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
                delete_call(i);
            }
            break;
        
        case HFP_AG_CALL_HOLD_USER_BUSY:
            // Held or waiting call gets active, 
            set_callsetup_status(HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS);
            free_call_slot(initiated_call_index);
            set_enhanced_call_status_active(held_call_index);
            break;
        
        case HFP_AG_CALL_HOLD_RELEASE_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL:
            if (index != 0 && index <= HFP_GSM_MAX_NR_CALLS ){
                for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
                    if (gsm_calls[i].index == index){
                        delete_call(i);
                        continue;
                    }
                }
            } else {
                for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
                    if (is_enhanced_call_status_active(i)){
                        delete_call(i);
                    }
                }    
            }
            
            if (callsetup_status != HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS){
                set_enhanced_call_status_active(initiated_call_index);
            } else {
                set_enhanced_call_status_active(held_call_index);
            }
            
            set_callsetup_status(HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS);
            break;
        
        case HFP_AG_CALL_HOLD_PARK_ACTIVE_ACCEPT_HELD_OR_WAITING_CALL:
            for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
                if (is_enhanced_call_status_active(i) && gsm_calls[i].index != index){
                    set_enhanced_call_status_held(i);
                }
            }
            
            if (callsetup_status != HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS){
                set_enhanced_call_status_active(initiated_call_index);
            } else {
                set_enhanced_call_status_active(held_call_index);
            }
            set_callsetup_status(HFP_CALLSETUP_STATUS_NO_CALL_SETUP_IN_PROGRESS);
            break;
        
        case HFP_AG_CALL_HOLD_ADD_HELD_CALL:
            if (hfp_gsm_callheld_status() != HFP_CALLHELD_STATUS_NO_CALLS_HELD){
                for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
                    if (gsm_calls[i].used_slot){
                        set_enhanced_call_status_active(i);
                        gsm_calls[i].mpty = HFP_ENHANCED_CALL_MPTY_CONFERENCE_CALL;
                    }
                }
            }
            break;

        case HFP_AG_CALL_HOLD_EXIT_AND_JOIN_CALLS:
            for (i = 0; i < HFP_GSM_MAX_NR_CALLS; i++){
                delete_call(i);
            }
            break;
        
        case HFP_AG_SET_CLIP:
            if (initiated_call_index != -1){
                hfp_gsm_set_clip(initiated_call_index, type, number);
                break;
            }

            clip_type = type;
            strncpy(clip_number, number, sizeof(clip_number));
            clip_number[sizeof(clip_number)-1] = '\0';

            break;
        default:
            break;
    }
}
