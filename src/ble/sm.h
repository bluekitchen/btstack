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
 * @title Security Manager
 * 
 */

#ifndef SM_H
#define SM_H

#if defined __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "btstack_util.h"
#include "btstack_defines.h"
#include "hci.h"

typedef struct {
    btstack_linked_item_t  item;
    bd_addr_t      address;
    bd_addr_type_t address_type;
} sm_lookup_entry_t;

/* API_START */

/**
 * @brief Initializes the Security Manager, connects to L2CAP
 */
void sm_init(void);

/**
 * @brief Set secret ER key for key generation as described in Core V4.0, Vol 3, Part G, 5.2.2
 * @note If not set and btstack_tlv is configured, ER key is generated and stored in TLV by SM
 * @param er key
 */
void sm_set_er(sm_key_t er);

/**
 * @brief Set secret IR key for key generation as described in Core V4.0, Vol 3, Part G, 5.2.2
 * @note If not set and btstack_tlv is configured, IR key is generated and stored in TLV by SM
 * @param ir key
 */
void sm_set_ir(sm_key_t ir);

/**
 * @brief Registers OOB Data Callback. The callback should set the oob_data and return 1 if OOB data is availble
 * @param get_oob_data_callback
 */
void sm_register_oob_data_callback( int (*get_oob_data_callback)(uint8_t address_type, bd_addr_t addr, uint8_t * oob_data));

/**
 * @brief Add event packet handler. 
 * @param callback_handler
 */
void sm_add_event_handler(btstack_packet_callback_registration_t * callback_handler);

/**
 * @brief Remove event packet handler.
 * @param callback_handler
 */
void sm_remove_event_handler(btstack_packet_callback_registration_t * callback_handler);

/**
 * @brief Limit the STK generation methods. Bonding is stopped if the resulting one isn't in the list
 * @param OR combination of SM_STK_GENERATION_METHOD_ 
 */
void sm_set_accepted_stk_generation_methods(uint8_t accepted_stk_generation_methods);

/**
 * @brief Set the accepted encryption key size range. Bonding is stopped if the result isn't within the range
 * @param min_size (default 7)
 * @param max_size (default 16)
 */
void sm_set_encryption_key_size_range(uint8_t min_size, uint8_t max_size);

/**
 * @brief Sets the requested authentication requirements, bonding yes/no, MITM yes/no, SC yes/no, keypress yes/no
 * @param OR combination of SM_AUTHREQ_ flags
 */
void sm_set_authentication_requirements(uint8_t auth_req);

/**
 * @brief Sets the available IO Capabilities
 * @param IO_CAPABILITY_
 */
void sm_set_io_capabilities(io_capability_t io_capability);

/**
 * @brief Enable/disable Secure Connections Mode only
 * @param enable secure connections only mode
 */
void sm_set_secure_connections_only_mode(bool enable);

/**
 * @brief Let Peripheral request an encrypted connection right after connecting
 * @note Not used normally. Bonding is triggered by access to protected attributes in ATT Server
 */
void sm_set_request_security(int enable);

/** 
 * @brief Trigger Security Request
 * @deprecated please use sm_request_pairing instead
 */
void sm_send_security_request(hci_con_handle_t con_handle);

/**
 * @brief Decline bonding triggered by event before
 * @param con_handle
 */
void sm_bonding_decline(hci_con_handle_t con_handle);

/**
 * @brief Confirm Just Works bonding 
 * @param con_handle
 */
void sm_just_works_confirm(hci_con_handle_t con_handle);

/**
 * @brief Confirm value from SM_EVENT_NUMERIC_COMPARISON_REQUEST for Numeric Comparison bonding 
 * @param con_handle
 */
void sm_numeric_comparison_confirm(hci_con_handle_t con_handle);

/**
 * @brief Reports passkey input by user
 * @param con_handle
 * @param passkey in [0..999999]
 */
void sm_passkey_input(hci_con_handle_t con_handle, uint32_t passkey);

/**
 * @brief Send keypress notification for keyboard only devices
 * @param con_handle
 * @param action see SM_KEYPRESS_* in bluetooth.h
 */
void sm_keypress_notification(hci_con_handle_t con_handle, uint8_t action);

/**
 * @brief Used by att_server.c and gatt_client.c to request user authentication
 * @param con_handle
 */
void sm_request_pairing(hci_con_handle_t con_handle);

