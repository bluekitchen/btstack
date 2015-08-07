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
//  Minimal setup for HFP Audio Gateway (AG) unit (!! UNDER DEVELOPMENT !!)
//
// *****************************************************************************


#ifndef btstack_hfp_ag_h
#define btstack_hfp_ag_h

#include "hci.h"
#include "sdp_query_rfcomm.h"
#include "hfp.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @brief Create HFP Audio Gateway (AG) SDP service record. 
 */
void hfp_ag_create_sdp_record(uint8_t * service, int rfcomm_channel_nr, const char * name, uint8_t ability_to_reject_call, uint16_t supported_features);;

/**
 * @brief Intialize HFP Audio Gateway (AG) device. 
 * TODO:  move optional params into setters
 */
void hfp_ag_init(uint16_t rfcomm_channel_nr, uint32_t supported_features, 
    uint8_t * codecs, int codecs_nr, 
    hfp_ag_indicator_t * ag_indicators, int ag_indicators_nr,
    hfp_generic_status_indicators_t * hf_indicators, int hf_indicators_nr,
    char *call_hold_services[], int call_hold_services_nr);

/**
 * @brief Register callback for the HFP Audio Gateway (AG) client. 
 */
void hfp_ag_register_packet_handler(hfp_callback_t callback);

/**
 * @brief Establish RFCOMM connection, and perform service level connection agreement:
 * - exchange of supported features
 * - report Audio Gateway (AG) indicators and their status 
 * - enable indicator status update in the AG
 * - accept the information about available codecs in the Hands-Free (HF), if sent
 * - report own information describing the call hold and multiparty services, if possible
 * - report which HF indicators are enabled on the AG, if possible
 */
void hfp_ag_establish_service_level_connection(bd_addr_t bd_addr);

/**
 * @brief Release the RFCOMM channel and the audio connection between the HF and the AG. 
 * TODO: trigger release of the audio connection
 */
void hfp_ag_release_service_level_connection(bd_addr_t bd_addr);

/* API_END */

#if defined __cplusplus
}
#endif

#endif