/*
 * Copyright (C) 2017 BlueKitchen GmbH
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ble/gatt-service/mesh_provisioning_service_server.h"
#include "hci_dump.h"
#include "mesh/mesh_node.h"
#include "mesh/pb_adv.h"
#include "mesh/pb_gatt.h"
#include "mesh/provisioning.h"
#include "mesh/provisioning_device.h"

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

static void CHECK_EQUAL_ARRAY(uint8_t * expected, uint8_t * actual, int size){
    int i;
    for (i=0; i<size; i++){
        if (expected[i] != actual[i]) {
            printf("offset %u wrong\n", i);
            printf("expected: "); printf_hexdump(expected, size);
            printf("actual:   "); printf_hexdump(actual, size);
        }
        BYTES_EQUAL(expected[i], actual[i]);
    }
}

void dump_data(uint8_t * buffer, uint16_t size){
    static int data_counter = 1;
    char var_name[80];
    sprintf(var_name, "test_data_%02u", data_counter);
    printf("uint8_t %s[] = { ", var_name);
    for (int i = 0; i < size ; i++){
        if ((i % 16) == 0) printf("\n    ");
        printf ("0x%02x, ", buffer[i]);
    }
    printf("};\n");
    data_counter++;
}

int parse_hex(uint8_t * buffer, const char * hex_string){
    int len = 0;
    while (*hex_string){
        if (*hex_string == ' '){
            hex_string++;
            continue;
        }
        int high_nibble = nibble_for_char(*hex_string++);
        int low_nibble = nibble_for_char(*hex_string++);
        *buffer++ = (high_nibble << 4) | low_nibble;
        len++;
    }
    return len;
}

// returns if anything was done
extern "C" int mock_process_hci_cmd(void);

const static uint8_t device_uuid[] = { 0x00, 0x1B, 0xDC, 0x08, 0x10, 0x21, 0x0B, 0x0E, 0x0A, 0x0C, 0x00, 0x0B, 0x0E, 0x0A, 0x0C, 0x00 };

// pb-adv mock for testing

static btstack_packet_handler_t pb_adv_packet_handler;

static uint8_t * pdu_data;
static uint16_t  pdu_size;

/**
 * Initialize Provisioning Bearer using Advertisement Bearer
 * @param DeviceUUID
 */
void pb_adv_init(void){}
void pb_gatt_init(void){}

/**
 * Close Link
 * @param con_handle
 * @param reason 0 = success, 1 = timeout, 2 = fail
 */
void pb_gatt_close_link(hci_con_handle_t con_handle, uint8_t reason){}
void pb_adv_close_link(hci_con_handle_t con_handle, uint8_t reason){}


/**
 * Register listener for Provisioning PDUs and MESH_PBV_ADV_SEND_COMPLETE
 */
void pb_adv_register_packet_handler(btstack_packet_handler_t packet_handler){
    pb_adv_packet_handler = packet_handler;
}

void pb_gatt_register_packet_handler(btstack_packet_handler_t packet_handler){
    UNUSED(packet_handler);
}
/** 
 * Send Provisioning PDU
 */
void pb_adv_send_pdu(uint16_t pb_transport_cid, const uint8_t * pdu, uint16_t size){
    UNUSED(pb_transport_cid);
    pdu_data = (uint8_t*) pdu;
    pdu_size = size;
    // dump_data((uint8_t*)pdu,size);
    // printf_hexdump(pdu, size);
}
void pb_gatt_send_pdu(uint16_t con_handle, const uint8_t * pdu, uint16_t pdu_size){}
 
uint16_t mesh_network_key_get_free_index(void){
    return 0;
}

static mesh_network_key_t network_key; 
extern "C" mesh_network_key_t * btstack_memory_mesh_network_key_get(void){
    return &network_key;
}

static void perform_crypto_operations(void){
    int more = 1;
    while (more){
        more = mock_process_hci_cmd();
    }
}

static void send_prov_pdu(const uint8_t * packet, uint16_t size){
    pb_adv_packet_handler(PROVISIONING_DATA_PACKET, 0, (uint8_t*) packet, size);
    perform_crypto_operations();
}

