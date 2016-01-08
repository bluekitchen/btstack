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
//  GSM model (!! UNDER DEVELOPMENT !!)
//
// *****************************************************************************


#ifndef BTSTACK_HFP_GSM_MODEL_H
#define BTSTACK_HFP_GSM_MODEL_H

#include "hci.h"
#include "sdp_query_rfcomm.h"
#include "hfp.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @brief
 */

hfp_callheld_status_t hfp_gsm_callheld_status();
hfp_call_status_t hfp_gsm_call_status();
hfp_callsetup_status_t hfp_gsm_callsetup_status();

void hfp_gsm_handle_event(hfp_ag_call_event_t event);

// /**
//  * @brief 
//  */
// void hfp_gsm_incoming_call(void);


// /
// /**
//  * @brief Report the change in AG's call status. 
//  * Call status:
//  * - 0 = No calls (held or active)
//  * - 1 = Call is present (active or held) 
//  */
// void hfp_gsm_transfer_call_status(bd_addr_t bd_addr, hfp_call_status_t status);

// /**
//  * @brief Report the change in AG's call setup status.
//  * Call setup status:
//  * - 0 = No call setup in progress 
//  * - 1 = Incoming call setup in progress
//  * - 2 = Outgoing call setup in dialing state
//  * - 3 = Outgoing call setup in alerting state
//  */
// void hfp_gsm_transfer_callsetup_status(bd_addr_t bd_addr, hfp_callsetup_status_t status);

// /**
//  * @brief Report the change in AG's held call status.
//  * Held call status:
//  * - 0 = No calls held
//  * - 1 = Call is placed on hold or active/held calls are swapped
//  * - 2 = Call on hold, no active calls
//  */
// void hfp_gsm_transfer_callheld_status(bd_addr_t bd_addr, hfp_callheld_status_t status);

// /**
//  * @brief Enable in-band ring tone
//  */
// void hfp_gsm_set_use_in_band_ring_tone(int use_in_band_ring_tone);



// /**
//  * @brief number is stored.
//  */
// void hfp_gsm_set_clip(uint8_t type, const char * number);

// /**
//  * @brief 
//  */
// void hfp_gsm_outgoing_call_rejected(void);

// /**
//  * @brief 
//  */
// void hfp_gsm_outgoing_call_accepted(void);

// /**
//  * @brief 
//  */
// void hfp_gsm_outgoing_call_ringing(void);

// /**
//  * @brief 
//  */
// void hfp_gsm_outgoing_call_established(void);

// *
//  * @brief 
 
// void hfp_gsm_call_dropped(void);

// /**
//  * @brief 
//  */
// void hfp_gsm_answer_incoming_call(void);

// /**
//  * @brief 
//  */
// void hfp_gsm_join_held_call(void);

// /**
//  * @brief 
//  */
// void hfp_gsm_terminate_call(void);

// /*
//  * @brief
//  */
// void hfp_gsm_set_registration_status(int status);

// /*
//  * @brief
//  */
// void hfp_gsm_set_signal_strength(int strength);

// /*
//  * @brief
//  */
// void hfp_gsm_set_roaming_status(int status);


// /*
//  * @brief
//  */
// void hfp_gsm_activate_voice_recognition(bd_addr_t bd_addr, int activate);


// /*
//  * @brief
//  */
// void hfp_gsm_send_phone_number_for_voice_tag(bd_addr_t bd_addr, const char * number);

// /*
//  * @brief
//  */
// void hfp_gsm_reject_phone_number_for_voice_tag(bd_addr_t bd_addr);

// /*
//  * @brief
//  */
// void hfp_gsm_send_dtmf_code_done(bd_addr_t bd_addr);

// /*
//  * @brief
//  */
// void hfp_gsm_set_subcriber_number_information(hfp_phone_number_t * numbers, int numbers_count);

// /*
//  * @brief
//  */
// void hfp_gsm_send_current_call_status(bd_addr_t bd_addr, int idx, hfp_enhanced_call_dir_t dir, 
//     hfp_enhanced_call_status_t status, hfp_enhanced_call_mode_t mode, 
//     hfp_enhanced_call_mpty_t mpty, uint8_t type, const char * number);

// /*
//  * @brief
//  */
// void hfp_gsm_send_current_call_status_done(bd_addr_t bd_addr);

// /*
//  * @brief
//  */
// void hfp_gsm_hold_incoming_call(void);

// /*
//  * @brief
//  */
// void hfp_gsm_accept_held_incoming_call(void);

// /*
//  * @brief
//  */
// void hfp_gsm_reject_held_incoming_call(void);

/* API_END */

#if defined __cplusplus
}
#endif

#endif