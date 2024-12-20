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

/**
 * @title HFP Hands-Free (HF)
 *
 */

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
 * @param name or NULL for default value. Provide "" (empty string) to skip attribute
 * @param supported_features 32-bit bitmap, see HFP_HFSF_* values in hfp.h
 * @param codecs_nr  number of codecs in codecs argument
 * @param codecs
 */
void hfp_hf_create_sdp_record_with_codecs(uint8_t * service, uint32_t service_record_handle, int rfcomm_channel_nr,
                                           const char * name, uint16_t supported_features, uint8_t codecs_nr, const uint8_t * codecs);

/**
 * @brief Set up HFP Hands-Free (HF) device without additional supported features. 
 * @param rfcomm_channel_nr
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *                  - L2CAP_SERVICE_ALREADY_REGISTERED, 
 *                  - RFCOMM_SERVICE_ALREADY_REGISTERED or 
 *                  - BTSTACK_MEMORY_ALLOC_FAILED if allocation of any of RFCOMM or L2CAP services failed 
 */
uint8_t hfp_hf_init(uint8_t rfcomm_channel_nr);

/**
 * @brief Set codecs. 
 * @param codecs_nr  number of codecs in codecs argument
 * @param codecs
 */
void hfp_hf_init_codecs(uint8_t codecs_nr, const uint8_t * codecs);

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
void hfp_hf_init_hf_indicators(int indicators_nr, const uint16_t * indicators);


/**
 * @brief Register callback for the HFP Hands-Free (HF) client. 
 * @param callback
 */
void hfp_hf_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Set microphone gain used during SLC for Volume Synchronization.
 *
 * @param gain Valid range: [0,15]
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *              - ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS if invalid gain range
 */
uint8_t hfp_hf_set_default_microphone_gain(uint8_t gain);

/**
 * @brief Set speaker gain used during SLC for Volume Synchronization.
 *
 * @param gain Valid range: [0,15]
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *              - ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS if invalid gain range
 */
uint8_t hfp_hf_set_default_speaker_gain(uint8_t gain);

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
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *              - ERROR_CODE_COMMAND_DISALLOWED if connection already exists, or
 *              - BTSTACK_MEMORY_ALLOC_FAILED 
 */
uint8_t hfp_hf_establish_service_level_connection(bd_addr_t bd_addr);

/**
 * @brief Release the RFCOMM channel and the audio connection between the HF and the AG. 
 * The status of releasing the SLC connection is reported via
 * HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_release_service_level_connection(hci_con_handle_t acl_handle);

/**
 * @brief Enable status update for all indicators in the AG.
 * The status field of the HFP_SUBEVENT_COMPLETE reports if the command was accepted.
 * The status of an AG indicator is reported via HFP_SUBEVENT_AG_INDICATOR_STATUS_CHANGED.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_enable_status_update_for_all_ag_indicators(hci_con_handle_t acl_handle);

/**
 * @brief Disable status update for all indicators in the AG.
 * The status field of the HFP_SUBEVENT_COMPLETE reports if the command was accepted.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_disable_status_update_for_all_ag_indicators(hci_con_handle_t acl_handle);

/**
 * @brief Enable or disable status update for the individual indicators in the AG using bitmap.
 * The status field of the HFP_SUBEVENT_COMPLETE reports if the command was accepted.
 * The status of an AG indicator is reported via HFP_SUBEVENT_AG_INDICATOR_STATUS_CHANGED.
 * 
 * @param acl_handle
 * @param indicators_status_bitmap 32-bit bitmap, 0 - indicator is disabled, 1 - indicator is enabled
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_set_status_update_for_individual_ag_indicators(hci_con_handle_t acl_handle, uint32_t indicators_status_bitmap);

/**
 * @brief Query the name of the currently selected Network operator by AG. 
 * 
 * The name is restricted to max 16 characters. The result is reported via 
 * HFP_SUBEVENT_NETWORK_OPERATOR_CHANGED subtype 
 * containing network operator mode, format and name.
 * If no operator is selected, format and operator are omitted.
 * 
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *              - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist, or
 *              - ERROR_CODE_COMMAND_DISALLOWED if connection in wrong state
 */
uint8_t hfp_hf_query_operator_selection(hci_con_handle_t acl_handle);

