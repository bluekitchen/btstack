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


#ifndef BTSTACK_HFP_HF_H
#define BTSTACK_HFP_HF_H

#include "hci.h"
#include "classic/sdp_client_rfcomm.h"
#include "classic/hfp.h"

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
 * @param wide_band_speech supported
 */
void hfp_hf_create_sdp_record(uint8_t * service, uint32_t service_record_handle, int rfcomm_channel_nr, const char * name, uint16_t supported_features, int wide_band_speech);

/**
 * @brief Set up HFP Hands-Free (HF) device without additional supported features. 
 * @param rfcomm_channel_nr
 */
void hfp_hf_init(uint16_t rfcomm_channel_nr);

/**
 * @brief Set codecs. 
 * @param codecs_nr
 * @param codecs
 */
void hfp_hf_init_codecs(int codecs_nr, uint8_t * codecs);

/**
 * @brief Set supported features.
 * @param supported_features 32-bit bitmap, see HFP_HFSF_* values in hfp.h
 */
void hfp_hf_init_supported_features(uint32_t supported_features);

/**
 * @brief Set HF indicators. 
 * @param indicators_nr
 * @param indicators
 */
void hfp_hf_init_hf_indicators(int indicators_nr, uint16_t * indicators);


/**
 * @brief Register callback for the HFP Hands-Free (HF) client. 
 * @param callback
 */
void hfp_hf_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Establish RFCOMM connection with the AG with given Bluetooth address, 
 * and perform service level connection (SLC) agreement:
 * - exchange supported features
 * - retrieve Audio Gateway (AG) indicators and their status 
 * - enable indicator status update in the AG
 * - notify the AG about its own available codecs, if possible
 * - retrieve the AG information describing the call hold and multiparty services, if possible
 * - retrieve which HF indicators are enabled on the AG, if possible
 * The status of SLC connection establishment is reported via
 * HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED.
 *
 * @param bd_addr Bluetooth address of the AG
 */
void hfp_hf_establish_service_level_connection(bd_addr_t bd_addr);

/**
 * @brief Release the RFCOMM channel and the audio connection between the HF and the AG. 
 * The status of releasing the SLC connection is reported via
 * HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED.
 *
 * @param acl_handle of the AG
 */
void hfp_hf_release_service_level_connection(hci_con_handle_t acl_handle);

/**
 * @brief Enable status update for all indicators in the AG.
 * The status field of the HFP_SUBEVENT_COMPLETE reports if the command was accepted.
 * The status of an AG indicator is reported via HFP_SUBEVENT_AG_INDICATOR_STATUS_CHANGED.
 *
 * @param acl_handle of the AG
 */
void hfp_hf_enable_status_update_for_all_ag_indicators(hci_con_handle_t acl_handle);

/**
 * @brief Disable status update for all indicators in the AG.
 * The status field of the HFP_SUBEVENT_COMPLETE reports if the command was accepted.
 * @param acl_handle of the AG
 */
void hfp_hf_disable_status_update_for_all_ag_indicators(hci_con_handle_t acl_handle);

/**
 * @brief Enable or disable status update for the individual indicators in the AG using bitmap.
 * The status field of the HFP_SUBEVENT_COMPLETE reports if the command was accepted.
 * The status of an AG indicator is reported via HFP_SUBEVENT_AG_INDICATOR_STATUS_CHANGED.
 * 
 * @param acl_handle of the AG
 * @param indicators_status_bitmap 32-bit bitmap, 0 - indicator is disabled, 1 - indicator is enabled
 */
void hfp_hf_set_status_update_for_individual_ag_indicators(hci_con_handle_t acl_handle, uint32_t indicators_status_bitmap);

/**
 * @brief Query the name of the currently selected Network operator by AG. 
 * 
 * The name is restricted to max 16 characters. The result is reported via 
 * HFP_SUBEVENT_NETWORK_OPERATOR_CHANGED subtype 
 * containing network operator mode, format and name.
 * If no operator is selected, format and operator are omitted.
 * 
 * @param acl_handle of the AG
 */
void hfp_hf_query_operator_selection(hci_con_handle_t acl_handle);

/**
 * @brief Enable Extended Audio Gateway Error result codes in the AG.
 * Whenever there is an error relating to the functionality of the AG as a 
 * result of AT command, the AG shall send +CME ERROR. This error is reported via 
 * HFP_SUBEVENT_EXTENDED_AUDIO_GATEWAY_ERROR, see hfp_cme_error_t in hfp.h
 *
 * @param acl_handle of the AG
 */
