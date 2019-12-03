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

// TODO: only include on posix platform
#include <unistd.h>

#include "btstack.h"
#include "btp.h"
#include "btp_socket.h"

#define AUTOPTS_SOCKET_NAME "/tmp/bt-stack-tester"

#define BT_LE_AD_LIMITED  (1U << 0)
#define BT_LE_AD_GENERAL  (1U << 1)
#define BT_LE_AD_NO_BREDR (1U << 2)

#define LIM_DISC_SCAN_MIN_MS 10000

//#define TEST_POWER_CYCLE

int btstack_main(int argc, const char * argv[]);

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

static bool gap_send_powered_state;
static char gap_name[249];
static char gap_short_name[11];
static uint32_t gap_cod;
static uint8_t gap_discovery_flags;
static btstack_timer_source_t gap_limited_discovery_timer;

static bd_addr_type_t   remote_addr_type;
static bd_addr_t        remote_addr;
static hci_con_handle_t remote_handle = HCI_CON_HANDLE_INVALID;

// gap_adv_data_len/gap_scan_response is 16-bit to simplify bounds calculation
static uint8_t  ad_flags;
static uint8_t  gap_adv_data[31];
static uint16_t gap_adv_data_len;
static uint8_t  gap_scan_response[31];
static uint16_t gap_scan_response_len;
static uint8_t gap_inquriy_scan_active;

static uint32_t current_settings;

static bd_addr_t pts_addr = { 0x00, 0x1b, 0xdc, 0x07, 0x32, 0xef};

// log/debug output
#define MESSAGE(format, ...) log_info(format, ## __VA_ARGS__); printf(format "\n", ## __VA_ARGS__)
static void MESSAGE_HEXDUMP(const uint8_t * data, uint16_t len){
    log_info_hexdump(data, len);
    printf_hexdump(data, len);
}

static void btp_send(uint8_t service_id, uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data){
    if (opcode >= 0x80){
        MESSAGE("Event: service id 0x%02x, opcode 0x%02x, controller_index 0x%0x, len %u", service_id, opcode, controller_index, length);
        if (length > 0) {
            MESSAGE_HEXDUMP(data, length);
        }
    } else {
        MESSAGE("Response: service id 0x%02x, opcode 0x%02x, controller_index 0x%0x, len %u", service_id, opcode, controller_index, length);
        if (length > 0){
            MESSAGE_HEXDUMP(data, length);
        }
    }
    btp_socket_send_packet(service_id, opcode, controller_index, length, data);
} 

static void btp_send_gap_settings(uint8_t opcode){
    MESSAGE("BTP_GAP_SETTINGS opcode %02x: %08x", opcode, current_settings);
    uint8_t buffer[4];
    little_endian_store_32(buffer, 0, current_settings);
    btp_send(BTP_SERVICE_ID_GAP, opcode, 0, 4, buffer);
}

static void btp_send_error(uint8_t service_id, uint8_t error){
    MESSAGE("BTP_GAP_ERROR error %02x", error);
    btp_send(service_id, BTP_OP_ERROR, 0, 1, &error);
}

static void btp_send_error_unknown_command(uint8_t service_id){
    btp_send_error(service_id, BTP_ERROR_UNKNOWN_CMD);
}

static void reset_gap(void){
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

    // TODO: check ENABLE_CLASSIC / Controller features
    // ad_flags = BT_LE_AD_NO_BREDR;
    ad_flags = 0;
}