/**
 * @brief Enable Extended Audio Gateway Error result codes in the AG.
 * Whenever there is an error relating to the functionality of the AG as a 
 * result of AT command, the AG shall send +CME ERROR. This error is reported via 
 * HFP_SUBEVENT_EXTENDED_AUDIO_GATEWAY_ERROR, see hfp_cme_error_t in hfp.h
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_enable_report_extended_audio_gateway_error_result_code(hci_con_handle_t acl_handle);

/**
 * @brief Disable Extended Audio Gateway Error result codes in the AG.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
 uint8_t hfp_hf_disable_report_extended_audio_gateway_error_result_code(hci_con_handle_t acl_handle);

/**
 * @brief Establish audio connection. 
 * The status of audio connection establishment is reported via HFP_SUBEVENT_AUDIO_CONNECTION_ESTABLISHED.
 * 
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *              - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist, or
 *              - ERROR_CODE_COMMAND_DISALLOWED if connection in wrong state
 */
uint8_t hfp_hf_establish_audio_connection(hci_con_handle_t acl_handle);

/**
 * @brief Release audio connection.
 * The status of releasing of the audio connection is reported via
 * HFP_SUBEVENT_AUDIO_CONNECTION_RELEASED.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_release_audio_connection(hci_con_handle_t acl_handle);

/**
 * @brief Answer incoming call.
 * 
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *              - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist, or
 *              - ERROR_CODE_COMMAND_DISALLOWED if answering incoming call with wrong callsetup status
 */
uint8_t hfp_hf_answer_incoming_call(hci_con_handle_t acl_handle);

/**
 * @brief Reject incoming call.
 * 
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_reject_incoming_call(hci_con_handle_t acl_handle);

/**
 * @brief Release all held calls or sets User Determined User Busy (UDUB) for a waiting call.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_user_busy(hci_con_handle_t acl_handle);

/**
 * @brief Release all active calls (if any exist) and accepts the other (held or waiting) call.
 * 
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_end_active_and_accept_other(hci_con_handle_t acl_handle);

/**
 * @brief Place all active calls (if any exist) on hold and accepts the other (held or waiting) call.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_swap_calls(hci_con_handle_t acl_handle);

/**
 * @brief Add a held call to the conversation.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_join_held_call(hci_con_handle_t acl_handle);

/**
 * @brief Connect the two calls and disconnects the subscriber from both calls (Explicit Call Transfer).
 * 
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_connect_calls(hci_con_handle_t acl_handle);

/**
 * @brief Terminate an incoming or an outgoing call. HFP_SUBEVENT_CALL_TERMINATED is sent upon call termination.
 * 
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_terminate_call(hci_con_handle_t acl_handle);

/**
 * @brief Terminate all held calls.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_terminate_held_calls(hci_con_handle_t acl_handle);

/**
 * @brief Initiate outgoing voice call by providing the destination phone number to the AG. 
 *
 * @param acl_handle
 * @param number
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_dial_number(hci_con_handle_t acl_handle, char * number);

/**
 * @brief Initiate outgoing voice call using the memory dialing feature of the AG.
 *
 * @param acl_handle
 * @param memory_id
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_dial_memory(hci_con_handle_t acl_handle, int memory_id);

/**
 * @brief Initiate outgoing voice call by recalling the last number dialed by the AG.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_redial_last_number(hci_con_handle_t acl_handle);

/**
 * @brief Enable the “Call Waiting notification” function in the AG. 
 * The AG shall send the corresponding result code to the HF whenever 
 * an incoming call is waiting during an ongoing call. In that event,
 * the HFP_SUBEVENT_CALL_WAITING_NOTIFICATION is emitted.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_activate_call_waiting_notification(hci_con_handle_t acl_handle);

/**
 * @brief Disable the “Call Waiting notification” function in the AG.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_deactivate_call_waiting_notification(hci_con_handle_t acl_handle);

/**
 * @brief Enable the “Calling Line Identification notification” function in the AG. 
 * The AG shall issue the corresponding result code just after every RING indication,
 * when the HF is alerted in an incoming call. In that event,
 * the HFP_SUBEVENT_CALLING_LINE_INDETIFICATION_NOTIFICATION is emitted.
 * @param acl_handle
 */
uint8_t hfp_hf_activate_calling_line_notification(hci_con_handle_t acl_handle);