static void pb_adv_emit_pdu_sent(uint8_t status){
    uint8_t event[] = { HCI_EVENT_MESH_META, 2, MESH_SUBEVENT_PB_TRANSPORT_PDU_SENT, status};
    pb_adv_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static int scan_hex_byte(const char * byte_string){
    int upper_nibble = nibble_for_char(*byte_string++);
    if (upper_nibble < 0) return -1;
    int lower_nibble = nibble_for_char(*byte_string);
    if (lower_nibble < 0) return -1;
    return (upper_nibble << 4) | lower_nibble;
}

static int btstack_parse_hex(const char * string, uint16_t len, uint8_t * buffer){
    int i;
    for (i = 0; i < len; i++) {
        int single_byte = scan_hex_byte(string);
        if (single_byte < 0) return 0;
        string += 2;
        buffer[i] = (uint8_t)single_byte;
        // don't check seperator after last byte
        if (i == len - 1) {
            return 1;
        }
        // optional seperator
        char separator = *string;
        if (separator == ':' && separator == '-' && separator == ' ') {
            string++;
        }
    }
    return 1;
}

// void pb_gatt_close_link(hci_con_handle_t con_handle, uint8_t reason){
// }

static uint8_t      prov_static_oob_data[16];
static const char * prov_static_oob_string = "00000000000000000102030405060708";

TEST_GROUP(Provisioning){
    void setup(void){
        btstack_crypto_init();
        provisioning_device_init();
        mesh_node_set_device_uuid(device_uuid);
        btstack_parse_hex(prov_static_oob_string, 16, prov_static_oob_data);
        provisioning_device_set_static_oob(16, prov_static_oob_data);
        provisioning_device_set_output_oob_actions(0x08, 0x08);
        provisioning_device_set_input_oob_actions(0x08, 0x08);
        perform_crypto_operations();
    }
};

uint8_t prov_invite[] = { 0x00, 0x00 };
uint8_t prov_capabilities[] = { 0x01, 0x01, 0x00, 0x01, 0x00, 0x01, 0x08, 0x00, 0x08, 0x08, 0x00, 0x08, };
uint8_t prov_start[] = { 0x02, 0x00, 0x00, 0x02, 0x00, 0x01 };
uint8_t prov_public_key[] = { 0x03,
    0xf0, 0xc8, 0x63, 0xf8, 0xe5, 0x55, 0x11, 0x4b, 0xf4, 0x88, 0x2c, 0xc7, 0x87, 0xb9, 0x5c, 0x27,
    0x2a, 0x7f, 0xe4, 0xdc, 0xdd, 0xf1, 0x92, 0x2f, 0x4f, 0x18, 0xa4, 0x94, 0xe1, 0xc3, 0x57, 0xa1,
    0xa6, 0xc3, 0x2d, 0x07, 0xbe, 0xb5, 0x76, 0xab, 0x60, 0x10, 0x68, 0x06, 0x8f, 0x0a, 0x9e, 0x01,
    0x60, 0xc3, 0xa1, 0x41, 0x19, 0xf5, 0xd4, 0x26, 0xa7, 0x95, 0x5d, 0xa3, 0xe6, 0xed, 0x3e, 0x81, };
uint8_t prov_confirm[] = { 0x05, 0x80, 0x4d, 0xdc, 0x3b, 0xba, 0x60, 0xd5, 0x93, 0x5b, 0x56, 0xef, 0xb5, 0xcb, 0x59, 0x31, 0xfa, };
uint8_t prov_random[]  = { 0x06, 0x9b, 0x4d, 0x39, 0xf6, 0xf7, 0xe8, 0xa1, 0x05, 0xd3, 0xfe, 0xed, 0xa5, 0xd5, 0xf3, 0xd9, 0xe4, };
uint8_t prov_data[] = {
    0x07,
    0x85, 0x66, 0xac, 0x46, 0x37, 0x34, 0x86, 0xe1, 0x3e, 0x4c, 0x13, 0x52, 0xd0, 0x6d, 0x34, 0x7d,
    0xce, 0xf1, 0xd1, 0x7d, 0xbd, 0xbe, 0xcc, 0x99, 0xc3, 
    0x93, 0x87, 0xfc, 0xb0, 0x72, 0x0f, 0xd8, 0x8d };
uint8_t prov_complete[] = { 0x08, };

TEST(Provisioning, Prov1){
    // send prov inviate
    send_prov_pdu(prov_invite, sizeof(prov_invite));        
    // check for prov cap
    CHECK_EQUAL_ARRAY(prov_capabilities, pdu_data, sizeof(prov_capabilities));
    pb_adv_emit_pdu_sent(0);
    // send prov start
    send_prov_pdu(prov_start, sizeof(prov_start));        
    // send public key
    send_prov_pdu(prov_public_key, sizeof(prov_public_key));
    // check for public key
    CHECK_EQUAL_ARRAY(prov_public_key, pdu_data, sizeof(prov_public_key));
    pb_adv_emit_pdu_sent(0);
    // send prov confirm
    send_prov_pdu(prov_confirm, sizeof(prov_confirm));
    // check for prov confirm
    CHECK_EQUAL_ARRAY(prov_confirm, pdu_data, sizeof(prov_confirm));
    pb_adv_emit_pdu_sent(0);
    // send prov random
    send_prov_pdu(prov_random, sizeof(prov_random));
    // check for prov random
    CHECK_EQUAL_ARRAY(prov_random, pdu_data, sizeof(prov_random));
    pb_adv_emit_pdu_sent(0);
    // send prov data
    send_prov_pdu(prov_data, sizeof(prov_data));
    // check prov complete
    CHECK_EQUAL_ARRAY(prov_complete, pdu_data, sizeof(prov_complete));
    pb_adv_emit_pdu_sent(0);
}

int main (int argc, const char * argv[]){
    hci_dump_open("hci_dump.pklg", HCI_DUMP_PACKETLOGGER);
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
