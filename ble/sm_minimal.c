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
 
#include <stdio.h>
#include <strings.h>

#include "debug.h"
#include "hci.h"
#include "l2cap.h"
#include "le_device_db.h"
#include "sm.h"
#include "gap_le.h"

//
// SM internal types and globals
//

static void sm_run();

// used to notify applicationss that user interaction is neccessary, see sm_notify_t below
static btstack_packet_handler_t sm_client_packet_handler = NULL;
static security_manager_state_t sm_state_responding = SM_GENERAL_IDLE;
static uint16_t sm_response_handle = 0;
static uint8_t  sm_pairing_failed_reason = 0;
static gap_random_address_type_t gap_random_adress_type = GAP_RANDOM_ADDRESS_TYPE_OFF;

void sm_set_er(sm_key_t er){}
void sm_set_ir(sm_key_t ir){}
void sm_test_set_irk(sm_key_t irk){}

void sm_register_oob_data_callback( int (*get_oob_data_callback)(uint8_t addres_type, bd_addr_t addr, uint8_t * oob_data)){}

void sm_set_accepted_stk_generation_methods(uint8_t accepted_stk_generation_methods){}
void sm_set_encryption_key_size_range(uint8_t min_size, uint8_t max_size){}
void sm_set_authentication_requirements(uint8_t auth_req){}
void sm_set_io_capabilities(io_capability_t io_capability){}
void sm_send_security_request(uint16_t handle){}

void sm_bonding_decline(uint8_t addr_type, bd_addr_t address){}
void sm_just_works_confirm(uint8_t addr_type, bd_addr_t address){}
void sm_passkey_input(uint8_t addr_type, bd_addr_t address, uint32_t passkey){}

// @returns 0 if not encrypted, 7-16 otherwise
int sm_encryption_key_size(uint8_t addr_type, bd_addr_t address){
	return 0;
}

// @returns 1 if bonded with OOB/Passkey (AND MITM protection)
int sm_authenticated(uint8_t addr_type, bd_addr_t address){
	return 0;
}

// @returns authorization_state for the current session
authorization_state_t sm_authorization_state(uint8_t addr_type, bd_addr_t address){
	return AUTHORIZATION_DECLINED;
}

// request pairing
void sm_request_pairing(uint8_t addr_type, bd_addr_t address){}

// called by client app on authorization request
void sm_authorization_decline(uint8_t addr_type, bd_addr_t address){}
void sm_authorization_grant(uint8_t addr_type, bd_addr_t address){}

// Support for signed writes
int  sm_cmac_ready(void){
	return 0;
}

void sm_cmac_start(sm_key_t k, uint8_t opcode, uint16_t attribute_handle, uint16_t message_len, uint8_t * message, uint32_t sign_counter, void (*done_handler)(uint8_t hash[8])){};

/**
 * @brief Identify device in LE Device DB
 * @param handle
 * @returns index from le_device_db or -1 if not found/identified
 */
int sm_le_device_index(uint16_t handle ){ 
    return -1;
}


void sm_register_packet_handler(btstack_packet_handler_t handler){
    sm_client_packet_handler = handler;    
}

static void sm_pdu_received_in_wrong_state(void){
    sm_pairing_failed_reason = SM_REASON_UNSPECIFIED_REASON;
    sm_state_responding = SM_GENERAL_SEND_PAIRING_FAILED;
}

static void sm_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){

    if (packet_type != SM_DATA_PACKET) return;

    if (handle != sm_response_handle){
        log_info("sm_packet_handler: packet from handle %u, but expecting from %u", handle, sm_response_handle);
        return;
    }

    if (packet[0] == SM_CODE_PAIRING_FAILED){
        sm_state_responding = SM_GENERAL_SEND_PAIRING_FAILED;
        return;
    }

    switch (sm_state_responding){
        
        case SM_GENERAL_IDLE: {
            if (packet[0] != SM_CODE_PAIRING_REQUEST){
                sm_pdu_received_in_wrong_state();
                break;;
            }
           	sm_state_responding = SM_GENERAL_SEND_PAIRING_FAILED;
            sm_pairing_failed_reason = SM_REASON_PAIRING_NOT_SUPPORTED;
            break;
        }
        default:
        	break;
    }

    // try to send preparared packet
    sm_run();
}

static void sm_event_packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){

    switch (packet_type) {
            
		case HCI_EVENT_PACKET:
			switch (packet[0]) {
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            // only single connection for peripheral
                            if (sm_response_handle){
                                log_info("Already connected, ignoring incoming connection");
                                return;
                            }
                            sm_response_handle = READ_BT_16(packet, 4);
                            sm_state_responding = SM_GENERAL_IDLE;
                            break;

                        case HCI_SUBEVENT_LE_LONG_TERM_KEY_REQUEST:
                            log_info("LTK Request: state %u", sm_state_responding);
                            sm_state_responding = SM_RESPONDER_PH0_SEND_LTK_REQUESTED_NEGATIVE_REPLY;
                            break;

                        default:
                            break;
                    }
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    sm_state_responding = SM_GENERAL_IDLE;
                    sm_response_handle = 0;
                    break;
			}                    

            // forward packet to higher layer
            if (sm_client_packet_handler){
                sm_client_packet_handler(packet_type, 0, packet, size);
            }
	}

    // try to send preparared packet
    sm_run();
}

static void sm_run(void){

    // assert that we can send either one
    switch (sm_state_responding){
        case SM_RESPONDER_PH0_SEND_LTK_REQUESTED_NEGATIVE_REPLY:
            if (!hci_can_send_command_packet_now()) return;
            hci_send_cmd(&hci_le_long_term_key_negative_reply, sm_response_handle);
            sm_state_responding = SM_GENERAL_IDLE;
            return;
        case SM_GENERAL_SEND_PAIRING_FAILED: {
            if (!l2cap_can_send_fixed_channel_packet_now(sm_response_handle)) return;
            uint8_t buffer[2];
            buffer[0] = SM_CODE_PAIRING_FAILED;
            buffer[1] = sm_pairing_failed_reason;
            l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
            sm_state_responding = SM_GENERAL_IDLE;
            break;
        }
       	default:
    		break;
    }
}

void sm_init(void){
    // attach to lower layers
    l2cap_register_fixed_channel(sm_packet_handler, L2CAP_CID_SECURITY_MANAGER_PROTOCOL);
    l2cap_register_packet_handler(sm_event_packet_handler);
}

// GAP LE
void gap_random_address_set_mode(gap_random_address_type_t random_address_type){}
void gap_random_address_set_update_period(int period_ms){}
/*
 * @brief Set Advertisement Paramters
 * @param adv_int_min
 * @param adv_int_max
 * @param adv_type
 * @param direct_address_type
 * @param direct_address
 * @param channel_map
 * @param filter_policy
 *
 * @note own_address_type is used from gap_random_address_set_mode
 */
void gap_advertisements_set_params(uint16_t adv_int_min, uint16_t adv_int_max, uint8_t adv_type,
    uint8_t direct_address_typ, bd_addr_t direct_address, uint8_t channel_map, uint8_t filter_policy){
    hci_le_advertisements_set_params(adv_int_min, adv_int_max, adv_type, gap_random_adress_type,
        direct_address_typ, direct_address, channel_map, filter_policy);
}