/**
 * @brief Disable the “Calling Line Identification notification” function in the AG.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_deactivate_calling_line_notification(hci_con_handle_t acl_handle);


/**
 * @brief Deactivate echo canceling (EC) and noise reduction (NR) in the AG and emit 
 * HFP_SUBEVENT_ECHO_CANCELING_NOISE_REDUCTION_DEACTIVATE with status ERROR_CODE_SUCCESS if AG supports EC and AG.
 * If the AG supports its own embedded echo canceling and/or noise reduction function, 
 * it shall have EC and NR activated until this function is called.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *              - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist, or 
 *              - ERROR_CODE_COMMAND_DISALLOWED if HFP_(HF/AG)SF_EC_NR_FUNCTION feature is not supported by AG and HF
 */
uint8_t hfp_hf_deactivate_echo_canceling_and_noise_reduction(hci_con_handle_t acl_handle);

/**
 * @brief Activate voice recognition and emit HFP_SUBEVENT_VOICE_RECOGNITION_ACTIVATED event with status ERROR_CODE_SUCCESS 
 * if successful, otherwise ERROR_CODE_COMMAND_DISALLOWED. Prerequisite is established SLC.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *              - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist, or
 *              - ERROR_CODE_COMMAND_DISALLOWED if feature HFP_(HF/AG)SF_VOICE_RECOGNITION_FUNCTION is not supported by HF and AG, or already activated
 */
uint8_t hfp_hf_activate_voice_recognition(hci_con_handle_t acl_handle);

/**
 * @brief Dectivate voice recognition and emit HFP_SUBEVENT_VOICE_RECOGNITION_DEACTIVATED event with status ERROR_CODE_SUCCESS 
 * if successful, otherwise ERROR_CODE_COMMAND_DISALLOWED. Prerequisite is established SLC.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *              - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist, or
 *              - ERROR_CODE_COMMAND_DISALLOWED if feature HFP_(HF/AG)SF_VOICE_RECOGNITION_FUNCTION is not supported by HF and AG, or already activated
 */
uint8_t hfp_hf_deactivate_voice_recognition(hci_con_handle_t acl_handle);


/**
 * @brief Indicate that the HF is ready to accept audio. Prerequisite is established voice recognition session.
 * The HF may call this function during an ongoing AVR (Audio Voice Recognition) session to terminate audio output from 
 * the AG (if there is any) and prepare the AG for new audio input.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *              - ERROR_CODE_COMMAND_DISALLOWED if feature HFP_(HF/AG)SF_ENHANCED_VOICE_RECOGNITION_STATUS is not supported by HF and AG, or wrong VRA status
 */
uint8_t hfp_hf_enhanced_voice_recognition_report_ready_for_audio(hci_con_handle_t acl_handle);


/**
 * @brief Set microphone gain. 
 *
 * @param acl_handle
 * @param gain Valid range: [0,15]
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *              - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist, or
 *              - ERROR_CODE_COMMAND_DISALLOWED if invalid gain range
 */
uint8_t hfp_hf_set_microphone_gain(hci_con_handle_t acl_handle, int gain);

/**
 * @brief Set speaker gain.
 *
 * @param acl_handle
 * @param gain Valid range: [0,15]
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *              - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist, or
 *              - ERROR_CODE_COMMAND_DISALLOWED if invalid gain range
 */
uint8_t hfp_hf_set_speaker_gain(hci_con_handle_t acl_handle, int gain);