void hfp_hf_enable_report_extended_audio_gateway_error_result_code(hci_con_handle_t acl_handle);

/**
 * @brief Disable Extended Audio Gateway Error result codes in the AG.
 *
 * @param acl_handle of the AG
 */
 void hfp_hf_disable_report_extended_audio_gateway_error_result_code(hci_con_handle_t acl_handle);

/**
 * @brief Establish audio connection. 
 * The status of audio connection establishment is reported via
 * HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED.
 * @param acl_handle of the AG
 */
void hfp_hf_establish_audio_connection(hci_con_handle_t acl_handle);

/**
 * @brief Release audio connection.
 * The status of releasing of the audio connection is reported via
 * HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED.
 *
 * @param acl_handle of the AG
 */
void hfp_hf_release_audio_connection(hci_con_handle_t acl_handle);

/**
 * @brief Answer incoming call.
 * @param acl_handle of the AG
 */
void hfp_hf_answer_incoming_call(hci_con_handle_t acl_handle);

/**
 * @brief Reject incoming call.
 * @param acl_handle of the AG
 */
void hfp_hf_reject_incoming_call(hci_con_handle_t acl_handle);

/**
 * @brief Release all held calls or sets User Determined User Busy (UDUB) for a waiting call.
 * @param acl_handle of the AG
 */
void hfp_hf_user_busy(hci_con_handle_t acl_handle);

/**
 * @brief Release all active calls (if any exist) and accepts the other (held or waiting) call.
 * @param acl_handle of the AG
 */
void hfp_hf_end_active_and_accept_other(hci_con_handle_t acl_handle);

/**
 * @brief Place all active calls (if any exist) on hold and accepts the other (held or waiting) call.
 * @param acl_handle of the AG
 */
void hfp_hf_swap_calls(hci_con_handle_t acl_handle);

/**
 * @brief Add a held call to the conversation.
 * @param acl_handle of the AG
 */
void hfp_hf_join_held_call(hci_con_handle_t acl_handle);

/**
 * @brief Connect the two calls and disconnects the subscriber from both calls (Explicit Call
Transfer).
 * @param acl_handle of the AG
 */
void hfp_hf_connect_calls(hci_con_handle_t acl_handle);

/**
 * @brief Terminate an incoming or an outgoing call. 
 * HFP_SUBEVENT_CALL_TERMINATED is sent upon call termination.
 * @param acl_handle of the AG
 */
void hfp_hf_terminate_call(hci_con_handle_t acl_handle);

/**
 * @brief Initiate outgoing voice call by providing the destination phone number to the AG. 
 * @param acl_handle of the AG
 * @param number
 */
void hfp_hf_dial_number(hci_con_handle_t acl_handle, char * number);

/**
 * @brief Initiate outgoing voice call using the memory dialing feature of the AG.
 * @param acl_handle of the AG
 * @param memory_id
 */
void hfp_hf_dial_memory(hci_con_handle_t acl_handle, int memory_id);

/**
 * @brief Initiate outgoing voice call by recalling the last number dialed by the AG.
 * @param acl_handle of the AG
 */
void hfp_hf_redial_last_number(hci_con_handle_t acl_handle);

/*
 * @brief Enable the “Call Waiting notification” function in the AG. 
 * The AG shall send the corresponding result code to the HF whenever 
 * an incoming call is waiting during an ongoing call. In that event,
 * the HFP_SUBEVENT_CALL_WAITING_NOTIFICATION is emitted.
 *
 * @param acl_handle of the AG
 */
void hfp_hf_activate_call_waiting_notification(hci_con_handle_t acl_handle);

/*
 * @brief Disable the “Call Waiting notification” function in the AG.
 * @param acl_handle of the AG
 */
void hfp_hf_deactivate_call_waiting_notification(hci_con_handle_t acl_handle);

/*
 * @brief Enable the “Calling Line Identification notification” function in the AG. 
 * The AG shall issue the corresponding result code just after every RING indication,
 * when the HF is alerted in an incoming call. In that event,
 * the HFP_SUBEVENT_CALLING_LINE_INDETIFICATION_NOTIFICATION is emitted.
 * @param acl_handle of the AG
 */
void hfp_hf_activate_calling_line_notification(hci_con_handle_t acl_handle);

/*
 * @brief Disable the “Calling Line Identification notification” function in the AG.
 * @param acl_handle of the AG
 */
void hfp_hf_deactivate_calling_line_notification(hci_con_handle_t acl_handle);


