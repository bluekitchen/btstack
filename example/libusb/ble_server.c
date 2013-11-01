/*
 * Copyright (C) 2011-2012 by Matthias Ringwald
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
 * 4. This software may not be used in a commercial product
 *    without an explicit license granted by the copyright holder. 
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
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
 */

//*****************************************************************************
//
// att device demo
//
//*****************************************************************************

// TODO: seperate BR/EDR from LE ACL buffers
// TODO: move LE init into HCI
// ..

// NOTE: Supports only a single connection

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#include <btstack/run_loop.h>
#include "debug.h"
#include "btstack_memory.h"
#include "hci.h"
#include "hci_dump.h"

#include "l2cap.h"

#include "att.h"

#include "rijndael.h"

typedef enum {
    SM_CODE_PAIRING_REQUEST = 0X01,
    SM_CODE_PAIRING_RESPONSE,
    SM_CODE_PAIRING_CONFIRM,
    SM_CODE_PAIRING_RANDOM,
    SM_CODE_PAIRING_FAILED,
    SM_CODE_ENCRYPTION_INFORMATION,
    SM_CODE_MASTER_IDENTIFICATION,
    SM_CODE_IDENTITY_INFORMATION,
    SM_CODE_IDENTITY_ADDRESS_INFORMATION,
    SM_CODE_SIGNING_INFORMATION,
    SM_CODE_SECURITY_REQUEST
} SECURITY_MANAGER_COMMANDS;

// Authentication requirement flags
#define SM_AUTHREQ_NO_BONDING 0x00
#define SM_AUTHREQ_BONDING 0x01
#define SM_AUTHREQ_MITM_PROTECTION 0x02

typedef uint8_t key_t[16];

static att_connection_t att_connection;
static uint16_t         att_response_handle = 0;
static uint16_t         att_response_size   = 0;
static uint8_t          att_response_buffer[28];

static uint16_t         sm_response_handle = 0;
static uint16_t         sm_response_size   = 0;
static uint8_t          sm_response_buffer[28];

static int sm_send_security_request = 0;
static int sm_send_encryption_information = 0;
static int sm_send_master_identification = 0;
static int sm_send_identity_information = 0;
static int sm_send_identity_address_information = 0;
static int sm_send_signing_identification = 0;

static int sm_received_encryption_information = 0;
static int sm_received_master_identification = 0;
static int sm_received_identity_information = 0;
static int sm_received_identity_address_information = 0;
static int sm_received_signing_identification = 0;

static key_t   sm_tk;
static key_t   sm_m_random;
static key_t   sm_m_confirm;
static uint8_t sm_preq[7];
static uint8_t sm_pres[7];

// key distribution, slave sends
static key_t     sm_s_ltk;
static uint16_t  sm_s_ediv;
static uint8_t   sm_s_rand[8];
static uint8_t   sm_s_addr_type;
static bd_addr_t sm_s_address;
static key_t     sm_s_csrk;
static key_t     sm_s_irk;

// key distribution, recevied from master
static key_t     sm_m_ltk;
static uint16_t  sm_m_ediv;
static uint8_t   sm_m_rand[8];
static uint8_t   sm_m_addr_type;
static bd_addr_t sm_m_address;
static key_t     sm_m_csrk;
static key_t     sm_m_irk;

static void att_try_respond(void){
    if (!att_response_size) return;
    if (!att_response_handle) return;
    if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)) return;
    
    // update state before sending packet
    uint16_t size = att_response_size;
    att_response_size = 0;
    l2cap_send_connectionless(att_response_handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, att_response_buffer, size);
}

static void att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
    if (packet_type != ATT_DATA_PACKET) return;
    
    att_response_handle = handle;
    att_response_size = att_handle_request(&att_connection, packet, size, att_response_buffer);
    att_try_respond();
}

// SECURITY MANAGER (SM) MATERIALIZES HERE

static void sm_validate(void){
}

static inline void swap128(uint8_t src[16], uint8_t dst[16]){
    int i;
    for (i = 0; i < 16; i++)
        dst[15 - i] = src[i];
}

static inline void swap56(uint8_t src[7], uint8_t dst[7]){
    int i;
    for (i = 0; i < 7; i++)
        dst[6 - i] = src[i];
}

static void sm_s1(key_t k, key_t r1, key_t r2, key_t s1){
    key_t r_prime;
    memcpy(&r_prime[0], r2, 8);
    memcpy(&r_prime[8], r1, 8);
    key_t r_flipped;
    swap128(r_prime, r_flipped);
    printf("r': "); hexdump(r_flipped, 16);
    
    key_t tk_flipped;
    swap128(sm_tk, tk_flipped);
    printf("tk' "); hexdump(tk_flipped, 16);
    
    // setup aes decryption
    unsigned long rk[RKLENGTH(KEYBITS)];
    int nrounds = rijndaelSetupEncrypt(rk, &tk_flipped[0], KEYBITS);
    
    key_t s1_flipped;
    rijndaelEncrypt(rk, nrounds, r_flipped, s1_flipped);

    printf("s1' "); hexdump(s1_flipped, 16);
    
    swap128(s1_flipped, s1);
    printf("s1: "); hexdump(s1, 16);
}

