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

#define BTSTACK_FILE__ "le_mitm.c"

// *****************************************************************************
/* EXAMPLE_START(le_mitm): LE Man-in-the-Middle Tool
 *
 * @text The example first does an LE scan and allows the user to select a Peripheral
 * device. Then, it connects to the Peripheral and starts advertising with the same
 * data as the Peripheral device.
 * ATT Requests and responses are forwarded between the peripheral and the central
 * Security requests are handled locally.
 *
 * @note A Bluetooth Controller that supports Central and Peripheral Role
 * at the same time is required for this example. See chipset/README.md
 *
 */
// *****************************************************************************


#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "btstack.h"

// Number of devices shown during scanning
#define MAX_NUM_DEVICES 36

// Max number of ATT PTUs to queue (if malloc is not used)
#define MAX_NUM_ATT_PDUS 20

// Max ATT MTU - can be increased if needed
#define MAX_ATT_MTU ATT_DEFAULT_MTU

typedef struct {
    bd_addr_t addr;
    bd_addr_type_t addr_type;
    int8_t  rssi;
    uint8_t ad_len;
    uint8_t ad_data[31];
    uint8_t scan_len;
    uint8_t scan_data[31];
} device_info_t;

typedef struct {
    btstack_linked_item_t  item;
    hci_con_handle_t handle;
    uint8_t len;
    uint8_t data[MAX_ATT_MTU];
} att_pdu_t;

typedef enum {
    TC_OFF,
    TC_SCANNING,
    TC_W4_CONNECT,
    TC_CONNECTED,
} app_state_t;

static uint16_t devices_found;
static device_info_t devices[MAX_NUM_DEVICES];

static uint16_t         remote_peripheral_index;
static bd_addr_t        remote_peripheral_addr;
static bd_addr_type_t   remote_peripheral_addr_type;
static hci_con_handle_t remote_peripheral_handle;

static hci_con_handle_t remote_central_handle;

static btstack_linked_list_t outgoing_att_pdus;

static app_state_t state = TC_OFF;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

static const char * ad_types[] = {
        "",
        "Flags",
        "Incomplete 16-bit UUIDs",
        "Complete 16-bit UUIDs",
        "Incomplete 32-bit UUIDs",
        "Complete 32-bit UUIDs",
        "Incomplete 128-bit UUIDs",
        "Complete 128-bit UUIDs",
        "Short Name",
        "Complete Name",
        "Tx Power Level",
        "",
        "",
        "Class of Device",
        "Simple Pairing Hash C",
        "Simple Pairing Randomizer R",
        "Device ID",
        "Security Manager TK Value",
        "Slave Connection Interval Range",
        "",
        "16-bit Solicitation UUIDs",
        "128-bit Solicitation UUIDs",
        "Service Data",
        "Public Target Address",
        "Random Target Address",
        "Appearance",
        "Advertising Interval"
};

static const char * adv_failed_warning = "\n"
 "[!] Start advertising failed!\n"
 "[!] Make sure your Bluetooth Controller supports Central and Peripheral Roles at the same time.\n\n";

// att pdu pool implementation
#ifndef HAVE_MALLOC
static att_pdu_t att_pdu_storage[MAX_NUM_ATT_PDUS];
static btstack_memory_pool_t att_pdu_pool;
static att_pdu_t * btstack_memory_att_pdu_get(void){
    void * buffer = btstack_memory_pool_get(&att_pdu_pool);
    if (buffer){
        memset(buffer, 0, sizeof(att_pdu_t));
    }
    return (att_pdu_t *) buffer;
}
static void btstack_memory_att_pdu_free(att_pdu_t *att_pdu){
    btstack_memory_pool_free(&att_pdu_pool, att_pdu);
}
#else
static att_pdu_t * btstack_memory_att_pdu_get(void){
    void * buffer = malloc(sizeof(att_pdu_t));
    if (buffer){
        memset(buffer, 0, sizeof(att_pdu_t));
    }
    return (att_pdu_t *) buffer;
}
static void btstack_memory_att_pdu_free(att_pdu_t * att_pdu){
    free(att_pdu);
}
#endif

