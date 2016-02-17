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
//  HFP Hands-Free (HF) unit
//
// *****************************************************************************


#ifndef __BTSTACK_HFP_HF_H
#define __BTSTACK_HFP_HF_H

#include "hci.h"
#include "sdp_query_rfcomm.h"
#include "hfp.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */

/**
 * @brief Create HFP Hands-Free (HF) SDP service record. 
 * @param service
 * @param rfcomm_channel_nr
 * @param name
 * @param suported_features 32-bit bitmap, see HFP_HFSF_* values in hfp.h
 */
void hfp_hf_create_sdp_record(uint8_t * service, int rfcomm_channel_nr, const char * name, uint32_t supported_features);

/**
 * @brief Set up HFP Hands-Free (HF) device. 
 * 
 * @param rfcomm_channel_nr
 */
void hfp_hf_init(uint16_t rfcomm_channel_nr);

/**
 * @brief Set codecs. 
 * 
 * @param codecs_nr
 * @param codecs
 */
void hfp_hf_set_codecs(int codecs_nr, uint8_t * codecs);

/**
 * @brief Set supported feature bitmap. 
 * 
 * @param supported_features 32-bit bitmap, see HFP_HFSF_* values in hfp.h
 */
void hfp_hf_set_supported_features(uint32_t supported_features);

/**
 * @brief Set HF indicators. 
 * 
 * @param indicators_nr
 * @param indicators
 */
void hfp_hf_set_indicators(int indicators_nr, uint16_t * indicators);

/**
 * @brief Set HF indicators status bitmap. 
 * 
 * @param indicators_status 32-bit bitmap, 0 - indicator is disabled, 1 - indicator is enabled
 */
void hfp_hf_set_indicators_status(uint32_t indicators_status);

/**
 * @brief Register callback for the HFP Hands-Free (HF) client. 
 */
void hfp_hf_register_packet_handler(hfp_callback_t callback);

/**
 * @brief Establish RFCOMM connection with the AG with given Bluetooth address, and perform service level connection agreement:
 * - exchange supported features
 * - retrieve Audio Gateway (AG) indicators and their status 
 * - enable indicator status update in the AG
 * - notify the AG about its own available codecs, if possible
 * - retrieve the AG information describing the call hold and multiparty services, if possible
 * - retrieve which HF indicators are enabled on the AG, if possible
 *
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_establish_service_level_connection(bd_addr_t bd_addr);

/**
 * @brief Release the RFCOMM channel and the audio connection between the HF and the AG. 
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_release_service_level_connection(bd_addr_t bd_addr);

/**
 * @brief Enable status update for all indicators in the AG.
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_enable_status_update_for_all_ag_indicators(bd_addr_t bd_addr);

/**
 * @brief Disable status update for all indicators in the AG.
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_disable_status_update_for_all_ag_indicators(bd_addr_t bd_addr);

/**
 * @brief Enable or disable status update for the individual indicators in the AG using bitmap.
 *
 * @param bd_addr Bluetooth address of the AG
 * @param indicators_status_bitmap 32-bit bitmap, 0 - indicator is disabled, 1 - indicator is enabled
 */
void hfp_hf_set_status_update_for_individual_ag_indicators(bd_addr_t bd_addr, uint32_t indicators_status_bitmap);


/**
 * @brief Find out the name of the currently selected Network operator by AG. 
 * 
 * The name is restricted to max 16 characters. If the operator is selected, a 
 * HCI_EVENT_HFP_META event with HFP_SUBEVENT_NETWORK_OPERATOR_CHANGED subtype 
 * containing network operator mode, format and name, is sent.
 * If no operator is selected, format and operator are omitted.
 *
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_query_operator_selection(bd_addr_t bd_addr);

/**
 * @brief Enable Extended Audio Gateway Error result codes in the AG.
 * Whenever there is an error relating to the functionality of the AG as a 
 * result of AT command, the AG shall send +CME ERROR, see hfp_cme_error_t in hfp.h
 *
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_enable_report_extended_audio_gateway_error_result_code(bd_addr_t bd_addr);

/**
 * @brief Disable Extended Audio Gateway Error result codes in the AG.
 *
 * @param bd_addr Bluetooth address of the AG
 */
 void hfp_hf_disable_report_extended_audio_gateway_error_result_code(bd_addr_t bd_addr);

/**
 * @brief 
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_establish_audio_connection(bd_addr_t bd_addr);

/**
 * @brief 
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_release_audio_connection(bd_addr_t bd_addr);

/**
 * @brief 
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_answer_incoming_call(bd_addr_t bd_addr);

/**
 * @brief 
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_reject_call(bd_addr_t bd_addr);

/**
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_user_busy(bd_addr_t addr);

/**
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_end_active_and_accept_other(bd_addr_t addr);

/**
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_swap_calls(bd_addr_t addr);

/**
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_join_held_call(bd_addr_t addr);

/**
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_connect_calls(bd_addr_t addr);

/**
 * @brief 
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_terminate_call(bd_addr_t bd_addr);

/**
 * @brief 
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_dial_number(bd_addr_t bd_addr, char * number);

/**
 * @brief
 * TODO: use int for number instead of string? 
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_dial_memory(bd_addr_t bd_addr, char * number);

/**
 * @brief 
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_redial_last_number(bd_addr_t bd_addr);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_activate_call_waiting_notification(bd_addr_t bd_addr);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_deactivate_call_waiting_notification(bd_addr_t bd_addr);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_activate_calling_line_notification(bd_addr_t bd_addr);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_deactivate_calling_line_notification(bd_addr_t bd_addr);


/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_activate_echo_canceling_and_noise_reduction(bd_addr_t bd_addr);

/*
 * @brief
 */
void hfp_hf_deactivate_echo_canceling_and_noise_reduction(bd_addr_t bd_addr);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_activate_voice_recognition_notification(bd_addr_t bd_addr);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_deactivate_voice_recognition_notification(bd_addr_t bd_addr);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_set_microphone_gain(bd_addr_t bd_addr, int gain);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_set_speaker_gain(bd_addr_t bd_addr, int gain);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_send_dtmf_code(bd_addr_t bd_addr, char code);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_request_phone_number_for_voice_tag(bd_addr_t addr);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_query_current_call_status(bd_addr_t addr);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_release_call_with_index(bd_addr_t addr, int index);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_private_consultation_with_call(bd_addr_t addr, int index);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_rrh_query_status(bd_addr_t addr);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_rrh_hold_call(bd_addr_t addr);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_rrh_accept_held_call(bd_addr_t addr);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_rrh_reject_held_call(bd_addr_t addr);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_query_subscriber_number(bd_addr_t addr);

/*
 * @brief
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_set_hf_indicator(bd_addr_t addr, int assigned_number, int value);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // __BTSTACK_HFP_HF_H