static void sm_run(void){
    if (!hci_can_send_packet_now(HCI_ACL_DATA_PACKET)) return;

    // send security manager packet
    if (sm_response_size){
        uint16_t size = sm_response_size;
        sm_response_size = 0;
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) sm_response_buffer, size);
    }
    // send security request
    if (sm_send_security_request){
        sm_send_security_request = 0;
        uint8_t buffer[2];
        buffer[0] = SM_CODE_SECURITY_REQUEST;
        buffer[1] = SM_AUTHREQ_BONDING;
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
        return;
    }
    if (sm_send_encryption_information){
        sm_send_encryption_information = 0;
        uint8_t buffer[17];
        buffer[0] = SM_CODE_ENCRYPTION_INFORMATION;
        memcpy(&buffer[1], sm_s_ltk, 16);
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
        return;
    }
    if (sm_send_master_identification){
        sm_send_master_identification = 0;
        uint8_t buffer[11];
        buffer[0] = SM_CODE_MASTER_IDENTIFICATION;
        bt_store_16(buffer, 1, sm_s_ediv);
        memcpy(&buffer[3],sm_s_rand,8); 
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
        return;
    }
    if (sm_send_identity_information){
        sm_send_identity_information = 0;
        uint8_t buffer[17];
        buffer[0] = SM_CODE_IDENTITY_INFORMATION;
        memcpy(&buffer[1], sm_s_irk, 16);
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
        return;
    }
    if (sm_send_identity_address_information ){
        sm_send_identity_address_information = 0;
        uint8_t buffer[8];
        buffer[0] = SM_CODE_IDENTITY_ADDRESS_INFORMATION;
        buffer[1] = sm_s_addr_type;
        BD_ADDR_COPY(&buffer[2], sm_s_address);
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
        return;
    }
    if (sm_send_signing_identification){
        sm_send_signing_identification = 0;
        uint8_t buffer[17];
        buffer[0] = SM_CODE_SIGNING_INFORMATION;
        memcpy(&buffer[1], sm_s_csrk, 16);
        l2cap_send_connectionless(sm_response_handle, L2CAP_CID_SECURITY_MANAGER_PROTOCOL, (uint8_t*) buffer, sizeof(buffer));
        return;
    }
}

static void sm_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){

    if (packet_type != SM_DATA_PACKET) return;

    printf("sm_packet_handler, request %0x\n", packet[0]);
    
    switch (packet[0]){
        case SM_CODE_PAIRING_REQUEST:
            // for validate
            memcpy(sm_preq, packet, 7);
            
            memcpy(sm_response_buffer, packet, size);        
            sm_response_buffer[0] = SM_CODE_PAIRING_RESPONSE;
            // sm_response_buffer[1] = 0x00;   // io capability: DisplayOnly
            // sm_response_buffer[1] = 0x02;   // io capability: KeyboardOnly
            // sm_response_buffer[1] = 0x03;   // io capability: NoInputNoOutput
            sm_response_buffer[1] = 0x04;   // io capability: KeyboardDisplay
            sm_response_buffer[2] = 0x00;   // no oob data available
            sm_response_buffer[3] = sm_response_buffer[3] & 3;  // remove MITM flag
            sm_response_buffer[4] = 0x10;   // maxium encryption key size
            sm_response_size = 7;

            // for validate
            memcpy(sm_pres, sm_response_buffer, 7);
            break;
        case  SM_CODE_PAIRING_CONFIRM:
            // received confirm value
            memcpy(sm_m_confirm, &packet[1], 16);
            sm_response_size = 17;

            // dummy
            memcpy(sm_response_buffer, packet, size);        
            break;
        case SM_CODE_PAIRING_RANDOM:
            // received confirm value
            memcpy(sm_m_random, &packet[1], 16);
            sm_response_size = 17;

            // validate 
            sm_validate();
            
            //
            memcpy(sm_response_buffer, packet, size);        
            break;   

        case SM_CODE_ENCRYPTION_INFORMATION:
            sm_received_encryption_information = 1;
            memcpy(sm_m_ltk, &packet[1], 16);
            break;

        case SM_CODE_MASTER_IDENTIFICATION:
            sm_received_master_identification = 1;
            sm_m_ediv = READ_BT_16(packet, 1);
            memcpy(sm_m_rand, &packet[3],8);
            break;

        case SM_CODE_IDENTITY_INFORMATION:
            sm_received_identity_information = 1;
            memcpy(sm_m_irk, &packet[1], 16);
            break;

        case SM_CODE_IDENTITY_ADDRESS_INFORMATION:
            sm_received_identity_address_information = 1;
            sm_m_addr_type = packet[1];
            BD_ADDR_COPY(sm_m_address, &packet[2]); 
            break;

        case SM_CODE_SIGNING_INFORMATION:
            sm_received_signing_identification = 1;
            memcpy(sm_m_csrk, &packet[1], 16);
            break;
    }

    // try to send preparared packet
    sm_run();
}