/**
 * @brief Report user authorization decline.
 * @param con_handle
 */
void sm_authorization_decline(hci_con_handle_t con_handle);

/**
 * @brief Report user authorization grant.
 * @param con_handle
 */
void sm_authorization_grant(hci_con_handle_t con_handle);

/**
 * @brief Support for signed writes, used by att_server.
 * @return ready
 */
int sm_cmac_ready(void);

/**
 * @brief Support for signed writes, used by att_server.
 * @note Message is in little endian to allows passing in ATT PDU without flipping. 
 * @note signing data: [opcode, attribute_handle, message, sign_counter]
 * @note calculated hash in done_callback is big endian and has 16 byte. 
 * @param key
 * @param opcde
 * @param attribute_handle
 * @param message_len
 * @param message
 * @param sign_counter
 */
void sm_cmac_signed_write_start(const sm_key_t key, uint8_t opcode, uint16_t attribute_handle, uint16_t message_len, const uint8_t * message, uint32_t sign_counter, void (*done_callback)(uint8_t * hash));

/**
 * @brief Match address against bonded devices
 * @param address_type
 * @param address
 * @return 0 if successfully added to lookup queue
 * @note Triggers SM_IDENTITY_RESOLVING_* events
 */
int sm_address_resolution_lookup(uint8_t address_type, bd_addr_t address);

/**
 * @brief Get Identity Resolving state
 * @param con_handle
 * @return irk_lookup_state_t
 * @note return IRK_LOOKUP_IDLE if connection does not exist
 */
irk_lookup_state_t sm_identity_resolving_state(hci_con_handle_t con_handle);

/**
 * @brief Identify device in LE Device DB.
 * @param con_handle
 * @return index from le_device_db or -1 if not found/identified
 */
int sm_le_device_index(hci_con_handle_t con_handle);

/**
 * @brief Use fixec passkey for Legacy and SC instead of generating a random number
 * @note Can be used to improve security over Just Works if no keyboard or displary are present and 
 *       individual random passkey can be printed on the device during production
 * @param passkey
 */
void sm_use_fixed_passkey_in_display_role(uint32_t passkey);

/**
 * @brief Allow connection re-encryption in Peripheral (Responder) role for LE Legacy Pairing 
 *       without entry for Central device stored in LE Device DB
 * @note BTstack in Peripheral Role (Responder) supports LE Legacy Pairing without a persistent LE Device DB as
 *       the LTK is reconstructed from a local secret IRK and EDIV + Random stored on Central (Initiator) device
 *       On the downside, it's not really possible to delete a pairing if this is enabled.
 * @param allow encryption using reconstructed LTK without stored entry (Default: 1)
 */
void sm_allow_ltk_reconstruction_without_le_device_db_entry(int allow);

/**
 * @brief Generate OOB data for LE Secure Connections
 * @note This generates a 128 bit random number ra and then calculates Ca = f4(PKa, PKa, ra, 0)
 *       New OOB data should be generated for each pairing. Ra is used for subsequent OOB pairings
 * @param callback
 * @return status
 */
uint8_t sm_generate_sc_oob_data(void (*callback)(const uint8_t * confirm_value, const uint8_t * random_value));

/**
 * @brief Registers OOB Data Callback for LE Secure Conections. The callback should set all arguments and return 1 if OOB data is availble
 * @note the oob_sc_local_random usually is the random_value returend by sm_generate_sc_oob_data
 * @param get_oob_data_callback
 */
void sm_register_sc_oob_data_callback( int (*get_sc_oob_data_callback)(uint8_t address_type, bd_addr_t addr, uint8_t * oob_sc_peer_confirm, uint8_t * oob_sc_peer_random));

/**
 * @bbrief Register LTK Callback that allows to provide a custom LTK on re-encryption. The callback returns true if LTK was modified
 * @param get_ltk_callback
 */
void sm_register_ltk_callback( bool (*get_ltk_callback)(hci_con_handle_t con_handle, uint8_t address_type, bd_addr_t addr, uint8_t * ltk));

/* API_END */

/**
 * @brief De-Init SM
 */
void sm_deinit(void);

// PTS testing
void sm_test_set_irk(sm_key_t irk);
void sm_test_use_fixed_local_csrk(void);

#ifdef ENABLE_TESTING_SUPPORT
void sm_test_set_pairing_failure(int reason);
#endif

#if defined __cplusplus
}
#endif

#endif // SM_H