/**
 * @brief Instruct the AG to transmit a DTMF code.
 *
 * @param acl_handle
 * @param dtmf_code
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_send_dtmf_code(hci_con_handle_t acl_handle, char code);

/**
 * @brief Read numbers from the AG for the purpose of creating 
 * a unique voice tag and storing the number and its linked voice
 * tag in the HF’s memory. 
 * The number is reported via HFP_SUBEVENT_NUMBER_FOR_VOICE_TAG.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_request_phone_number_for_voice_tag(hci_con_handle_t acl_handle);

/**
 * @brief Query the list of current calls in AG. 
 * The result is received via HFP_SUBEVENT_ENHANCED_CALL_STATUS.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_query_current_call_status(hci_con_handle_t acl_handle);

/**
 * @brief Release a call with index in the AG.
 *
 * @param acl_handle
 * @param index
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_release_call_with_index(hci_con_handle_t acl_handle, int index);

/**
 * @brief Place all parties of a multiparty call on hold with the 
 * exception of the specified call.
 *
 * @param acl_handle
 * @param index
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_private_consultation_with_call(hci_con_handle_t acl_handle, int index);

/**
 * @brief Query the status of the “Response and Hold” state of the AG.
 * The result is reported via HFP_SUBEVENT_RESPONSE_AND_HOLD_STATUS.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_rrh_query_status(hci_con_handle_t acl_handle);

/**
 * @brief Put an incoming call on hold in the AG.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_rrh_hold_call(hci_con_handle_t acl_handle);

/**
 * @brief Accept held incoming call in the AG.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_rrh_accept_held_call(hci_con_handle_t acl_handle);

/**
 * @brief Reject held incoming call in the AG.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_rrh_reject_held_call(hci_con_handle_t acl_handle);

/**
 * @brief Query the AG subscriber number. The result is reported via HFP_SUBEVENT_SUBSCRIBER_NUMBER_INFORMATION.
 *
 * @param acl_handle
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_query_subscriber_number(hci_con_handle_t acl_handle);

/**
 * @brief Set HF indicator.
 *
 * @param acl_handle
 * @param assigned_number
 * @param value
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist
 */
uint8_t hfp_hf_set_hf_indicator(hci_con_handle_t acl_handle, int assigned_number, int value);

/**
 * @brief Tests if in-band ringtone is active on AG (requires SLC)
 *
 * @param acl_handler
 * @return 0 if unknown acl_handle or in-band ring-tone disabled, otherwise 1
 */
int hfp_hf_in_band_ringtone_active(hci_con_handle_t acl_handle);

/**
 * @brief Provide Apple Accessory information after SLC
 * @param vendor_id
 * @param product_id
 * @param version
 * @param features bitmask: bit 0 = reserved, bit 1 = battery reporting, bit 2 = docked or powered, bit 3 = Siri
 */
void hfp_hf_apple_set_identification(uint16_t vendor_id, uint16_t product_id, const char * version, uint8_t features);

/**
 * @brief Set Apple Accessory Battery Level
 * @param battery_level range: 0..9
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS battery level out of range
 */
uint8_t hfp_hf_apple_set_battery_level(uint8_t battery_level);

/**
 * @brief Set Apple Accessory Docked State
 * @param docked 0 = undocked, 1 = docked
 * @return status ERROR_CODE_SUCCESS if successful, otherwise ERROR_CODE_INVALID_HCI_COMMAND_PARAMETERS docked state invalid
 */
uint8_t hfp_hf_apple_set_docked_state(uint8_t docked);

/**
 * @brief Send AT command (most likely a vendor-specific command not part of standard HFP).
 * @note  Result (OK/ERROR) is reported via HFP_SUBEVENT_CUSTOM_AT_MESSAGE_SENT
 *        To receive potential unsolicited result code, add ENABLE_HFP_AT_MESSAGES to get all message via HFP_SUBEVENT_AT_MESSAGE_RECEIVED
 *
 * @param acl_handle
 * @param at_command to send
 * @return status ERROR_CODE_SUCCESS if successful, otherwise:
 *              - ERROR_CODE_UNKNOWN_CONNECTION_IDENTIFIER if connection does not exist, or
 *              - ERROR_CODE_COMMAND_DISALLOWED if extended audio gateway error report is disabled
 */
uint8_t hfp_hf_send_at_command(hci_con_handle_t acl_handle, const char * at_command);

/**
 * @brief Register custom AT command.
 * @param hfp_custom_at_command (with '+' prefix)
 */
void hfp_hf_register_custom_at_command(hfp_custom_at_command_t * custom_at_command);

/**
 * @brief De-Init HFP HF
 */
void hfp_hf_deinit(void);

/**
 * @brief Create HFP Hands-Free (HF) SDP service record.
 * @deprecated Use hfp_hf_create_sdp_record_with_codecs instead
 * @param service
 * @param rfcomm_channel_nr
 * @param name
 * @param suported_features 32-bit bitmap, see HFP_HFSF_* values in hfp.h
 * @param wide_band_speech supported
 */
void hfp_hf_create_sdp_record(uint8_t * service, uint32_t service_record_handle, int rfcomm_channel_nr, const char * name, uint16_t supported_features, int wide_band_speech);

/* API_END */

#if defined __cplusplus
}
#endif

#endif // BTSTACK_HFP_HF_H