/*
 * @brief Activate echo canceling and noise reduction in the AG. By default, 
 * if the AG supports its own embedded echo canceling and/or noise reduction 
 * functions, it shall have them activated until this function is called.
 * If the AG does not support any echo canceling and noise reduction functions, 
 * it shall respond with the ERROR indicator (TODO)
 * @param acl_handle of the AG
 */
void hfp_hf_activate_echo_canceling_and_noise_reduction(hci_con_handle_t acl_handle);

/*
 * @brief Deactivate echo canceling and noise reduction in the AG.
 */
void hfp_hf_deactivate_echo_canceling_and_noise_reduction(hci_con_handle_t acl_handle);

/*
 * @brief Activate voice recognition function.
 * @param acl_handle of the AG
 */
void hfp_hf_activate_voice_recognition_notification(hci_con_handle_t acl_handle);

/*
 * @brief Dectivate voice recognition function.
 * @param acl_handle of the AG
 */
void hfp_hf_deactivate_voice_recognition_notification(hci_con_handle_t acl_handle);

/*
 * @brief Set microphone gain. 
 * @param acl_handle of the AG
 * @param gain Valid range: [0,15]
 */
void hfp_hf_set_microphone_gain(hci_con_handle_t acl_handle, int gain);

/*
 * @brief Set speaker gain.
 * @param acl_handle of the AG
 * @param gain Valid range: [0,15]
 */
void hfp_hf_set_speaker_gain(hci_con_handle_t acl_handle, int gain);

/*
 * @brief Instruct the AG to transmit a DTMF code.
 * @param acl_handle of the AG
 * @param dtmf_code
 */
void hfp_hf_send_dtmf_code(hci_con_handle_t acl_handle, char code);

/*
 * @brief Read numbers from the AG for the purpose of creating 
 * a unique voice tag and storing the number and its linked voice
 * tag in the HF’s memory. 
 * The number is reported via HFP_SUBEVENT_NUMBER_FOR_VOICE_TAG.
 * @param acl_handle of the AG
 */
void hfp_hf_request_phone_number_for_voice_tag(hci_con_handle_t acl_handle);

/*
 * @brief Query the list of current calls in AG. 
 * The result is received via HFP_SUBEVENT_ENHANCED_CALL_STATUS.
 * @param acl_handle of the AG
 */
void hfp_hf_query_current_call_status(hci_con_handle_t acl_handle);

/*
 * @brief Release a call with index in the AG.
 * @param acl_handle of the AG
 * @param index
 */
void hfp_hf_release_call_with_index(hci_con_handle_t acl_handle, int index);

/*
 * @brief Place all parties of a multiparty call on hold with the 
 * exception of the specified call.
 * @param acl_handle of the AG
 * @param index
 */
void hfp_hf_private_consultation_with_call(hci_con_handle_t acl_handle, int index);

/*
 * @brief Query the status of the “Response and Hold” state of the AG.
 * The result is reported via HFP_SUBEVENT_RESPONSE_AND_HOLD_STATUS.
 * @param acl_handle of the AG
 */
void hfp_hf_rrh_query_status(hci_con_handle_t acl_handle);

/*
 * @brief Put an incoming call on hold in the AG.
 * @param acl_handle of the AG
 */
void hfp_hf_rrh_hold_call(hci_con_handle_t acl_handle);

/*
 * @brief Accept held incoming call in the AG.
 * @param acl_handle of the AG
 */
void hfp_hf_rrh_accept_held_call(hci_con_handle_t acl_handle);

/*
 * @brief Reject held incoming call in the AG.
 * @param acl_handle of the AG
 */
void hfp_hf_rrh_reject_held_call(hci_con_handle_t acl_handle);

/*
 * @brief Query the AG subscriber number.
 * The result is reported via HFP_SUBEVENT_SUBSCRIBER_NUMBER_INFORMATION.
 * @param acl_handle of the AG
 */
void hfp_hf_query_subscriber_number(hci_con_handle_t acl_handle);

/*
 * @brief Set HF indicator.
 * @param acl_handle of the AG
 * @param assigned_number
 * @param value
 */
void hfp_hf_set_hf_indicator(hci_con_handle_t acl_handle, int assigned_number, int value);

/*
 * @brief Tests if in-band ringtone is active on AG (requires SLC)
 * @aram acl_handler of the AG
 */
int hfp_hf_in_band_ringtone_active(hci_con_handle_t acl_handle);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // BTSTACK_HFP_HF_H