static void mitm_start_scan(btstack_timer_source_t * ts){
    UNUSED(ts);
    printf("[-] Start scanning\n");
    printf("To select device, enter advertisement number:\n");
    state = TC_SCANNING;
    gap_set_scan_parameters(0,0x0030, 0x0030);
    gap_start_scan();
}

static void mitm_connect(uint16_t index){
    // stop scanning, and connect to the device
    gap_stop_scan();
    state = TC_W4_CONNECT;
    remote_peripheral_index = index;
    memcpy(remote_peripheral_addr, devices[index].addr, 6);
    remote_peripheral_addr_type = devices[index].addr_type;
    printf("\n");
    printf("[-] Connecting to Peripheral %s\n", bd_addr_to_str(remote_peripheral_addr));
    gap_auto_connection_start(remote_peripheral_addr_type, remote_peripheral_addr);
}

static void mitm_start_advertising(void){
    // set adv + scan data if available
    if (devices[remote_peripheral_index].ad_len > 0){
        gap_advertisements_set_data(devices[remote_peripheral_index].ad_len, devices[remote_peripheral_index].ad_data);
        printf("[-] Setup adv data (len %02u): ", devices[remote_peripheral_index].ad_len);
        printf_hexdump(devices[remote_peripheral_index].ad_data, devices[remote_peripheral_index].ad_len);
    }
    if (devices[remote_peripheral_index].scan_len > 0){
        gap_scan_response_set_data(devices[remote_peripheral_index].scan_len, devices[remote_peripheral_index].scan_data);
        printf("[-] Setup scan data (len %02u): ", devices[remote_peripheral_index].scan_len);
        printf_hexdump(devices[remote_peripheral_index].ad_data, devices[remote_peripheral_index].ad_len);
    }
    uint16_t adv_int_min = 0x0030;
    uint16_t adv_int_max = 0x0030;
    uint8_t adv_type = 0;
    bd_addr_t null_addr;
    memset(null_addr, 0, 6);
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_enable(1);
}