static void btstack_packet_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    switch (packet_type) {
        case HCI_EVENT_PACKET:
            switch (hci_event_packet_get_type(packet)) {
                case BTSTACK_EVENT_STATE:
                    switch (btstack_event_state_get_state(packet)){
                        case HCI_STATE_WORKING:
                            if (gap_send_powered_state){
                                gap_send_powered_state = false;
                                current_settings |= BTP_GAP_SETTING_POWERED;
                                btp_send_gap_settings(BTP_GAP_OP_SET_POWERED);
                            }
#ifdef TEST_POWER_CYCLE
                            hci_power_control(HCI_POWER_OFF);
#endif
                            break;
                        case HCI_STATE_OFF:
                            if (gap_send_powered_state){
                                gap_send_powered_state = false;
                                // update settings
                                current_settings &= ~BTP_GAP_SETTING_ADVERTISING;
                                current_settings &= ~BTP_GAP_SETTING_POWERED;
                                btp_send_gap_settings(BTP_GAP_OP_SET_POWERED);
                            }
#ifdef TEST_POWER_CYCLE
                            hci_power_control(HCI_POWER_ON);
#endif
                            break;
                        default:
                            break;
                    }
                    break;

                case GAP_EVENT_ADVERTISING_REPORT:{
                    bd_addr_t address;
                    gap_event_advertising_report_get_address(packet, address);
                    uint8_t event_type = gap_event_advertising_report_get_advertising_event_type(packet);
                    uint8_t address_type = gap_event_advertising_report_get_address_type(packet);
                    int8_t rssi = gap_event_advertising_report_get_rssi(packet);
                    uint8_t length = gap_event_advertising_report_get_data_length(packet);
                    const uint8_t * data = gap_event_advertising_report_get_data(packet);

                    // filter during discovery (not doing observation)
                    if ((gap_discovery_flags & BTP_GAP_DISCOVERY_FLAG_OBSERVATION) == 0){
                        bool discoverable = false;
                        ad_context_t context;
                        for (ad_iterator_init(&context, length, data) ; ad_iterator_has_more(&context) ; ad_iterator_next(&context)){
                            uint8_t data_type    = ad_iterator_get_data_type(&context);
                            if (data_type != BLUETOOTH_DATA_TYPE_FLAGS) continue;
                            uint8_t data_size    = ad_iterator_get_data_len(&context);
                            if (data_size < 1) continue;
                            const uint8_t * data = ad_iterator_get_data(&context);
                            uint8_t flags = data[0];
                            if ((gap_discovery_flags & BTP_GAP_DISCOVERY_FLAG_LIMITED) != 0){
                                // only report ad for devices in limited discovery mode
                                if ((flags & BT_LE_AD_LIMITED) != 0){
                                    discoverable = true;
                                }
                            } else {
                                // report all devices in discovery mode
                                if ((flags & (BT_LE_AD_GENERAL | BT_LE_AD_LIMITED)) != 0){
                                    discoverable = true;
                                }
                            }
                        }
                        if (discoverable == false) {
                            printf("Skip non-discoverable advertisement\n");
                            break;
                        }
                    }

                    printf("Advertisement event: evt-type %u, addr-type %u, addr %s, rssi %d, data[%u] ", event_type,
                       address_type, bd_addr_to_str(address), rssi, length);
                    printf_hexdump(data, length);

                    // max 255 bytes EIR data
                    uint8_t buffer[11 + 255];
                    buffer[0] = address_type;
                    reverse_bd_addr(address, &buffer[1]);
                    buffer[7] = rssi;
                    buffer[8] = BTP_GAP_EV_DEVICE_FOUND_FLAG_RSSI | BTP_GAP_EV_DEVICE_FOUND_FLAG_AD | BTP_GAP_EV_DEVICE_FOUND_FLAG_SR;
                    little_endian_store_16(buffer, 9, 0);
                    // TODO: deliver AD/SD if needed
                    btp_send(BTP_SERVICE_ID_GAP, BTP_GAP_EV_DEVICE_FOUND, 0, 11, &buffer[0]);
                    break;
                }

                case GAP_EVENT_INQUIRY_RESULT: {
                    bd_addr_t address;
                    gap_event_inquiry_result_get_bd_addr(packet, address);
                    // TODO: fetch EIR if needed
                    // TODO: fetch RSSI if needed
                    int8_t rssi = 0;

                    // max 255 bytes EIR data
                    uint8_t buffer[11 + 255];
                    buffer[0] = BTP_GAP_ADDR_PUBLIC;
                    reverse_bd_addr(address, &buffer[1]);
                    buffer[7] = rssi;
                    buffer[8] = 0;
                    little_endian_store_16(buffer, 9, 0);
                    btp_send(BTP_SERVICE_ID_GAP, BTP_GAP_EV_DEVICE_FOUND, 0, 11, &buffer[0]);
                    break;
                }

                case HCI_EVENT_CONNECTION_COMPLETE: {
                    // assume remote device
                    remote_handle          = hci_event_connection_complete_get_connection_handle(packet);
                    remote_addr_type       = 0;
                    hci_event_connection_complete_get_bd_addr(packet, remote_addr);
                    printf("Connected Classic to %s with con handle 0x%04x\n", bd_addr_to_str(remote_addr), remote_handle);

                    uint8_t buffer[13];
                    buffer[0] =  remote_addr_type;
                    reverse_bd_addr(remote_addr, &buffer[1]);
                    little_endian_store_16(buffer, 7,  0);
                    little_endian_store_16(buffer, 9,  0);
                    little_endian_store_16(buffer, 11, 0);
                    btp_send(BTP_SERVICE_ID_GAP, BTP_GAP_EV_DEVICE_CONNECTED, 0, sizeof(buffer), &buffer[0]);
                    break;
                }

                case HCI_EVENT_DISCONNECTION_COMPLETE: {
                    // assume remote device
                    printf("Disconnected\n");
                    uint8_t buffer[7];
                    buffer[0] =  remote_addr_type;
                    reverse_bd_addr(remote_addr, &buffer[1]);
                    btp_send(BTP_SERVICE_ID_GAP, BTP_GAP_EV_DEVICE_DISCONNECTED, 0, sizeof(buffer), &buffer[0]);
                    break;
                }

                case HCI_EVENT_LE_META:
                    // wait for connection complete
                    switch (hci_event_le_meta_get_subevent_code(packet)){
                        case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                            // Assume success
                            remote_handle          = hci_subevent_le_connection_complete_get_connection_handle(packet);
                            remote_addr_type       = hci_subevent_le_connection_complete_get_peer_address_type(packet);
                            hci_subevent_le_connection_complete_get_peer_address(packet, remote_addr);
                            uint16_t conn_interval = hci_subevent_le_connection_complete_get_conn_interval(packet);
                            uint16_t conn_latency  = hci_subevent_le_connection_complete_get_conn_latency(packet);
                            uint16_t supervision_timeout = hci_subevent_le_connection_complete_get_supervision_timeout(packet);

                            printf("Connected LE to %s with con handle 0x%04x\n", bd_addr_to_str(remote_addr), remote_handle);

                            uint8_t buffer[13];
                            buffer[0] =  remote_addr_type;
                            reverse_bd_addr(remote_addr, &buffer[1]);
                            little_endian_store_16(buffer, 7, conn_interval);
                            little_endian_store_16(buffer, 9, conn_latency);
                            little_endian_store_16(buffer, 11, supervision_timeout);
                            btp_send(BTP_SERVICE_ID_GAP, BTP_GAP_EV_DEVICE_CONNECTED, 0, sizeof(buffer), &buffer[0]);
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
static void sm_packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    uint8_t buffer[11];
    uint32_t passkey;

    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case SM_EVENT_JUST_WORKS_REQUEST:
            MESSAGE("Just works requested - auto-accept\n");
             sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            MESSAGE("Confirming numeric comparison: %"PRIu32" - auto-accept\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            passkey = sm_event_passkey_display_number_get_passkey(packet);
            MESSAGE("Display Passkey: %"PRIu32"\n", passkey);
            buffer[0] = remote_addr_type;
            reverse_bd_addr(remote_addr, &buffer[1]);
            little_endian_store_32(buffer, 7, passkey);
            btp_send(BTP_SERVICE_ID_GAP, BTP_GAP_EV_PASSKEY_DISPLAY, 0, 11, &buffer[0]);
            break;
        case SM_EVENT_PASSKEY_INPUT_NUMBER:
            MESSAGE("Passkey Input requested\n");
            // MESSAGE("Sending fixed passkey %"PRIu32"\n", FIXED_PASSKEY);
            // sm_passkey_input(sm_event_passkey_input_number_get_handle(packet), FIXED_PASSKEY);
            break;
        case SM_EVENT_PAIRING_COMPLETE:
            switch (sm_event_pairing_complete_get_status(packet)){
                case ERROR_CODE_SUCCESS:
                    MESSAGE("Pairing complete, success\n");
                    break;
                case ERROR_CODE_CONNECTION_TIMEOUT:
                    MESSAGE("Pairing failed, timeout\n");
                    break;
                case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                    MESSAGE("Pairing faileed, disconnected\n");
                    break;
                case ERROR_CODE_AUTHENTICATION_FAILURE:
                    MESSAGE("Pairing failed, reason = %u\n", sm_event_pairing_complete_get_reason(packet));
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void gap_limited_discovery_timeout_handler(btstack_timer_source_t * ts){
    UNUSED(ts);
    if ((gap_discovery_flags & BTP_GAP_DISCOVERY_FLAG_LIMITED) != 0){
        MESSAGE("Limited Discovery Stopped");
        gap_discovery_flags = 0;
        gap_stop_scan();
    }
}

static void btp_core_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data){
    uint8_t status;
    switch (opcode){
        case BTP_OP_ERROR:
            MESSAGE("BTP_OP_ERROR");
            status = data[0];
            if (status == BTP_ERROR_NOT_READY){
                // connection stopped, abort
                // exit(10);
            }
            break;
        case BTP_CORE_OP_READ_SUPPORTED_COMMANDS:
            MESSAGE("BTP_CORE_OP_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER){
                uint8_t commands = 0;
                commands |= (1U << BTP_CORE_OP_READ_SUPPORTED_COMMANDS);
                commands |= (1U << BTP_CORE_OP_READ_SUPPORTED_SERVICES);
                commands |= (1U << BTP_CORE_OP_REGISTER);
                commands |= (1U << BTP_CORE_OP_UNREGISTER);
                btp_send(BTP_SERVICE_ID_CORE, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_CORE_OP_READ_SUPPORTED_SERVICES:
            MESSAGE("BTP_CORE_OP_READ_SUPPORTED_SERVICES");
            if (controller_index == BTP_INDEX_NON_CONTROLLER){
                uint8_t services = 0;
                services |= (1U << BTP_SERVICE_ID_CORE);
                services |= (1U << BTP_SERVICE_ID_GAP);
                services |= (1U << BTP_SERVICE_ID_GATT );
                services |= (1U << BTP_SERVICE_ID_L2CAP);
                services |= (1U << BTP_SERVICE_ID_MESH );
                btp_send(BTP_SERVICE_ID_CORE, opcode, controller_index, 1, &services);
            }
            break;
        case BTP_CORE_OP_REGISTER:
            MESSAGE("BTP_CORE_OP_REGISTER");
            if (controller_index == BTP_INDEX_NON_CONTROLLER){
                btp_send(BTP_SERVICE_ID_CORE, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_CORE_OP_UNREGISTER:
            MESSAGE("BTP_CORE_OP_UNREGISTER");
            if (controller_index == BTP_INDEX_NON_CONTROLLER){
                btp_send(BTP_SERVICE_ID_CORE, opcode, controller_index, 0, NULL);
            }
            break;
        default:
            btp_send_error_unknown_command(BTP_SERVICE_ID_CORE);
            break;
    }
}

static void btp_gap_handler(uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data){
    switch (opcode){
        case BTP_OP_ERROR:
            MESSAGE("BTP_OP_ERROR");
            break;
        case BTP_GAP_OP_READ_SUPPORTED_COMMANDS:
            MESSAGE("BTP_GAP_OP_READ_SUPPORTED_COMMANDS");
            if (controller_index == BTP_INDEX_NON_CONTROLLER){
                uint8_t commands = 0;
                btp_send(BTP_SERVICE_ID_GAP, opcode, controller_index, 1, &commands);
            }
            break;
        case BTP_GAP_OP_READ_CONTROLLER_INDEX_LIST:
            MESSAGE("BTP_GAP_OP_READ_CONTROLLER_INDEX_LIST - not implemented");
            break;
        case BTP_GAP_OP_READ_CONTROLLER_INFO:
            MESSAGE("BTP_GAP_OP_READ_CONTROLLER_INFO");
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
                btp_send(BTP_SERVICE_ID_GAP, opcode, controller_index, 277, buffer);
            }
            break;
        case BTP_GAP_OP_RESET:
            MESSAGE("BTP_GAP_OP_RESET - NOP");
            // ignore
            btp_send_gap_settings(opcode);
            break;
        case BTP_GAP_OP_SET_POWERED:
            MESSAGE("BTP_GAP_OP_SET_POWERED");
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
            MESSAGE("BTP_GAP_OP_SET_CONNECTABLE - NOP");
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
        case BTP_GAP_OP_SET_FAST_CONNECTABLE:
            MESSAGE("BTP_GAP_OP_SET_FAST_CONNECTABLE - NOP");
            if (controller_index == 0){
                uint8_t connectable = data[0];
                if (connectable) {
                    current_settings |= BTP_GAP_SETTING_FAST_CONNECTABLE;
                } else {
                    current_settings &= ~BTP_GAP_SETTING_FAST_CONNECTABLE;

                }
                btp_send_gap_settings(opcode);
            }
            break;
        case BTP_GAP_OP_SET_DISCOVERABLE:
            MESSAGE("BTP_GAP_OP_SET_DISCOVERABLE");
            if (controller_index == 0){
                uint8_t discoverable = data[0];
                // Classic
                gap_discoverable_control(discoverable > 0);

                switch (discoverable) {
                    case BTP_GAP_DISCOVERABLE_NON:
                        ad_flags &= ~(BT_LE_AD_GENERAL | BT_LE_AD_LIMITED);
                        current_settings &= ~BTP_GAP_SETTING_DISCOVERABLE;
                        break;
                    case BTP_GAP_DISCOVERABLE_GENERAL:
                        ad_flags &= ~BT_LE_AD_LIMITED;
                        ad_flags |= BT_LE_AD_GENERAL;
                        current_settings |= BTP_GAP_SETTING_DISCOVERABLE;
                        break;
                    case BTP_GAP_DISCOVERABLE_LIMITED:
                        ad_flags &= ~BT_LE_AD_GENERAL;
                        ad_flags |= BT_LE_AD_LIMITED;
                        current_settings |= BTP_GAP_SETTING_DISCOVERABLE;
                        break;
                    default:
                        return;
                }
                btp_send_gap_settings(opcode);
            }
            break;
        case BTP_GAP_OP_SET_BONDABLE:
            MESSAGE("BTP_GAP_OP_SET_BONDABLE");
            if (controller_index == 0){
                uint8_t bondable = data[0];
                // TODO: Classic
                if (bondable) {
                    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
                    current_settings |= BTP_GAP_SETTING_BONDABLE;
                } else {
                    sm_set_authentication_requirements(SM_AUTHREQ_NO_BONDING);
                    current_settings &= ~BTP_GAP_SETTING_BONDABLE;
                }
                btp_send_gap_settings(opcode);
            }
            break;

        case BTP_GAP_OP_START_DIRECTED_ADVERTISING:
            MESSAGE("BTP_GAP_OP_START_DIRECTED_ADVERTISING");
            if (controller_index == 0){
                remote_addr_type = data[0];
                reverse_bd_addr(&data[1], remote_addr);
                uint8_t high_duty =  data[7];
                // uint8_t use_own_id_address = data[8];

                uint16_t adv_int_min;
                uint16_t adv_int_max;
                uint8_t adv_type;
                if (high_duty){
                    adv_type = 0x01;
                    adv_int_min = 0;
                    adv_int_max = 0;
                } else {
                    adv_type = 0x04;
                    adv_int_min = 0x800;
                    adv_int_max = 0x800;
                }

                gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, remote_addr_type, remote_addr, 0x07, 0x00);
                gap_advertisements_enable(1);
                // update settings
                current_settings |= BTP_GAP_SETTING_ADVERTISING;
                btp_send_gap_settings(opcode);
            }
            break;

        case BTP_GAP_OP_START_ADVERTISING:
            MESSAGE("BTP_GAP_OP_START_ADVERTISING");
            if (controller_index == 0){
                uint8_t adv_data_len = data[0];
                uint8_t scan_response_len = data[1];
                const uint8_t * adv_data = &data[2];
                const uint8_t * scan_response = &data[2 + adv_data_len];
                // uint32_t duration = little_endian_read_32(data, 2 + adv_data_len + scan_response_len);
                uint8_t own_addr_type = data[6 + adv_data_len + scan_response_len];

                // prefix adv_data with flags and append rest
                gap_adv_data_len = 0;
                gap_adv_data[gap_adv_data_len++] = 0x02;
                gap_adv_data[gap_adv_data_len++] = 0x01;
                gap_adv_data[gap_adv_data_len++] = ad_flags;

                uint8_t ad_pos = 0;
                while ((ad_pos + 2) < adv_data_len){
                    uint8_t ad_type = adv_data[ad_pos++];
                    uint8_t ad_len  = adv_data[ad_pos++];
                    if ((ad_type != BLUETOOTH_DATA_TYPE_FLAGS) && ((ad_pos + ad_len) <= adv_data_len) && (gap_adv_data_len + 2 + ad_len < 31)) {
                        gap_adv_data[gap_adv_data_len++] = ad_len + 1;
                        gap_adv_data[gap_adv_data_len++] = ad_type;
                        memcpy(&gap_adv_data[gap_adv_data_len], &adv_data[ad_pos], ad_len);
                        gap_adv_data_len += ad_len;
                    }
                    ad_pos += ad_len;
                }

                // process scan data
                uint8_t scan_pos = 0;
                gap_scan_response_len = 0;
                while ((scan_pos + 2) < scan_response_len){
                    uint8_t ad_type = scan_response[scan_pos++];
                    uint8_t ad_len  = scan_response[scan_pos++];
                    if (((scan_pos + ad_len) < scan_response_len) && (gap_scan_response_len + 2 + ad_len < 31)) {
                        gap_scan_response[gap_scan_response_len++] = ad_len + 1;
                        gap_scan_response[gap_scan_response_len++] = ad_type;
                        memcpy(&gap_scan_response[gap_scan_response_len], &scan_response[scan_pos], ad_len);
                        gap_scan_response_len += ad_len;
                    }
                    scan_pos += ad_len;
                }

                // configure controller
                switch (own_addr_type){
                    case BTP_GAP_OWN_ADDR_TYPE_IDENTITY:
                        gap_random_address_set_mode(GAP_RANDOM_ADDRESS_TYPE_OFF);
                        break;
                    case BTP_GAP_OWN_ADDR_TYPE_RPA:
                        gap_random_address_set_mode(GAP_RANDOM_ADDRESS_RESOLVABLE);
                        break;
                    case BTP_GAP_OWN_ADDR_TYPE_NON_RPA:
                        gap_random_address_set_mode(GAP_RANDOM_ADDRESS_NON_RESOLVABLE);
                        break;
                    default:
                        break;
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

                gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, pts_addr, 0x07, 0x00);
                gap_advertisements_set_data(gap_adv_data_len, (uint8_t *) gap_adv_data);
                gap_scan_response_set_data(scan_response_len, (uint8_t *) scan_response);
                gap_advertisements_enable(1);
                // update settings
                current_settings |= BTP_GAP_SETTING_ADVERTISING;
                btp_send_gap_settings(opcode);
            }
            break;
        case BTP_GAP_OP_STOP_ADVERTISING:
            MESSAGE("BTP_GAP_OP_STOP_ADVERTISING");
            if (controller_index == 0){
                gap_advertisements_enable(0);
                // update settings
                current_settings &= ~BTP_GAP_SETTING_ADVERTISING;
                btp_send_gap_settings(opcode);
            }
            break;
        case BTP_GAP_OP_START_DISCOVERY:
            MESSAGE("BTP_GAP_OP_START_DISCOVERY");
            if (controller_index == 0){
                uint8_t flags = data[0];
                if ((flags & BTP_GAP_DISCOVERY_FLAG_LE) != 0){
                    uint8_t scan_type;
                    if ((flags & BTP_GAP_DISCOVERY_FLAG_ACTIVE) != 0){
                        scan_type = 1;
                    } else {
                        scan_type = 0;
                    }
                    if (flags & BTP_GAP_DISCOVERY_FLAG_LIMITED){
                        // set timer
                        btstack_run_loop_set_timer_handler(&gap_limited_discovery_timer, gap_limited_discovery_timeout_handler);
                        btstack_run_loop_set_timer(&gap_limited_discovery_timer, LIM_DISC_SCAN_MIN_MS);
                        btstack_run_loop_add_timer(&gap_limited_discovery_timer);
                    }
                    gap_discovery_flags = flags;
                    gap_set_scan_parameters(scan_type, 0x30, 0x30);
                    gap_start_scan();
                }
                if (flags & BTP_GAP_DISCOVERY_FLAG_BREDR){
                    gap_inquriy_scan_active = 1;
                    gap_inquiry_start(15);
                }
                btp_send(BTP_SERVICE_ID_GAP, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_GAP_OP_STOP_DISCOVERY:
            MESSAGE("BTP_GAP_OP_STOP_DISCOVERY");
            if (controller_index == 0){
                gap_stop_scan();
                if (gap_inquriy_scan_active){
                    gap_inquriy_scan_active = 0;
                    gap_inquiry_stop();
                }
                btp_send(BTP_SERVICE_ID_GAP, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_GAP_OP_CONNECT:
            MESSAGE("BTP_GAP_OP_CONNECT");
            if (controller_index == 0){
                remote_addr_type = data[0];
                reverse_bd_addr(&data[1], remote_addr);
                // uint8_t own_addr_type = data[7];
                gap_auto_connection_start(remote_addr_type, remote_addr);
                btp_send(BTP_SERVICE_ID_GAP, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_GAP_OP_DISCONNECT:
            MESSAGE("BTP_GAP_OP_DISCONNECT");
            if (controller_index == 0){
                if (remote_handle != HCI_CON_HANDLE_INVALID){
                    gap_disconnect(remote_handle);
                    remote_handle = HCI_CON_HANDLE_INVALID;
                }
                btp_send(BTP_SERVICE_ID_GAP, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_GAP_OP_SET_IO_CAPA:
            MESSAGE("BTP_GAP_OP_SET_IO_CAPA");
            if (controller_index == 0){
                uint8_t io_capabilities = data[0];
                gap_ssp_set_io_capability(io_capabilities);
                sm_set_io_capabilities( (io_capability_t) io_capabilities);
                btp_send(BTP_SERVICE_ID_GAP, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_GAP_OP_PAIR:
            MESSAGE("BTP_GAP_OP_PAIR");
            if (controller_index == 0){
                // assumption, already connected
                sm_request_pairing(remote_handle);
                btp_send(BTP_SERVICE_ID_GAP, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_GAP_OP_UNPAIR:{
            MESSAGE("BTP_GAP_OP_UNPAIR");
            if (controller_index == 0){
                uint8_t   command_addr_type = data[0];
                bd_addr_t command_addr;
                reverse_bd_addr(&data[1], command_addr);
                gap_drop_link_key_for_bd_addr(command_addr);
                uint16_t index;
                for (index =0 ; index < le_device_db_max_count(); index++){
                    int addr_type;
                    bd_addr_t addr;
                    sm_key_t irk;
                    memset(addr, 0, 6);
                    le_device_db_info(index, &addr_type, addr, NULL);
                    if (bd_addr_cmp(command_addr, addr) == 0){
                        le_device_db_remove(index);
                    }
                }
                btp_send(BTP_SERVICE_ID_GAP, opcode, controller_index, 0, NULL);
            }
            break;
        }
        case BTP_GAP_OP_PASSKEY_ENTRY_RSP:
            if (controller_index == 0){
                // assume already connected
                uint8_t   command_addr_type = data[0];
                bd_addr_t command_addr;
                reverse_bd_addr(&data[1], command_addr);
                uint32_t passkey = little_endian_read_32(data, 7);
                MESSAGE("BTP_GAP_OP_PASSKEY_ENTRY_RSP, passkey %06u", passkey);
                sm_passkey_input(remote_handle, passkey);
                btp_send(BTP_SERVICE_ID_GAP, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_GAP_OP_PASSKEY_CONFIRM_RSP:
            if (controller_index == 0){
                // assume already connected
                uint8_t   command_addr_type = data[0];
                bd_addr_t command_addr;
                reverse_bd_addr(&data[1], command_addr);
                uint8_t match = data[7];
                MESSAGE("BTP_GAP_OP_PASSKEY_CONFIRM_RSP - match %u", match);
                if (match){
                    // note: sm_numeric_comparison_confirm == sm_just_works_confirm for nwo
                    sm_just_works_confirm(remote_handle);
                } else {
                    sm_bonding_decline(remote_handle);
                }
                btp_send(BTP_SERVICE_ID_GAP, opcode, controller_index, 0, NULL);
            }
            break;
        case BTP_GAP_OP_CONNECTION_PARAM_UPDATE:
            if (controller_index == 0){
                MESSAGE("BTP_GAP_OP_CONNECTION_PARAM_UPDATE");
                // assume already connected
                uint8_t   command_addr_type = data[0];
                bd_addr_t command_addr;
                reverse_bd_addr(&data[1], command_addr);
                uint16_t conn_interval_min = little_endian_read_16(data, 7);
                uint16_t conn_interval_max = little_endian_read_16(data, 9);
                uint16_t conn_latency = little_endian_read_16(data, 11);
                uint16_t supervision_timeout = little_endian_read_16(data, 13);
                gap_request_connection_parameter_update(remote_handle, conn_interval_min, conn_interval_max, conn_latency, supervision_timeout);
                btp_send(BTP_SERVICE_ID_GAP, opcode, controller_index, 0, NULL);
            }
            break;
        default:
            btp_send_error_unknown_command(BTP_SERVICE_ID_GAP);
            break;
    }
}

static void btp_packet_handler(uint8_t service_id, uint8_t opcode, uint8_t controller_index, uint16_t length, const uint8_t *data){
    MESSAGE("Command: service id 0x%02x, opcode 0x%02x, controller_index 0x%0x, len %u", service_id, opcode, controller_index, length);
    if (length > 0){
        MESSAGE_HEXDUMP(data, length);
    }

    switch (service_id){
        case BTP_SERVICE_ID_CORE:
            btp_core_handler(opcode, controller_index, length, data);
            break;
        case BTP_SERVICE_ID_GAP:
            btp_gap_handler(opcode, controller_index, length, data);
            break;
        default:
            btp_send_error_unknown_command(service_id);
            break;
    }
}

enum console_state {
    CONSOLE_STATE_MAIN = 0,
    CONSOLE_STATE_GAP,
} console_state;

static void usage(void){
    switch (console_state){
        case CONSOLE_STATE_MAIN:
            printf("BTstack BTP Client for auto-pts framework: MAIN console interface\n");
            printf("g - GAP Command\n");
            break;
        case CONSOLE_STATE_GAP:
            printf("BTstack BTP Client for auto-pts framework: GAP console interface\n");
            printf("s - Start active scanning\n");
            printf("S - Stop discovery and scanning\n");
            printf("g - Start general discovery\n");
            printf("l - Start limited discovery\n");
            printf("i - Start inquiry scan\n");
            printf("p - Power On\n");
            printf("P - Power Off\n");
            printf("x - Back to main\n");
            printf("c - enable connectable mode\n");
            printf("d - enable general discoverable mode\n");
            printf("D - enable limited discoverable mode\n");
            printf("B - enable bondable mode\n");
            printf("a - start advertising with public address\n");
            printf("A - start directed advertising with public address to %s\n", bd_addr_to_str(pts_addr));
            printf("C - connect to public address to %s\n", bd_addr_to_str(pts_addr));
            printf("r - start advertising with resolvable random address\n");
            printf("n - start advertising with resolvable random address\n");
            printf("L - send L2CAP Conn Param Update Request\n");
            printf("t - disconnect\n");
            printf("b - start pairing\n");
            break;
        default:
            break;
    }
}
static void stdin_process(char cmd){
    const uint8_t inquiry_scan = BTP_GAP_DISCOVERY_FLAG_BREDR;
    const uint8_t active_le_scan = BTP_GAP_DISCOVERY_FLAG_LE | BTP_GAP_DISCOVERY_FLAG_ACTIVE;
    const uint8_t limited_le_scan = BTP_GAP_DISCOVERY_FLAG_LE | BTP_GAP_DISCOVERY_FLAG_LIMITED;
    const uint8_t general_le_scan = BTP_GAP_DISCOVERY_FLAG_LE;
    const uint8_t general_discoverable = BTP_GAP_DISCOVERABLE_GENERAL;
    const uint8_t limited_discoverable = BTP_GAP_DISCOVERABLE_LIMITED;
    const uint8_t value_on  = 1;
    const uint8_t value_off = 0;
    const uint8_t public_adv[]  = { 0x08, 0x00, 0x08, 0x06, 'T', 'e', 's', 't', 'e', 'r', 0xff, 0xff, 0xff, 0xff, 0x00, };
    const uint8_t rpa_adv[]     = { 0x08, 0x00, 0x08, 0x06, 'T', 'e', 's', 't', 'e', 'r', 0xff, 0xff, 0xff, 0xff, 0x02, };
    const uint8_t non_rpa_adv[] = { 0x08, 0x00, 0x08, 0x06, 'T', 'e', 's', 't', 'e', 'r', 0xff, 0xff, 0xff, 0xff, 0x03, };
    uint8_t directed_advertisement_request[8];
    uint8_t connection_request[7];
    uint8_t conn_param_update[15];

    switch (console_state){
        case CONSOLE_STATE_MAIN:
            switch (cmd){
                case 'g':
                    console_state = CONSOLE_STATE_GAP;
                    usage();
                    break;
                default:
                    usage();
                    break;
            }
            break;
        case CONSOLE_STATE_GAP:
            switch (cmd){
                case 'p':
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_SET_POWERED, 0, 1, &value_on);
                    break;
                case 'P':
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_SET_POWERED, 0, 1, &value_off);
                    break;
                case 'c':
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_SET_CONNECTABLE, 0, 1, &value_on);
                    break;
                case 'i':
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_START_DISCOVERY, 0, 1, &inquiry_scan);
                    break;
                case 's':
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_START_DISCOVERY, 0, 1, &active_le_scan);
                    break;
                case 'S':
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_STOP_DISCOVERY, 0, 0, NULL);
                    break;
                case 'l':
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_START_DISCOVERY, 0, 1, &limited_le_scan);
                    break;
                case 'g':
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_START_DISCOVERY, 0, 1, &general_le_scan);
                    break;
                case 'a':
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_START_ADVERTISING, 0, sizeof(public_adv), public_adv);
                    break;
                case 'r':
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_START_ADVERTISING, 0, sizeof(rpa_adv), rpa_adv);
                    break;
                case 'n':
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_START_ADVERTISING, 0, sizeof(non_rpa_adv), non_rpa_adv);
                    break;
                case 'd':
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_SET_DISCOVERABLE, 0, 1, &general_discoverable);
                    break;
                case 'D':
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_SET_DISCOVERABLE, 0, 1, &limited_discoverable);
                    break;
                case 'A':
                    directed_advertisement_request[0] = 0;
                    reverse_bd_addr(pts_addr, &directed_advertisement_request[1]);
                    directed_advertisement_request[7] = 1; // 0 low duty - 1 high duty
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_START_DIRECTED_ADVERTISING, 0, sizeof(directed_advertisement_request), directed_advertisement_request);
                    break;
                case 'C':
                    connection_request[0] = 0;
                    reverse_bd_addr(pts_addr, &connection_request[1]);
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_CONNECT, 0, sizeof(connection_request), connection_request);
                    break;
                case 't':
                    connection_request[0] = 0;
                    reverse_bd_addr(pts_addr, &connection_request[1]);
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_DISCONNECT, 0, sizeof(connection_request), connection_request);
                    break;
                case 'L':
                    conn_param_update[0] = 0;
                    reverse_bd_addr(pts_addr, &conn_param_update[1]);
                    little_endian_store_16(conn_param_update, 7, 0x32);
                    little_endian_store_16(conn_param_update, 9, 0x46);
                    little_endian_store_16(conn_param_update, 11, 0x01);
                    little_endian_store_16(conn_param_update, 13, 0x1f4);
                    btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_CONNECTION_PARAM_UPDATE, 0, sizeof(conn_param_update), conn_param_update);
                    break;
                case 'b':
                      btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_PAIR, 0, 0, NULL);
                    break;
                case 'B':
                      btp_packet_handler(BTP_SERVICE_ID_GAP, BTP_GAP_OP_SET_BONDABLE, 0, 1, &value_on);
                    break;
                case 'x':
                    console_state = CONSOLE_STATE_MAIN;
                    usage();
                    break;
                default:
                    usage();
                    break;
            }
            break;
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

    // use fixed IRK
    uint8_t test_irk[16] = { 0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF,0x01,0x23,0x45,0x67,0x89,0xAB,0xCD,0xEF };
    sm_test_set_irk(test_irk);
    printf("TEST IRK: "); 
    printf_hexdump(test_irk, 16);

    // register for HCI events
    hci_event_callback_registration.callback = &btstack_packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    // register for SM events
    sm_event_callback_registration.callback = &sm_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // configure GAP
    gap_ssp_set_io_capability(SSP_IO_CAPABILITY_DISPLAY_YES_NO);

    strcpy(gap_name, "iut 00:00:00:00:00:00");
    gap_set_local_name(gap_name);

    strcpy(gap_short_name, "iut");

    gap_cod = 0x007a020c;   // smartphone
    gap_set_class_of_device(gap_cod);

    // configure GAP LE
    sm_set_authentication_requirements(SM_AUTHREQ_NO_BONDING);
    
    reset_gap();

    MESSAGE("auto-pts iut-btp-client started");

    // connect to auto-pts client 
    btp_socket_open_unix(AUTOPTS_SOCKET_NAME);
    btp_socket_register_packet_handler(&btp_packet_handler);

    MESSAGE("BTP_CORE_SERVICE/BTP_EV_CORE_READY/BTP_INDEX_NON_CONTROLLER()");
    btp_send(BTP_SERVICE_ID_CORE, BTP_CORE_EV_READY, BTP_INDEX_NON_CONTROLLER, 0, NULL);

#ifdef HAVE_POSIX_FILE_IO
#ifdef HAVE_BTSTACK_STDIN
    if (isatty(fileno(stdin))){
        btstack_stdin_setup(stdin_process);
        usage();
    } else {
        MESSAGE("Terminal not interactive");
    }
#endif
#endif

#ifdef TEST_POWER_CYCLE
    hci_power_control(HCI_POWER_ON);
#endif

    return 0;
}
