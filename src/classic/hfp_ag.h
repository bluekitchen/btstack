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

/**
 * @title HFP Audio Gateway (AG)
 *
 */

#ifndef btstack_hfp_ag_h
#define btstack_hfp_ag_h

#include "hci.h"
#include "classic/sdp_client_rfcomm.h"
#include "classic/hfp.h"
#include "classic/hfp_gsm_model.h"

#if defined __cplusplus
extern "C" {
#endif

/* API_START */
typedef struct {
    uint8_t type;
    const char * number;
} hfp_phone_number_t;

/**
 * @brief Create HFP Audio Gateway (AG) SDP service record. 
 * @param service
 * @param rfcomm_channel_nr
 * @param name
 * @param ability_to_reject_call
 * @param suported_features 32-bit bitmap, see HFP_AGSF_* values in hfp.h
 * @param wide_band_speech supported 
 */
void hfp_ag_create_sdp_record(uint8_t * service, uint32_t service_record_handle, int rfcomm_channel_nr, const char * name, uint8_t ability_to_reject_call, uint16_t supported_features, int wide_band_speech);

/**
 * @brief Set up HFP Audio Gateway (AG) device without additional supported features.
 * @param rfcomm_channel_nr
 */
void hfp_ag_init(uint16_t rfcomm_channel_nr);

/**
 * @brief Set codecs. 
 * @param codecs_nr
 * @param codecs
 */
void hfp_ag_init_codecs(int codecs_nr, const uint8_t * codecs);

/**
 * @brief Set supported features.
 * @param supported_features 32-bit bitmap, see HFP_AGSF_* values in hfp.h
 */
void hfp_ag_init_supported_features(uint32_t supported_features);

/**
 * @brief Set AG indicators. 
 * @param indicators_nr
 * @param indicators
 */
void hfp_ag_init_ag_indicators(int ag_indicators_nr, const hfp_ag_indicator_t * ag_indicators);

/**
 * @brief Set HF indicators. 
 * @param indicators_nr
 * @param indicators
 */
void hfp_ag_init_hf_indicators(int hf_indicators_nr, const hfp_generic_status_indicator_t * hf_indicators);

/**
 * @brief Set Call Hold services. 
 * @param indicators_nr
 * @param indicators
 */
void hfp_ag_init_call_hold_services(int call_hold_services_nr, const char * call_hold_services[]);


/**
 * @brief Register callback for the HFP Audio Gateway (AG) client. 
 * @param callback
 */
void hfp_ag_register_packet_handler(btstack_packet_handler_t callback);

/**
 * @brief Enable in-band ring tone.
 * @param use_in_band_ring_tone
 */
void hfp_ag_set_use_in_band_ring_tone(int use_in_band_ring_tone);


// actions used by local device / user

/**
 * @brief Establish RFCOMM connection, and perform service level connection agreement:
 * - exchange of supported features
 * - report Audio Gateway (AG) indicators and their status 
 * - enable indicator status update in the AG
 * - accept the information about available codecs in the Hands-Free (HF), if sent
 * - report own information describing the call hold and multiparty services, if possible
 * - report which HF indicators are enabled on the AG, if possible
 * The status of SLC connection establishment is reported via
 * HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_ESTABLISHED.
 *
 * @param bd_addr Bluetooth address of the HF
 */
void hfp_ag_establish_service_level_connection(bd_addr_t bd_addr);

/**
 * @brief Release the RFCOMM channel and the audio connection between the HF and the AG. 
 * If the audio connection exists, it will be released.
 * The status of releasing the SLC connection is reported via
 * HFP_SUBEVENT_SERVICE_LEVEL_CONNECTION_RELEASED.
 *
 * @param bd_addr Bluetooth address of the HF
 */
void hfp_ag_release_service_level_connection(hci_con_handle_t acl_handle);

/**
 * @brief Establish audio connection.
 * The status of Audio connection establishment is reported via is reported via
 * HSP_SUBEVENT_AUDIO_CONNECTION_COMPLETE.
 * @param bd_addr Bluetooth address of the HF
 */
void hfp_ag_establish_audio_connection(hci_con_handle_t acl_handle);

/**
 * @brief Release audio connection.
 * The status of releasing the Audio connection is reported via is reported via
 * HSP_SUBEVENT_AUDIO_DISCONNECTION_COMPLETE.
 * @param bd_addr Bluetooth address of the HF
 */
void hfp_ag_release_audio_connection(hci_con_handle_t acl_handle);

/**
 * @brief Put the current call on hold, if it exists, and accept incoming call. 
 */
void hfp_ag_answer_incoming_call(void);

/**
 * @brief Join held call with active call.
 */
void hfp_ag_join_held_call(void);

/**
 * @brief Reject incoming call, if exists, or terminate active call.
 */
void hfp_ag_terminate_call(void);

/*
 * @brief Put incoming call on hold.
 */
void hfp_ag_hold_incoming_call(void);

/*
 * @brief Accept the held incoming call.
 */
void hfp_ag_accept_held_incoming_call(void);

/*
 * @brief Reject the held incoming call.
 */
void hfp_ag_reject_held_incoming_call(void);

/*
 * @brief Set microphone gain. 
 * @param bd_addr Bluetooth address of the HF
 * @param gain Valid range: [0,15]
 */
void hfp_ag_set_microphone_gain(hci_con_handle_t acl_handle, int gain);

/*
 * @brief Set speaker gain.
 * @param bd_addr Bluetooth address of the HF
 * @param gain Valid range: [0,15]
 */
void hfp_ag_set_speaker_gain(hci_con_handle_t acl_handle, int gain);

/*
 * @brief Set battery level.
 * @param level Valid range: [0,5]
 */
void hfp_ag_set_battery_level(int level);

/*
 * @brief Clear last dialed number.
 */
void hfp_ag_clear_last_dialed_number(void);

/*
 * @brief Set last dialed number.
 */
void hfp_ag_set_last_dialed_number(const char * number);

/*
 * @brief Notify the HF that an incoming call is waiting 
 * during an ongoing call. The notification will be sent only if the HF has
 * has previously enabled the "Call Waiting notification" in the AG. 
 * @param bd_addr Bluetooth address of the HF
 */
void hfp_ag_notify_incoming_call_waiting(hci_con_handle_t acl_handle);

// Voice Recognition

/*
 * @brief Activate voice recognition.
 * @param bd_addr Bluetooth address of the HF
 * @param activate
 */
void hfp_ag_activate_voice_recognition(hci_con_handle_t acl_handle, int activate);

void hfp_ag_enhanced_voice_recognition_activate(hci_con_handle_t acl_handle);

void hfp_ag_enhanced_voice_recognition_status(hci_con_handle_t acl_handle, hfp_voice_recognition_state_t state);
void hfp_ag_enhanced_voice_recognition_starting_sound(hci_con_handle_t acl_handle);
void hfp_ag_enhanced_voice_recognition_ready_for_input(hci_con_handle_t acl_handle);
void hfp_ag_enhanced_voice_recognition_processing_input(hci_con_handle_t acl_handle);

void hfp_ag_enhanced_voice_recognition_message(hci_con_handle_t acl_handle, hfp_voice_recognition_state_t state, hfp_voice_recognition_message_t msg);
void hfp_ag_enhanced_voice_recognition_deactivate(hci_con_handle_t acl_handle);

/*
 * @brief Send a phone number back to the HF.
 * @param bd_addr Bluetooth address of the HF
 * @param phone_number
 */
void hfp_ag_send_phone_number_for_voice_tag(hci_con_handle_t acl_handle, const char * phone_number);

/*
 * @brief Reject sending a phone number to the HF.
 * @param bd_addr Bluetooth address of the HF
 */
void hfp_ag_reject_phone_number_for_voice_tag(hci_con_handle_t acl_handle);

/**
 * @brief Store phone number with initiated call.
 * @param type
 * @param number
 */
void hfp_ag_set_clip(uint8_t type, const char * number);


// Cellular Actions

/**
 * @brief Pass the accept incoming call event to the AG.
 */
void hfp_ag_incoming_call(void);

/**
 * @brief Outgoing call initiated
 */
void hfp_ag_outgoing_call_initiated(const char * number);

/**
 * @brief Pass the reject outgoing call event to the AG.
 */
void hfp_ag_outgoing_call_rejected(void);

/**
 * @brief Pass the accept outgoing call event to the AG.
 */
void hfp_ag_outgoing_call_accepted(void);

/**
 * @brief Pass the outgoing call ringing event to the AG.
 */
void hfp_ag_outgoing_call_ringing(void);

/**
 * @brief Pass the outgoing call established event to the AG.
 */
void hfp_ag_outgoing_call_established(void);

/**
 * @brief Pass the call droped event to the AG.
 */
void hfp_ag_call_dropped(void);

/*
 * @brief Set network registration status.  
 * @param status 0 - not registered, 1 - registered 
 */
void hfp_ag_set_registration_status(int status);

/*
 * @brief Set network signal strength.
 * @param strength [0-5]
 */
void hfp_ag_set_signal_strength(int strength);

/*
 * @brief Set roaming status.
 * @param status 0 - no roaming, 1 - roaming active
 */
void hfp_ag_set_roaming_status(int status);

/*
 * @brief Set subcriber number information, e.g. the phone number 
 * @param numbers
 * @param numbers_count
 */
void hfp_ag_set_subcriber_number_information(hfp_phone_number_t * numbers, int numbers_count);

/*
 * @brief Called by cellular unit after a DTMF code was transmitted, so that the next one can be emitted.
 * @param bd_addr Bluetooth address of the HF 
 */
void hfp_ag_send_dtmf_code_done(hci_con_handle_t acl_handle);

/**
 * @brief Report Extended Audio Gateway Error result codes in the AG.
 * Whenever there is an error relating to the functionality of the AG as a 
 * result of AT command, the AG shall send +CME ERROR:
 * - +CME ERROR: 0  - AG failure
 * - +CME ERROR: 1  - no connection to phone 
 * - +CME ERROR: 3  - operation not allowed 
 * - +CME ERROR: 4  - operation not supported 
 * - +CME ERROR: 5  - PH-SIM PIN required 
 * - +CME ERROR: 10 - SIM not inserted 
 * - +CME ERROR: 11 - SIM PIN required 
 * - +CME ERROR: 12 - SIM PUK required 
 * - +CME ERROR: 13 - SIM failure
 * - +CME ERROR: 14 - SIM busy
 * - +CME ERROR: 16 - incorrect password 
 * - +CME ERROR: 17 - SIM PIN2 required 
 * - +CME ERROR: 18 - SIM PUK2 required 
 * - +CME ERROR: 20 - memory full
 * - +CME ERROR: 21 - invalid index
 * - +CME ERROR: 23 - memory failure
 * - +CME ERROR: 24 - text string too long
 * - +CME ERROR: 25 - invalid characters in text string
 * - +CME ERROR: 26 - dial string too long
 * - +CME ERROR: 27 - invalid characters in dial string
 * - +CME ERROR: 30 - no network service
 * - +CME ERROR: 31 - network Timeout.
 * - +CME ERROR: 32 - network not allowed – Emergency calls only
 *
 * @param bd_addr Bluetooth address of the HF
 * @param error
 */
void hfp_ag_report_extended_audio_gateway_error_result_code(hci_con_handle_t acl_handle, hfp_cme_error_t error);

/**
 * @brief De-Init HFP AG
 */
void hfp_ag_deinit(void);

/* API_END */

// testing
hfp_ag_indicator_t * hfp_ag_get_ag_indicators(hfp_connection_t * hfp_connection);

#if defined __cplusplus
}
#endif

#endif