static void mitm_print_advertisement(uint16_t index) {
    // get character for index
    char c;
    if (index < 10) {
        c = '0' + index;
    } else {
        c = 'a' + (index - 10);
    }

    printf("%c. %s (%-3d dBm)", c, bd_addr_to_str(devices[index].addr), devices[index].rssi);

    ad_context_t context;
    bd_addr_t address;
    uint8_t uuid_128[16];
    for (ad_iterator_init(&context, devices[index].ad_len, devices[index].ad_data); ad_iterator_has_more(
            &context); ad_iterator_next(&context)) {
        uint8_t data_type = ad_iterator_get_data_type(&context);
        uint8_t size = ad_iterator_get_data_len(&context);
        const uint8_t *data = ad_iterator_get_data(&context);

        if (data_type > 0 && data_type < 0x1B) {
            printf(" - %s: ", ad_types[data_type]);
        }
        uint8_t i;
        switch (data_type) {
            case BLUETOOTH_DATA_TYPE_FLAGS:
                printf("0x%02x", data[0]);
                break;
            case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_16_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_LIST_OF_16_BIT_SERVICE_SOLICITATION_UUIDS:
                for (i = 0; i < size; i += 2) {
                    printf("%02X ", little_endian_read_16(data, i));
                }
                break;
            case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_32_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_32_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_LIST_OF_32_BIT_SERVICE_SOLICITATION_UUIDS:
                for (i = 0; i < size; i += 4) {
                    printf("%04"PRIX32, little_endian_read_32(data, i));
                }
                break;
            case BLUETOOTH_DATA_TYPE_INCOMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LIST_OF_128_BIT_SERVICE_CLASS_UUIDS:
            case BLUETOOTH_DATA_TYPE_LIST_OF_128_BIT_SERVICE_SOLICITATION_UUIDS:
                reverse_128(data, uuid_128);
                printf("%s", uuid128_to_str(uuid_128));
                break;
            case BLUETOOTH_DATA_TYPE_SHORTENED_LOCAL_NAME:
            case BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME:
                for (i = 0; i < size; i++) {
                    printf("%c", (char) (data[i]));
                }
                break;
            case BLUETOOTH_DATA_TYPE_TX_POWER_LEVEL:
                printf("%d dBm", *(int8_t *) data);
                break;
            case BLUETOOTH_DATA_TYPE_SLAVE_CONNECTION_INTERVAL_RANGE:
                printf("Connection Interval Min = %u ms, Max = %u ms", little_endian_read_16(data, 0) * 5 / 4,
                       little_endian_read_16(data, 2) * 5 / 4);
                break;
            case BLUETOOTH_DATA_TYPE_SERVICE_DATA:
                printf_hexdump(data, size);
                break;
            case BLUETOOTH_DATA_TYPE_PUBLIC_TARGET_ADDRESS:
            case BLUETOOTH_DATA_TYPE_RANDOM_TARGET_ADDRESS:
                reverse_bd_addr(data, address);
                printf("%s", bd_addr_to_str(address));
                break;
            case BLUETOOTH_DATA_TYPE_APPEARANCE:
                // https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.gap.appearance.xml
                printf("%02X", little_endian_read_16(data, 0));
                break;
            case BLUETOOTH_DATA_TYPE_ADVERTISING_INTERVAL:
                printf("%u ms", little_endian_read_16(data, 0) * 5 / 8);
                break;
            case BLUETOOTH_DATA_TYPE_3D_INFORMATION_DATA:
                printf_hexdump(data, size);
                break;
            case BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA:
            case BLUETOOTH_DATA_TYPE_CLASS_OF_DEVICE:
            case BLUETOOTH_DATA_TYPE_SIMPLE_PAIRING_HASH_C:
            case BLUETOOTH_DATA_TYPE_SIMPLE_PAIRING_RANDOMIZER_R:
            case BLUETOOTH_DATA_TYPE_DEVICE_ID:
            case BLUETOOTH_DATA_TYPE_SECURITY_MANAGER_OUT_OF_BAND_FLAGS:
            default:
                break;
        }
    }
    printf("\n");
}

static void mitm_handle_adv(uint8_t * packet){
    // get addr and type
    bd_addr_t remote_addr;
    gap_event_advertising_report_get_address(packet, remote_addr);
    bd_addr_type_t remote_addr_type = gap_event_advertising_report_get_address_type(packet);
    uint8_t adv_event_type = gap_event_advertising_report_get_advertising_event_type(packet);
    bool is_scan_response = adv_event_type == 2 || adv_event_type == 4;

    // find remote in list
    uint16_t i;
    for (i=0;i<devices_found;i++) {
        if (memcmp(remote_addr, devices[i].addr, 6) != 0) continue;
        if (remote_addr_type != devices[i].addr_type) continue;
        break;
    }

    if (i == MAX_NUM_DEVICES) return;

    if (devices_found == i){
        // skip first event with scan response data (should not happen)
        if (is_scan_response) return;
        memset(&devices[i], 0, sizeof(device_info_t));
        devices[i].rssi = (int8_t) gap_event_advertising_report_get_rssi(packet);
        devices[i].addr_type = remote_addr_type;
        memcpy(devices[i].addr, remote_addr, 6);
        devices[i].ad_len = gap_event_advertising_report_get_data_length(packet);
        memcpy(devices[i].ad_data, gap_event_advertising_report_get_data(packet), devices[i].ad_len);
        mitm_print_advertisement(i);
        devices_found++;
        return;
    }

    // store scan data
    if (!is_scan_response) return;
    devices[i].scan_len = gap_event_advertising_report_get_data_length(packet);
    memcpy(devices[i].scan_data, gap_event_advertising_report_get_data(packet), devices[i].scan_len);
}

static void mitm_console_connected_menu(void){
    printf("=== Connected menu ===\n");
    printf("p - Pair Peripheral\n");
}

