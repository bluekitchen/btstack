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
//  Minimal setup for HFP Hands-Free (HF) unit (!! UNDER DEVELOPMENT !!)
//
// *****************************************************************************


#ifndef btstack_hfp_hf_h
#define btstack_hfp_hf_h

#include "hci.h"
#include "sdp_query_rfcomm.h"
#include "hfp.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */


/**
 * @brief Create HFP Hands-Free (HF) SDP service record. 
 */
void hfp_hf_create_sdp_record(uint8_t * service, int rfcomm_channel_nr, const char * name, uint16_t supported_features);

/**
 * @brief Intialize HFP Hands-Free (HF) device. 
 * TODO:  move optional params into setters
 */
void hfp_hf_init(uint16_t rfcomm_channel_nr, uint32_t supported_features, uint8_t * codecs, int codecs_nr, uint16_t * indicators, int indicators_nr, uint32_t indicators_status);

/**
 * @brief Register callback for the HFP Hands-Free (HF) client. 
 */
void hfp_hf_register_packet_handler(hfp_callback_t callback);

/**
 * @brief Establish RFCOMM connection, and perform service level connection agreement:
 * - exchange of supported features
 * - retrieve Audio Gateway (AG) indicators and their status 
 * - enable indicator status update in the AG
 * - notify the AG about its own available codecs, if possible
 * - retrieve the AG information describing the call hold and multiparty services, if possible
 * - retrieve which HF indicators are enabled on the AG, if possible
 */
void hfp_hf_establish_service_level_connection(bd_addr_t bd_addr);


/**
 * @brief Release the RFCOMM channel and the audio connection between the HF and the AG. 
 * TODO: trigger release of the audio connection
 */
void hfp_hf_release_service_level_connection(bd_addr_t bd_addr);

/**
 * @brief Deactivate/reactivate status update for all indicators in the AG.
 */
void hfp_hf_enable_status_update_for_all_ag_indicators(bd_addr_t bd_addr, uint8_t enable);

/**
 * @brief Deactivate/reactivate status update for the individual indicators in the AG using bitmap.
 */
void hfp_hf_enable_status_update_for_ag_indicator(bd_addr_t bd_addr, uint32_t indicators_status_bitmap, uint8_t enable);


/**
 * @brief
 */
void hfp_hf_transfer_signal_strength_indication(bd_addr_t bd_addr);

/**
 * @brief
 */
void hfp_hf_transfer_roaming_status_indication(bd_addr_t bd_addr);

/**
 * @brief
 */
 void hfp_hf_transfer_battery_level_indication_of_ag(bd_addr_t bd_addr);

/* API_END */

#if defined __cplusplus
}
#endif

#endif