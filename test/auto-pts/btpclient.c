/*
 * Copyright (C) 2019 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btpclient.c"

/*
 * btpclient.c
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
 
#include "btstack.h"
#include "btp.h"
#include "btp_socket.h"

#define AUTOPTS_SOCKET_NAME "/tmp/bt-stack-tester"

int btstack_main(int argc, const char * argv[]);


static btstack_packet_callback_registration_t hci_event_callback_registration;
static bool gap_send_powered_state;
static char gap_name[249];
static char gap_short_name[11];
static uint32_t gap_cod;
static uint8_t gap_adv_data[31];
static uint8_t gap_adv_data_len;

static uint32_t current_settings;


static void btp_send_gap_settings(uint8_t opcode){
    log_info("BTP_GAP_SETTINGS opcode %02x: %08x", opcode, current_settings);
    uint8_t buffer[4];
    little_endian_store_32(buffer, 0, current_settings);
    btp_socket_send_packet(BTP_SERVICE_ID_GAP, opcode, 0, 4, buffer);
}

static void btstack_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
	switch (packet_type) {
		case HCI_EVENT_PACKET:
			switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                    log_info("BTSTACK_EVENT_STATE %x, gap_send_powered_state %u", btstack_event_state_get_state(packet), gap_send_powered_state);
                    switch (btstack_event_state_get_state(packet)){
                        case HCI_STATE_WORKING:
                            if (gap_send_powered_state){
                                gap_send_powered_state = false;
                                current_settings |= BTP_GAP_SETTING_POWERED;
                                btp_send_gap_settings(BTP_GAP_OP_SET_POWERED);
                            }
                            break;
                        case HCI_STATE_OFF:
                            if (gap_send_powered_state){
                                gap_send_powered_state = false;
                                // update settings
                                current_settings &= ~BTP_GAP_SETTING_ADVERTISING;
                                current_settings &= ~BTP_GAP_SETTING_POWERED;
                                btp_send_gap_settings(BTP_GAP_OP_SET_POWERED);
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    break;
			}
            break;
        default:
            break;
	}
}

static void btp_core_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data){
    uint8_t status;
    switch (opcode){
        case BTP_OP_ERROR:
            log_info("BTP_OP_ERROR");
            status = data[0];
            if (status == BTP_ERROR_NOT_READY){
                // connection stopped, abort
                exit(10);
            }
            break;
        case BTP_CORE_OP_READ_SUPPORTED_COMMANDS:
            log_info("BTP_CORE_OP_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER){
                uint8_t commands = 0;
                commands |= (1U << BTP_CORE_OP_READ_SUPPORTED_COMMANDS);
                commands |= (1U << BTP_CORE_OP_READ_SUPPORTED_SERVICES);
                commands |= (1U << BTP_CORE_OP_REGISTER);
                commands |= (1U << BTP_CORE_OP_UNREGISTER);
                btp_socket_send_packet(BTP_SERVICE_ID_CORE, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_CORE_OP_READ_SUPPORTED_SERVICES:
            log_info("BTP_CORE_OP_READ_SUPPORTED_SERVICES");
            if (controller_index == BTP_INDEX_NON_CONTROLLER){
                uint8_t services = 0;
                services |= (1U << BTP_SERVICE_ID_CORE);
                services |= (1U << BTP_SERVICE_ID_GAP);
                services |= (1U << BTP_SERVICE_ID_GATT );
                services |= (1U << BTP_SERVICE_ID_L2CAP);
                services |= (1U << BTP_SERVICE_ID_MESH );
                btp_socket_send_packet(BTP_SERVICE_ID_CORE, opcode, controller_index, 1, &services);
            }
            break;
        case BTP_CORE_OP_REGISTER:
            log_info("BTP_CORE_OP_REGISTER");
            if (controller_index == BTP_INDEX_NON_CONTROLLER){
                btp_socket_send_packet(BTP_SERVICE_ID_CORE, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_CORE_OP_UNREGISTER:
            log_info("BTP_CORE_OP_UNREGISTER");
            if (controller_index == BTP_INDEX_NON_CONTROLLER){
                btp_socket_send_packet(BTP_SERVICE_ID_CORE, opcode, controller_index, 0, NULL);
            }
            break;
        default:
            break;
    }
}

static void btp_gap_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data){
    switch (opcode){
        case BTP_OP_ERROR:
            log_info("BTP_OP_ERROR");
            break;
        case BTP_GAP_OP_READ_SUPPORTED_COMMANDS:
            log_info("BTP_GAP_OP_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER){
                uint8_t commands = 0;
                btp_socket_send_packet(BTP_SERVICE_ID_GAP, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_GAP_OP_READ_CONTROLLER_INDEX_LIST:
            log_info("BTP_GAP_OP_READ_CONTROLLER_INDEX_LIST - not implemented");
            break;
        case BTP_GAP_OP_READ_CONTROLLER_INFO:
            log_info("BTP_GAP_OP_READ_CONTROLLER_INFO");
            if (controller_index == 0){
                uint8_t buffer[277];
                bd_addr_t local_addr;
                gap_local_bd_addr( local_addr);
                reverse_bd_addr(local_addr, &buffer[0]);
                uint32_t supported_settings = 0;
                supported_settings |=  BTP_GAP_SETTING_POWERED;
                supported_settings |=  BTP_GAP_SETTING_CONNECTABLE;
                supported_settings |=  BTP_GAP_SETTING_DISCOVERABLE;
                supported_settings |=  BTP_GAP_SETTING_BONDABLE;
                supported_settings |=  BTP_GAP_SETTING_SSP;
    #ifdef ENABLE_CLASSIC
                supported_settings |=  BTP_GAP_SETTING_BREDR;
    #endif
    #ifdef ENABLE_BLE
                supported_settings |=  BTP_GAP_SETTING_LE;
    #endif
                supported_settings |=  BTP_GAP_SETTING_ADVERTISING;
    #ifdef ENABLE_LE_SECURE_CONNECTIONS
                supported_settings |=  BTP_GAP_SETTING_SC;
    #endif
                supported_settings |=  BTP_GAP_SETTING_PRIVACY;
                // supported_settings |=  BTP_GAP_SETTING_STATIC_ADDRESS;
                little_endian_store_32(buffer, 6, supported_settings);
                little_endian_store_32(buffer,10, current_settings);
                little_endian_store_24(buffer, 14, gap_cod);
                strncpy((char *) &buffer[17],  &gap_name[0],       249);
                strncpy((char *) &buffer[266], &gap_short_name[0], 11 );
                btp_socket_send_packet(BTP_SERVICE_ID_GAP, opcode, controller_index, 277, buffer);
            }
            break;
        case BTP_GAP_OP_RESET:
            log_info("BTP_GAP_OP_RESET - NOP");
            // ignore
            btp_send_gap_settings(opcode);
            break;
        case BTP_GAP_OP_SET_POWERED:
            log_info("BTP_GAP_OP_SET_POWERED");
            if (controller_index == 0){
                gap_send_powered_state = true;
                uint8_t powered = data[0];
                if (powered){
                    hci_power_control(HCI_POWER_ON);
                } else {
                    hci_power_control(HCI_POWER_OFF);
                }
            }
            break;
        case BTP_GAP_OP_SET_CONNECTABLE:
            log_info("BTP_GAP_OP_SET_POWERED");
            if (controller_index == 0){
                uint8_t connectable = data[0];
                if (connectable) {
                    current_settings |= BTP_GAP_SETTING_CONNECTABLE;
                } else {
                    current_settings &= ~BTP_GAP_SETTING_CONNECTABLE;

                }
                btp_send_gap_settings(opcode);
            }
            break;
        case BTP_GAP_OP_START_ADVERTISING:
            log_info("BTP_GAP_OP_START_ADVERTISING");
            if (controller_index == 0){
                uint8_t adv_data_len = data[0];
                uint8_t scan_response_len = data[1];
                const uint8_t * adv_data = &data[2];
                const uint8_t * scan_response = &data[2 + adv_data_len];
                // uint32_t duration = little_endian_read_32(data, 2 + adv_data_len + scan_response_len);
                bool use_own_id_address = (bool) &data[6 + adv_data_len + scan_response_len];

                // prefix adv_data with flags
                gap_adv_data[0] = 0x02;
                gap_adv_data[1] = 0x01;
                gap_adv_data[2] = 0x04;
                memcpy(&gap_adv_data[3], adv_data, adv_data_len);
                gap_adv_data_len = 3 + adv_data_len;

                // configure controller
                if (use_own_id_address){
                    gap_random_address_set_mode(GAP_RANDOM_ADDRESS_TYPE_OFF);
                } else {
                    gap_random_address_set_mode(GAP_RANDOM_ADDRESS_RESOLVABLE);
                }
                uint16_t adv_int_min = 0x0030;
                uint16_t adv_int_max = 0x0030;
                uint8_t adv_type;
                bd_addr_t null_addr;
                memset(null_addr, 0, 6);
                if (current_settings & BTP_GAP_SETTING_CONNECTABLE){
                    adv_type = 0;   // ADV_IND
                } else {
                    // min advertising interval 100 ms for non-connectable advertisements (pre 5.0 controllers)
                    adv_type = 3;   // ADV_NONCONN_IND
                    adv_int_min = 0xa0;
                    adv_int_max = 0xa0;
                }
                gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
                gap_advertisements_set_data(gap_adv_data_len, (uint8_t *) gap_adv_data);
                gap_scan_response_set_data(scan_response_len, (uint8_t *) scan_response);
                gap_advertisements_enable(1);
                // update settings
                current_settings |= BTP_GAP_SETTING_ADVERTISING;
                btp_send_gap_settings(opcode);
            }
            break;
        default:
            break;
    }
}

static void btp_packet_handler(uint8_t service_id, uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data){
    log_info("service id 0x%02x, opcode 0x%02x, controller_index 0x%0x, len %u", service_id, opcode, controller_index, length);
    log_info_hexdump(data, length);

    switch (service_id){
        case BTP_SERVICE_ID_CORE:
            btp_core_handler(opcode, controller_index, length, data);
            break;
        case BTP_SERVICE_ID_GAP:
            btp_gap_handler(opcode, controller_index, length, data);
        default:
            break;
    }
}

int btstack_main(int argc, const char * argv[])
{
    (void)argc;
    (void)argv;

    l2cap_init();
    rfcomm_init();
    sdp_init();

    le_device_db_init();
    sm_init();

    // register for HCI events
    hci_event_callback_registration.callback = &btstack_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // configure GAP
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);

    strcpy(gap_name, "iut 00:00:00:00:00:00");
    gap_set_local_name(gap_name);

    strcpy(gap_short_name, "iut");

    gap_cod = 0x007a020c;   // smartphone
    gap_set_class_of_device(gap_cod);

    btp_socket_register_packet_handler(&btp_packet_handler);
    printf("auto-pts iut-btp-client started\n");

    // current settings
    current_settings |=  BTP_GAP_SETTING_SSP;
    current_settings |=  BTP_GAP_SETTING_LE;
    current_settings |=  BTP_GAP_SETTING_PRIVACY;
#ifdef ENABLE_CLASSIC
    current_settings |=  BTP_GAP_SETTING_BREDR;
#endif
#ifdef ENABLE_BLE
#endif
#ifdef ENABLE_LE_SECURE_CONNECTIONS
    current_settings |=  BTP_GAP_SETTING_SC;
#endif

    // connect to auto-pts client 
    btp_socket_open_unix(AUTOPTS_SOCKET_NAME);
    log_info("BTP_CORE_SERVICE/BTP_EV_CORE_READY/BTP_INDEX_NON_CONTROLLER()");
    btp_socket_send_packet(BTP_SERVICE_ID_CORE, BTP_CORE_EV_READY, BTP_INDEX_NON_CONTROLLER, 0, NULL);
	    
    return 0;
}
