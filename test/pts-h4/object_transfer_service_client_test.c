/*
 * Copyright (C) 2024 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "object_transfer_service_client_test.c"


#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "btstack.h"

#define CBM_RECEIVE_BUFFER_LEN 300

typedef struct advertising_report {
    uint8_t   type;
    uint8_t   event_type;
    uint8_t   address_type;
    bd_addr_t address;
    uint8_t   rssi;
    uint8_t   length;
    const uint8_t * data;
} advertising_report_t;

static enum {
    APP_STATE_IDLE,
    APP_STATE_W4_SCAN_RESULT,
    APP_STATE_W4_CONNECT,
    APP_STATE_W4_CHANGE_SERVER_CONNECT,
    APP_STATE_CONNECTED
} app_state;

static ots_client_connection_t ots_connection;

static advertising_report_t report;
static btstack_packet_callback_registration_t sm_event_callback_registration;
static hci_con_handle_t connection_handle;
static uint16_t ots_cid;


static bd_addr_t public_pts_address = {0xC0, 0x07, 0xE8, 0x41, 0x45, 0x91};
static int       public_pts_address_type = 0;
static bd_addr_t current_pts_address;
static int       current_pts_address_type;

static char * ots_object_name_short = "Short name";
static char * ots_object_name_long  = "OTS long object name that exceeds the MTU size for this test example";
static btstack_utc_t first_created = {2023, 6, 22, 10, 59, 30};
static btstack_utc_t last_modified = {2023, 6, 22, 10, 59, 30};
static uint8_t filter_data_starts_with[] = {0x4F, 0x62, 0x6A};
static uint8_t filter_data_ends_with[] = {0x65, 0x63, 0x74, 0x20, 0x32};
static uint8_t filter_data_contains[] = {0x65, 0x63, 0x74};
static uint8_t filter_data_name_is_exactly[] = {0x4F, 0x62, 0x6A, 0x65, 0x63, 0x74, 0x20, 0x32};

static uint8_t filter_data_long[]  = {0x4F, 0x54, 0x53, 0x20, 0x6C, 0x6F, 0x6E, 0x67, 0x20, 0x6F, 0x62,
                                    0x6A, 0x65, 0x63, 0x74, 0x20, 0x6E, 0x61, 0x6D, 0x65, 0x20, 0x74};
static ots_object_id_t object_id = {};
static olcp_list_sort_order_t sort_order = OLCP_LIST_SORT_ORDER_BY_FIRST_NAME_ASCENDING;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t gatt_client_service_changed_callback_registration;

static uint8_t  cbm_receive_buffer[CBM_RECEIVE_BUFFER_LEN];
static uint32_t cbm_object_size;
static uint32_t cbm_object_read_chunk_offset = 0;
static uint32_t cbm_object_read_chunk_length = 0;
static uint32_t cbm_object_read_chunk_transferred = 0;

static void show_usage(void);
static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void gatt_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

static void object_transfer_service_client_setup(void){
    // Init L2CAP
    l2cap_init();

    // GATT Client setup
    gatt_client_init();
    gatt_client_service_changed_callback_registration.callback = gatt_client_event_handler;
    gatt_client_add_service_changed_handler(&gatt_client_service_changed_callback_registration);
\
    object_transfer_service_client_init();

    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_SECURE_CONNECTION | SM_AUTHREQ_BONDING);
    sm_event_callback_registration.callback = &hci_event_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);
} 

static void dump_advertising_report(uint8_t *packet){
    bd_addr_t address;
    gap_event_advertising_report_get_address(packet, address);

    printf("    * adv. event: evt-type %u, addr-type %u, addr %s, rssi %u, length adv %u, data: ", 
        gap_event_advertising_report_get_advertising_event_type(packet),
        gap_event_advertising_report_get_address_type(packet), 
        bd_addr_to_str(address), 
        gap_event_advertising_report_get_rssi(packet), 
        gap_event_advertising_report_get_data_length(packet));
    printf_hexdump(gap_event_advertising_report_get_data(packet), gap_event_advertising_report_get_data_length(packet));
    
}

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    bd_addr_t address;

    if (packet_type != HCI_EVENT_PACKET){
        return;  
    }

    bd_addr_t addr;
    switch (hci_event_packet_get_type(packet)) {

        case SM_EVENT_JUST_WORKS_REQUEST:
            printf("Just Works requested\n");
            sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            printf("Confirming numeric comparison: %"PRIu32"\n", sm_event_numeric_comparison_request_get_passkey(packet));
            sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            printf("Display Passkey: %"PRIu32"\n", sm_event_passkey_display_number_get_passkey(packet));
            break;
        case SM_EVENT_IDENTITY_CREATED:
            sm_event_identity_created_get_identity_address(packet, addr);
            printf("Identity created: type %u address %s\n", sm_event_identity_created_get_identity_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_IDENTITY_RESOLVING_SUCCEEDED:
            sm_event_identity_resolving_succeeded_get_identity_address(packet, addr);
            printf("Identity resolved: type %u address %s\n", sm_event_identity_resolving_succeeded_get_identity_addr_type(packet), bd_addr_to_str(addr));
            break;
        case SM_EVENT_IDENTITY_RESOLVING_FAILED:
            sm_event_identity_created_get_address(packet, addr);
            printf("Identity resolving failed\n");
            break;
        case SM_EVENT_PAIRING_COMPLETE:
            switch (sm_event_pairing_complete_get_status(packet)){
                case ERROR_CODE_SUCCESS:
                    printf("Pairing complete, success\n");
                    break;
                case ERROR_CODE_CONNECTION_TIMEOUT:
                    printf("Pairing failed, timeout\n");
                    break;
                case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                    printf("Pairing faileed, disconnected\n");
                    break;
                case ERROR_CODE_AUTHENTICATION_FAILURE:
                    printf("Pairing failed, reason = %u\n", sm_event_pairing_complete_get_reason(packet));
                    break;
                default:
                    break;
            }
            break;

        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started
            if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) break;
            printf("BTstack activated, start scanning\n");
            // app_state = APP_STATE_W4_SCAN_RESULT;
            // gap_start_scan();
            break;

        case GAP_EVENT_ADVERTISING_REPORT:
            if (app_state != APP_STATE_W4_SCAN_RESULT) return;

            gap_event_advertising_report_get_address(packet, address);
            dump_advertising_report(packet);

            if (memcmp(address, public_pts_address, 6) != 0){
                break;
            }
            // stop scanning, and connect to the device
            app_state = APP_STATE_W4_CONNECT;
            gap_stop_scan();
            printf("Stop scan. Connect to device with addr %s.\n", bd_addr_to_str(report.address));
            gap_connect(report.address,report.address_type);
            break;

        case HCI_EVENT_META_GAP:
            // Wait for connection complete
            if (hci_event_gap_meta_get_subevent_code(packet) !=  GAP_SUBEVENT_LE_CONNECTION_COMPLETE) break;
            
            // Get connection handle from event
            connection_handle = gap_subevent_le_connection_complete_get_connection_handle(packet);
            gap_stop_scan();
            app_state = APP_STATE_CONNECTED;
            printf("GAP connected.\n");
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            connection_handle = HCI_CON_HANDLE_INVALID;

            object_transfer_service_client_disconnect(&ots_connection);
            printf("Disconnected\n");
            break;


        default:
            break;
    }
}

static const char * ots_filter_type2str(ots_filter_type_t filter_type){
    static char * filter_type_names[] = {
        "NO_FILTER",
        "NAME_STARTS_WITH",
        "NAME_ENDS_WITH",
        "NAME_CONTAINS",
        "NAME_IS_EXACTLY",
        "OBJECT_TYPE",
        "CREATED_BETWEEN",
        "MODIFIED_BETWEEN",
        "CURRENT_SIZE_BETWEEN",
        "ALLOCATED_SIZE_BETWEEN",
        "MARKED_OBJECTS",
        "RFU",
    };

    if (filter_type < OTS_FILTER_TYPE_RFU){
        return filter_type_names[(uint8_t) filter_type];
    }
    return filter_type_names[OTS_FILTER_TYPE_RFU];
}

static void gatt_client_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(packet_type);
    UNUSED(channel);
    UNUSED(size);

    uint8_t status;

    if (hci_event_packet_get_type(packet) == GATT_EVENT_SERVICE_CHANGED) {
        printf("GATT_EVENT_SERVICE_CHANGED\n");
        return;
    }

    if (hci_event_packet_get_type(packet) != HCI_EVENT_LEAUDIO_META) {
        return;
    }

    switch (hci_event_leaudio_meta_get_subevent_code(packet)) {
        case LEAUDIO_SUBEVENT_OTS_CLIENT_CONNECTED:
            status = leaudio_subevent_ots_client_connected_get_att_status(packet);

            switch (status) {
                case ERROR_CODE_SUCCESS:
                    app_state = APP_STATE_CONNECTED;
                    printf("OTS Client Test: connected\n");
                    break;

                default:
                    printf("OTS Client Test: connection failed, err 0x%02x.\n", status);
                    gap_disconnect(connection_handle);
                    break;
            }
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_WRITE_DONE:
            status = leaudio_subevent_ots_client_write_done_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("OTS Client Test: Write failed, 0x%02x\n", status);
            } else {
                printf("OTS Client Test: Write successful\n");
            }
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_FEATURES:
            status = leaudio_subevent_ots_client_features_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("OTS Client Test: Read failed, 0x%02x\n", status);
            } else {
                printf("OTS Client Test: OACP 0x%08X, OLCP 0x%08X\n",
                       leaudio_subevent_ots_client_features_get_oacp_features(packet),
                       leaudio_subevent_ots_client_features_get_olcp_features(packet));
            }
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_NAME:
            status = leaudio_subevent_ots_client_object_name_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("OTS Client Test: Read failed, 0x%02x\n", status);
            } else {
                printf("OTS Client Test: Object Name \'%s\'\n",
                       leaudio_subevent_ots_client_object_name_get_value(packet));
            }
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_TYPE:
            status = leaudio_subevent_ots_client_object_type_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("OTS Client Test: Read failed, 0x%02x\n", status);
            } else {
                printf("OTS Client Test: Object Type\n");
                printf_hexdump(leaudio_subevent_ots_client_object_type_get_value(packet), leaudio_subevent_ots_client_object_type_get_value_len(packet));
            }
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_FIRST_CREATED:
            status = leaudio_subevent_ots_client_object_first_created_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("OTS Client Test: Read failed, 0x%02x\n", status);
            } else {
                printf("OTS Client Test: First created: %d-%d-%d %d:%d:%d\n",
                       leaudio_subevent_ots_client_object_first_created_get_day(packet),
                       leaudio_subevent_ots_client_object_first_created_get_month(packet),
                       leaudio_subevent_ots_client_object_first_created_get_year(packet),
                       leaudio_subevent_ots_client_object_first_created_get_hours(packet),
                       leaudio_subevent_ots_client_object_first_created_get_minutes(packet),
                       leaudio_subevent_ots_client_object_first_created_get_seconds(packet));
            }
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_LAST_MODIFIED:
            status = leaudio_subevent_ots_client_object_last_modified_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("OTS Client Test: Read failed, 0x%02x\n", status);
            } else {
                printf("OTS Client Test: Last Modified: %d-%d-%d %d:%d:%d\n",
                       leaudio_subevent_ots_client_object_last_modified_get_day(packet),
                       leaudio_subevent_ots_client_object_last_modified_get_month(packet),
                       leaudio_subevent_ots_client_object_last_modified_get_year(packet),
                       leaudio_subevent_ots_client_object_last_modified_get_hours(packet),
                       leaudio_subevent_ots_client_object_last_modified_get_minutes(packet),
                       leaudio_subevent_ots_client_object_last_modified_get_seconds(packet));
            }
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_SIZE:
            status = leaudio_subevent_ots_client_object_size_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("OTS Client Test: Read failed, 0x%02x\n", status);
            } else {
                cbm_object_size = leaudio_subevent_ots_client_object_size_get_current_size(packet);
                printf("OTS Client Test: Object allocated size %d, current size %d \n",
                    leaudio_subevent_ots_client_object_size_get_allocated_size(packet), leaudio_subevent_ots_client_object_size_get_current_size(packet));
            }
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_ID:
            status = leaudio_subevent_ots_client_object_id_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("OTS Client Test: Read failed, 0x%02x\n", status);
            } else {
                printf("OTS Client Test: Object ID \n");
                printf_hexdump(leaudio_subevent_ots_client_object_id_get_value(packet), leaudio_subevent_ots_client_object_id_get_value_len(packet));
            }
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_PROPERTIES:
            status = leaudio_subevent_ots_client_object_properties_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("OTS Client Test: Read failed, 0x%02x\n", status);
            } else {
                printf("OTS Client Test: Object Properties 0x%08x\n",
                    leaudio_subevent_ots_client_object_properties_get_bitmask(packet));
            }
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_FILTER:
            status = leaudio_subevent_ots_client_filter_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("OTS Client Test: Read failed, 0x%02x\n", status);
            } else {
                printf("OTS Client Test: Filter-%d %s \n",
                       leaudio_subevent_ots_client_filter_get_index(packet),
                       ots_filter_type2str(leaudio_subevent_ots_client_filter_get_type(packet)));
                printf_hexdump(leaudio_subevent_ots_client_filter_get_value(packet),
                               leaudio_subevent_ots_client_filter_get_value_len(packet));

            }
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_OBJECT_CHANGED:
            status = leaudio_subevent_ots_client_object_changed_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("OTS Client Test: Indication incomplete, 0x%02x\n", status);
            } else {
                printf("OTS Client Test: Flags 0x%04X, Object ID \n",
                       leaudio_subevent_ots_client_object_changed_get_flags(packet));
                printf_hexdump(leaudio_subevent_ots_client_object_changed_get_id(packet), leaudio_subevent_ots_client_object_changed_get_id_len(packet));
            }
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_OLCP_RESPONSE:
            status = leaudio_subevent_ots_client_olcp_response_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("OTS Client Test: OLCP response incomplete, 0x%02x\n", status);
            } else {
                printf("OTS Client Test: OLCP response opcode %d, result %d\n",
                       leaudio_subevent_ots_client_olcp_response_get_opcode(packet),
                       leaudio_subevent_ots_client_olcp_response_get_result_code(packet));
            }
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_OACP_RESPONSE:
            status = leaudio_subevent_ots_client_oacp_response_get_att_status(packet);
            if (status != ATT_ERROR_SUCCESS){
                printf("OTS Client Test: OACP response incomplete, 0x%02x\n", status);
            } else {
                printf("OTS Client Test: OACP response opcode %d, result %d\n",
                       leaudio_subevent_ots_client_oacp_response_get_opcode(packet),
                       leaudio_subevent_ots_client_oacp_response_get_result_code(packet));
            }
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_DATA_CHUNK:
            cbm_object_read_chunk_length = leaudio_subevent_ots_client_data_chunk_get_bytes_transferred_num(packet);
            cbm_object_read_chunk_offset = leaudio_subevent_ots_client_data_chunk_get_offset(packet);
            cbm_object_read_chunk_transferred = leaudio_subevent_ots_client_data_chunk_get_bytes_transferred_num(packet);

            printf("OTS Client Test: Read data [%d] offset %d, chunk length %d, transferred %d \n",
                   leaudio_subevent_ots_client_data_chunk_get_state(packet),
                   cbm_object_read_chunk_offset, cbm_object_read_chunk_length, cbm_object_read_chunk_transferred);

            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_TIMEOUT:
            printf("OTS Client Test: Operation timeout\n");
            break;

        case LEAUDIO_SUBEVENT_OTS_CLIENT_DISCONNECTED:
            printf("OTS Client Test: disconnected\n");
            break;

        default:
            break;
    }
}

static void show_usage(void){
    uint8_t iut_address_type;
    bd_addr_t      iut_address;
    gap_le_get_own_address(&iut_address_type, iut_address);

    printf("--- OTS Client ---\n");
    printf("c    - Connect to %s\n", bd_addr_to_str(current_pts_address));
    printf("C    - Disconnect from %s\n", bd_addr_to_str(current_pts_address));
    printf("Z    - Connect to OTS servert\n");
    printf("d    - Read ots feature\n");
    printf("D    - Read object name\n");
    printf("e    - Read object type\n");
    printf("E    - Read object size\n");
    printf("f    - Read object first created\n");
    printf("F    - Read object last modified\n");
    printf("g    - Read object id\n");
    printf("G    - Read object properties\n");
    printf("h    - Read filter 1\n");
    printf("H    - Read filter 2\n");
    printf("i    - Read filter 3\n");
    printf("I    - Read long object name\n");
    printf("j    - Read long filter 1\n");
    printf("J    - Read long_filter 2\n");
    printf("k    - Read long_filter 3\n");
    printf("K    - Write object name\n");
    printf("l    - Write long object name\n");
    printf("L    - Write first created\n");
    printf("m    - Write last modifies\n");
    printf("M    - Write object properties\n");
    printf("n    - Write filter 1\n");
    printf("N    - Write filter 2\n");
    printf("o    - Write filter 3\n");
    printf("O    - Write long filter 1\n");
    printf("p    - Write long filter 2\n");
    printf("P    - Write long filter 3\n");
    printf("q    - OLCP cmd FIRST\n");
    printf("Q    - OLCP cmd LAST n");
    printf("r    - OLCP cmd PREVIOUS\n");
    printf("R    - OLCP cmd NEXT\n");
    printf("s    - OLCP cmd GOTO\n");
    printf("S    - OLCP cmd ORDER\n");
    printf("t    - OLCP cmd NUMBER OF OBJECTS\n");
    printf("T    - OLCP cmd CLEAR MARKING\n");
    printf("u    - Filter NAME_IS_EXACTLY\n");
    printf("v    - Filter OBJECT_TYPE 7FB1\n");
    printf("V    - Filter CREATED_BETWEEN\n");
    printf("x    - Filter MODIFIED_BETWEEN\n");
    printf("X    - Filter CURRENT_SIZE_BETWEEN\n");
    printf("y    - Filter ALLOCATED_SIZE_BETWEEN\n");
    printf("Y    - Filter MARKED_OBJECTS\n");
    printf("0    - Open Object Channel\n");
    printf("1    - OACP - Read Object \n");
    printf("2    - OACP - Calculate Checksum, chunk length %d\n", cbm_object_read_chunk_transferred);
    printf("3    - OACP - Write (not truncated)\n");
    printf("4    - OACP - Write (truncated)\n");
    printf("5    - OACP - Execute\n");
    printf("6    - OACP - Abort\n");
    printf("7    - OACP - Create\n");
    printf("8    - OACP - Delete\n");
    printf("9    - Close Object Channel\n");
}


static void stdin_process(char c){
    uint8_t status = ERROR_CODE_SUCCESS;

    switch (c){
        case 'c':
            printf("Connect to %s\n", bd_addr_to_str(current_pts_address));
            app_state = APP_STATE_W4_CONNECT;
            status = gap_connect(current_pts_address, current_pts_address_type);
            break;
        case 'C':
            printf("Disconnect OTS client from %s\n", bd_addr_to_str(current_pts_address));
            status = object_transfer_service_client_disconnect(&ots_connection);
            break;
        case 'Z':
            printf("Connect to OTS server\n");
            status = object_transfer_service_client_connect(connection_handle, &gatt_client_event_handler,
                                                          &ots_connection,
                                                          0, 0xFFFF, 0,
                                                          &ots_cid);
            break;
    case 'd':
            printf("Read ots feature\n");
            status = object_transfer_service_client_read_ots_feature(&ots_connection);
            break;

        case 'D':
            printf("Read object name\n");
            status = object_transfer_service_client_read_object_name(&ots_connection);
            break;

        case 'e':
            printf("Read object type\n");
            status = object_transfer_service_client_read_object_type(&ots_connection);
            break;

        case 'E':
            printf("Read object size\n");
            status = object_transfer_service_client_read_object_size(&ots_connection);
            break;

        case 'f':
            printf("Read object first created\n");
            status = object_transfer_service_client_read_object_first_created(&ots_connection);
            break;

        case 'F':
            printf("Read object last modified\n");
            status = object_transfer_service_client_read_object_last_modified(&ots_connection);
            break;

        case 'g':
            printf("Read object id\n");
            status = object_transfer_service_client_read_object_id(&ots_connection);
            break;

        case 'G':
            printf("Read object properties\n");
            status = object_transfer_service_client_read_object_properties(&ots_connection);
            break;

        case 'h':
            printf("Read filter 1\n");
            status = object_transfer_service_client_read_object_list_filter_1(&ots_connection);
            break;

        case 'H':
            printf("Read filter 2\n");
            status = object_transfer_service_client_read_object_list_filter_2(&ots_connection);
            break;

        case 'i':
            printf("Read filter 3\n");
            status = object_transfer_service_client_read_object_list_filter_3(&ots_connection);
            break;

        case 'I':
            printf("\n");
            status = object_transfer_service_client_read_long_object_name(&ots_connection);
            break;

        case 'j':
            printf("Read long filter 1\n");
            status = object_transfer_service_client_read_object_long_list_filter_1(&ots_connection);
            break;

        case 'J':
            printf("Read long filter 2\n");
            status = object_transfer_service_client_read_object_long_list_filter_2(&ots_connection);
            break;

        case 'k':
            printf("Read long filter 3\n");
            status = object_transfer_service_client_read_object_long_list_filter_3(&ots_connection);
            break;

        case 'K':
            printf("Write object name\n");
            status = object_transfer_service_client_write_object_name(&ots_connection, ots_object_name_short);
            break;

        case 'l':
            printf("Write long object name\n");
            status = object_transfer_service_client_write_object_name(&ots_connection, ots_object_name_long);
            break;

        case 'L':
            printf("Write first created\n");
            status = object_transfer_service_client_write_object_first_created(&ots_connection, &first_created);
            break;

        case 'm':
            printf("Write last modifies\n");
            status = object_transfer_service_client_write_object_last_modified(&ots_connection, &last_modified);
            break;

        case 'M':
            printf("Write object properties\n");
            object_transfer_service_client_write_object_properties(&ots_connection, 0xFF);
            break;

        case 'n':
            printf("Write filter 1\n");
            status = object_transfer_service_client_write_object_list_filter_1(&ots_connection, OTS_FILTER_TYPE_NAME_STARTS_WITH, sizeof(filter_data_starts_with), filter_data_starts_with);
            break;

        case 'N':
            printf("Write filter 2\n");
            status = object_transfer_service_client_write_object_list_filter_2(&ots_connection, OTS_FILTER_TYPE_NAME_ENDS_WITH, sizeof(filter_data_ends_with), filter_data_ends_with);
            break;

        case 'o':
            printf("Write filter 3\n");
            status = object_transfer_service_client_write_object_list_filter_3(&ots_connection, OTS_FILTER_TYPE_NAME_CONTAINS, sizeof(filter_data_contains), filter_data_contains);
            break;

        case 'O':
            printf("Write long filter 1\n");
            status = object_transfer_service_client_write_object_list_filter_1(&ots_connection, OTS_FILTER_TYPE_NAME_CONTAINS, sizeof(filter_data_long), filter_data_long);
            break;

        case 'p':
            printf("Write long filter 2\n");
            status = object_transfer_service_client_write_object_list_filter_2(&ots_connection, OTS_FILTER_TYPE_NAME_CONTAINS, sizeof(filter_data_long), filter_data_long);
            break;

        case 'P':
            printf("Write long filter 3\n");
            status = object_transfer_service_client_write_object_list_filter_3(&ots_connection, OTS_FILTER_TYPE_NAME_CONTAINS, sizeof(filter_data_long), filter_data_long);
            break;
        case 'q':
            printf("OLCP cmd FIRST\n");
            status = object_transfer_service_client_command_first(&ots_connection);
            break;

        case 'Q':
            printf("OLCP cmd LAST n");
            status = object_transfer_service_client_command_last(&ots_connection);
            break;

        case 'r':
            printf("OLCP cmd PREVIOUS\n");
            status = object_transfer_service_client_command_previous(&ots_connection);
            break;

        case 'R':
            printf("OLCP cmd NEXT\n");
            status = object_transfer_service_client_command_next(&ots_connection);
            break;

        case 's':
            printf("OLCP cmd GOTO\n");
            status = object_transfer_service_client_command_goto(&ots_connection, &object_id);
            break;

        case 'S':
            printf("OLCP cmd ORDER\n");
            status = object_transfer_service_client_command_order(&ots_connection, sort_order);
            break;

        case 't':
            printf("OLCP cmd NUMBER OF OBJECTS\n");
            status = object_transfer_service_client_command_request_number_of_objects(&ots_connection);
            break;

        case 'T':
            printf("OLCP cmd CLEAR MARKING\n");
            status = object_transfer_service_client_command_request_clear_marking(&ots_connection);
            break;

        case 'u':
            printf("Filter NAME_IS_EXACTLY\n");
            status = object_transfer_service_client_write_object_list_filter_1(&ots_connection, OTS_FILTER_TYPE_NAME_IS_EXACTLY, sizeof(filter_data_name_is_exactly), filter_data_name_is_exactly);
            break;
        case 'v': {
            printf("Filter OBJECT_TYPE 7FB1\n");
            uint8_t value[] = {0xB1, 0x7f};
            status = object_transfer_service_client_write_object_list_filter_1(&ots_connection, OTS_FILTER_TYPE_OBJECT_TYPE,sizeof(value),value);
            break;
        }
        case 'V': {
            printf("Filter CREATED_BETWEEN\n");
            uint8_t value[] = {225,7,  11, 12, 1, 0,30, 238,7,  2, 10, 13, 2, 0};
            status = object_transfer_service_client_write_object_list_filter_1(&ots_connection, OTS_FILTER_TYPE_CREATED_BETWEEN,sizeof(value),value);
            break;
        }
        case 'x': {
            printf("Filter MODIFIED_BETWEEN\n");
            uint8_t value[] = {222,7,  1, 1, 0, 0,0, 238,7,  11, 30, 13, 2, 0};
            status = object_transfer_service_client_write_object_list_filter_1(&ots_connection, OTS_FILTER_TYPE_MODIFIED_BETWEEN,sizeof(value),value);
            break;
        }
        case 'X': {
            printf("Filter CURRENT_SIZE_BETWEEN\n");
            uint8_t value[] = {1, 0, 0, 0, 0, 4, 0, 0};
            status = object_transfer_service_client_write_object_list_filter_1(&ots_connection, OTS_FILTER_TYPE_CURRENT_SIZE_BETWEEN,sizeof(value),value);
            break;
        }
        case 'y': {
            printf("Filter ALLOCATED_SIZE_BETWEEN\n");
            uint8_t value[] = {1, 0, 0, 0, 0, 4, 0, 0};
            status = object_transfer_service_client_write_object_list_filter_1(&ots_connection, OTS_FILTER_TYPE_ALLOCATED_SIZE_BETWEEN,sizeof(value),value);
            break;
        }

        case 'Y':
            printf("Filter MARKED_OBJECTS\n");
            status = object_transfer_service_client_write_object_list_filter_1(&ots_connection, OTS_FILTER_TYPE_MARKED_OBJECTS,0,NULL);
            break;

        case '0':
            printf("Open Object Channel\n");
            status = object_transfer_service_client_open_object_channel(&ots_connection, cbm_receive_buffer, CBM_RECEIVE_BUFFER_LEN);
            break;

        case '1':
            printf("OACP - Read Object \n");
            status = object_transfer_service_client_read(&ots_connection, 0, cbm_object_size);
            break;

        case '2':
            printf("OACP - Calculate Checksum, chunk length %d\n", cbm_object_read_chunk_transferred);
            status = object_transfer_service_client_calculate_checksum(&ots_connection, 0, cbm_object_read_chunk_transferred);
            break;

        case '3':
            printf("OACP - Write (not truncated)\n");
            status = object_transfer_service_client_write(&ots_connection, false, 0, (uint8_t *) ots_object_name_long, strlen(ots_object_name_long));
            break;

        case '4':
            printf("OACP - Write (truncated)\n");
            status = object_transfer_service_client_write(&ots_connection, true, 0, (uint8_t *) ots_object_name_long, strlen(ots_object_name_long));
            break;

        case '5':
            printf("OACP - Execute\n");
            status = object_transfer_service_client_execute(&ots_connection);
            break;

        case '6':
            printf("OACP - Abort\n");
            status = object_transfer_service_client_abort(&ots_connection);
            break;


        case '7':
            printf("OACP - Create\n");
            status = object_transfer_service_client_create_object(&ots_connection, 277, OTS_OBJECT_TYPE_TRACK);
            break;
        case '8':
            printf("OACP - Delete\n");
            status = object_transfer_service_client_delete_object(&ots_connection);
            break;
        case '9':
            printf("Close Object Channel\n");
            status = object_transfer_service_client_close_object_channel(&ots_connection);
            break;

        case '\n':
        case '\r':
            break;
        default:
            show_usage();
            break;
    }

    if (status != ERROR_CODE_SUCCESS){
        printf("OTS Client Test: command %c failed due to 0%02x\n", c, status);
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){
    UNUSED(argc);
    (void)argv;

    btstack_stdin_setup(stdin_process);

    object_transfer_service_client_setup();
    memcpy(current_pts_address, public_pts_address, 6);
    current_pts_address_type = public_pts_address_type;

    app_state = APP_STATE_IDLE;

    // turn on!
    hci_power_control(HCI_POWER_ON);
    return 0;
}