static hci_con_handle_t mitm_opposite_handle(hci_con_handle_t handle){
    if (handle == remote_peripheral_handle) {
        return remote_central_handle;
    } else {
        return remote_peripheral_handle;
    }
}

static void mitm_request_to_send(void){
    // request to send again if more packets queued
    if (btstack_linked_list_empty(&outgoing_att_pdus)) return;
    att_pdu_t * pdu = (att_pdu_t *) btstack_linked_list_get_first_item((&outgoing_att_pdus));
    l2cap_request_can_send_fix_channel_now_event(pdu->handle, L2CAP_CID_ATTRIBUTE_PROTOCOL);
}

static const char * mitm_name_for_handle(hci_con_handle_t handle){
    if (handle == remote_peripheral_handle) return "Peripheral";
    if (handle == remote_central_handle)    return "Central";
    return "(unknown handle)'";
}

static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);

    if (packet_type != HCI_EVENT_PACKET) return;
    
    uint8_t event = hci_event_packet_get_type(packet);
    hci_con_handle_t connection_handle;
    uint32_t passkey;

    switch (event) {
        case BTSTACK_EVENT_STATE:
            // BTstack activated, get started
            if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                mitm_start_scan(NULL);
                state = TC_SCANNING;
            } else {
                state = TC_OFF;
            }
            break;
        case GAP_EVENT_ADVERTISING_REPORT:
            if (state != TC_SCANNING) return;
            mitm_handle_adv(packet);
            break;
        case HCI_EVENT_COMMAND_COMPLETE:
            // warn if adv enable fails
            if (hci_event_command_complete_get_command_opcode(packet) != hci_le_set_advertise_enable.opcode) break;
            if (hci_event_command_complete_get_return_parameters(packet)[0] == ERROR_CODE_SUCCESS) break;
            printf("%s", adv_failed_warning);
            break;
        case HCI_EVENT_LE_META:
            // wait for connection complete
            if (hci_event_le_meta_get_subevent_code(packet) !=  HCI_SUBEVENT_LE_CONNECTION_COMPLETE) break;
            switch (state){
                case TC_W4_CONNECT:
                    state = TC_CONNECTED;
                    remote_peripheral_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    printf("[-] Peripheral connected\n");
                    mitm_start_advertising();
                    printf ("You can connect now!\n");
                    printf("\n");
                    mitm_console_connected_menu();
                    break;
                case TC_CONNECTED:
                    remote_central_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    printf("[-] Central connected!\n");
                    break;
                default:
                    break;
            }
            break;
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            // unregister listener
            connection_handle = HCI_CON_HANDLE_INVALID;
            printf("[-] %s disconnected", mitm_name_for_handle(connection_handle));
            if (connection_handle == remote_peripheral_handle){
                mitm_start_scan(NULL);
                state = TC_SCANNING;
            }
            break;
        case SM_EVENT_JUST_WORKS_REQUEST:
            connection_handle = sm_event_just_works_request_get_handle(packet);
            printf("[-] %s request 'Just Works' pairing\n", mitm_name_for_handle(connection_handle));
            sm_just_works_confirm(connection_handle);
            break;
        case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
            passkey = sm_event_numeric_comparison_request_get_passkey(packet);
            connection_handle = sm_event_numeric_comparison_request_get_handle(packet);
            printf("[-] %s accepting numeric comparison: %"PRIu32"\n", mitm_name_for_handle(connection_handle), passkey);
            sm_numeric_comparison_confirm(connection_handle);
            break;
        case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
            passkey = sm_event_passkey_display_number_get_passkey(packet);
            connection_handle = sm_event_passkey_display_number_get_handle(packet);
            printf("[-] %s display passkey: %"PRIu32"\n", mitm_name_for_handle(connection_handle), passkey);
            break;
        case SM_EVENT_PAIRING_COMPLETE:
            connection_handle = sm_event_pairing_complete_get_handle(packet);
            switch (sm_event_pairing_complete_get_status(packet)){
                case ERROR_CODE_SUCCESS:
                    printf("[-] %s pairing complete, success\n", mitm_name_for_handle(connection_handle));
                    break;
                case ERROR_CODE_CONNECTION_TIMEOUT:
                    printf("[-] %s pairing failed, timeout\n", mitm_name_for_handle(connection_handle));
                    break;
                case ERROR_CODE_REMOTE_USER_TERMINATED_CONNECTION:
                    printf("[-] %s pairing failed, disconnected\n", mitm_name_for_handle(connection_handle));
                    break;
                case ERROR_CODE_AUTHENTICATION_FAILURE:
                    printf("[-] %s pairing failed, reason = %u\n", mitm_name_for_handle(connection_handle), sm_event_pairing_complete_get_reason(packet));
                    break;
                default:
                    break;
            }
            break;
        default:
            break;
    }
}