// END OF SM

// enable LE, setup ADV data
static void packet_handler (void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    uint8_t adv_data[] = { 02, 01, 05,   03, 02, 0xf0, 0xff }; 

    sm_run();

    switch (packet_type) {
            
		case HCI_EVENT_PACKET:
			switch (packet[0]) {
				
                case BTSTACK_EVENT_STATE:
					// bt stack activated, get started - set local name
					if (packet[2] == HCI_STATE_WORKING) {
                        printf("Working!\n");
                        hci_send_cmd(&hci_le_set_advertising_data, sizeof(adv_data), adv_data);
					}
					break;
                
                case DAEMON_EVENT_HCI_PACKET_SENT:
                    att_try_respond();
                    break;
                    
                case HCI_EVENT_LE_META:
                    switch (packet[2]) {
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            sm_response_handle = READ_BT_16(packet, 4);
                            
                            // request security
                            // sm_send_security_request = 1;

                            // reset connection MTU
                            att_connection.mtu = 23;
                            break;

                        case HCI_SUBEVENT_LE_LONG_TERM_KEY_REQUEST:
                            sm_s1(sm_tk, sm_m_random, sm_m_random, sm_s_ltk);
                            hci_send_cmd(&hci_le_long_term_key_request_reply, READ_BT_16(packet, 3), sm_s_ltk);
                            break;

                        default:
                            break;
                    }
                    break;
                case HCI_EVENT_ENCRYPTION_CHANGE:
                
                    // TODO: check state: 
                
                    // TODO: only send requested keys

                    // distribute keys
                    sm_send_encryption_information = 1;
                    sm_send_master_identification = 1;
                    sm_send_identity_information = 1;
                    sm_send_identity_address_information = 1;
                    sm_send_signing_identification = 1;                
                    break;

                case HCI_EVENT_DISCONNECTION_COMPLETE:
                    att_response_handle =0;
                    att_response_size = 0;
                    
                    // restart advertising
                    hci_send_cmd(&hci_le_set_advertise_enable, 1);
                    break;
                    
				case HCI_EVENT_COMMAND_COMPLETE:
				    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_advertising_parameters)){
                        // only needed for BLE Peripheral
                        hci_send_cmd(&hci_le_set_advertising_data, sizeof(adv_data), adv_data);
                        break;
					}
				    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_advertising_data)){
                        // only needed for BLE Peripheral
					   hci_send_cmd(&hci_le_set_scan_response_data, 10, adv_data);
					   break;
					}
				    if (COMMAND_COMPLETE_EVENT(packet, hci_le_set_scan_response_data)){
                        // only needed for BLE Peripheral
					   hci_send_cmd(&hci_le_set_advertise_enable, 1);
					   break;
					}
			}
	}

    sm_run();
}

// test profile
#include "profile.h"

// write requests
static void att_write_callback(uint16_t handle, uint16_t transaction_mode, uint16_t offset, uint8_t *buffer, uint16_t buffer_size, signature_t * signature){
    printf("WRITE Callback, handle %04x\n", handle);
    switch(handle){
        case 0x000b:
            buffer[buffer_size]=0;
            printf("New text: %s\n", buffer);
            break;
        case 0x000d:
            printf("New value: %u\n", buffer[0]);
            break;
    }
}

void setup(void){
    /// GET STARTED with BTstack ///
    btstack_memory_init();
    run_loop_init(RUN_LOOP_POSIX);
        
    // use logger: format HCI_DUMP_PACKETLOGGER, HCI_DUMP_BLUEZ or HCI_DUMP_STDOUT
    hci_dump_open("/tmp/hci_dump.pklg", HCI_DUMP_PACKETLOGGER);

    // init HCI
    hci_transport_t    * transport = hci_transport_usb_instance();
    hci_uart_config_t  * config    = NULL;
    bt_control_t       * control   = NULL;
    remote_device_db_t * remote_db = (remote_device_db_t *) &remote_device_db_memory;
        
    hci_init(transport, config, control, remote_db);

    // set up l2cap_le
    l2cap_init();
    l2cap_register_fixed_channel(att_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);
    l2cap_register_fixed_channel(sm_packet_handler, L2CAP_CID_SECURITY_MANAGER_PROTOCOL);
    l2cap_register_packet_handler(packet_handler);
    
    // set up ATT
    att_set_db(profile_data);
    att_set_write_callback(att_write_callback);
    att_dump_attributes();
    att_connection.mtu = 27;
}

// main == setup
int main(void)
{
    setup();

    // turn on!
    hci_power_control(HCI_POWER_ON);
    
    // go!
    run_loop_execute(); 
    
    // happy compiler!
    return 0;
}