static void att_packet_handler(uint8_t packet_type, uint16_t handle, uint8_t *packet, uint16_t size){
    att_pdu_t * pdu;
    switch (packet_type){
        case ATT_DATA_PACKET:
            printf("[%10s] ", mitm_name_for_handle(handle));
            printf_hexdump(packet, size);
            pdu = btstack_memory_att_pdu_get();
            if (!pdu) break;
            // handle att mtu exchange directly
            if (packet[0] == ATT_EXCHANGE_MTU_REQUEST){
                pdu->handle = handle;
                pdu->len = 3;
                pdu->data[0] = ATT_EXCHANGE_MTU_RESPONSE;
                little_endian_store_16(pdu->data, 1, MAX_ATT_MTU);
            } else {
                btstack_assert(size <= MAX_ATT_MTU);
                pdu->handle = mitm_opposite_handle(handle);
                pdu->len = size;
                memcpy(pdu->data, packet, size);
            }
            btstack_linked_list_add_tail(&outgoing_att_pdus, (btstack_linked_item_t *) pdu);
            mitm_request_to_send();
            break;
        case HCI_EVENT_PACKET:
            if (packet[0] == L2CAP_EVENT_CAN_SEND_NOW) {
                // send next packet
                pdu = (att_pdu_t *) btstack_linked_list_pop(&outgoing_att_pdus);
                if (pdu == NULL) break;
                l2cap_send_connectionless(pdu->handle, L2CAP_CID_ATTRIBUTE_PROTOCOL, pdu->data, pdu->len);
                btstack_memory_att_pdu_free(pdu);
                // request to send again if more packets queued
                mitm_request_to_send();
            }
            break;
        default:
            break;
    }
}

static void stdin_process(char cmd) {
    unsigned int index;
    switch(state){
        case TC_OFF:
            break;
        case TC_SCANNING:
            if ((cmd >= '0') && (cmd <= '9')){
                index = cmd - '0';
            } else if ((cmd >= 'a') && (cmd <= 'z')){
                index = cmd - 'a' + 10;
            } else {
                break;
            }
            if (index >= devices_found) break;
            mitm_connect(index);
            break;
        case TC_CONNECTED:
            switch (cmd){
                case 'p':
                    printf("[-] Start pairing / encryption with Peripheral\n");
                    sm_request_pairing(remote_peripheral_handle);
                    break;
                default:
                    mitm_console_connected_menu();
                    break;
            }
            break;
        default:
            break;
    }
}

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]){

    (void)argc;
    (void)argv;

    l2cap_init();

    l2cap_register_fixed_channel(att_packet_handler, L2CAP_CID_ATTRIBUTE_PROTOCOL);

    sm_init();

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    sm_event_callback_registration.callback = &hci_event_handler;
    sm_add_event_handler(&sm_event_callback_registration);

#ifndef HAVE_MALLOC
    btstack_memory_pool_create(&att_pdu_pool, att_pdu_storage, MAX_NUM_ATT_PDUS, sizeof(att_pdu_t));
#endif

#ifdef HAVE_BTSTACK_STDIN
    btstack_stdin_setup(stdin_process);
#endif

    // turn on!
    hci_power_control(HCI_POWER_ON);

    return 0;
}
/* EXAMPLE_END */
