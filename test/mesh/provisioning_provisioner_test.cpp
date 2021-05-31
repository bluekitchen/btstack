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
#include "mesh/pb_adv.h"
#include "mesh/pb_gatt.h"
#include "ble/gatt-service/mesh_provisioning_service_server.h"
#include "mesh/provisioning.h"
#include "mesh/provisioning_provisioner.h"
#include "hci_dump.h"
#include "hci_dump_posix_fs.h"
#include "mock.h"

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

// pb-adv mock for testing

static btstack_packet_handler_t pb_adv_packet_handler;

static uint8_t * pdu_data;
static uint16_t  pdu_size;

static void pb_adv_emit_link_open(uint8_t status, uint16_t pb_adv_cid){
    uint8_t event[7] = { HCI_EVENT_MESH_META, 5, MESH_SUBEVENT_PB_TRANSPORT_LINK_OPEN, status};
    little_endian_store_16(event, 4, pb_adv_cid);
    event[6] = MESH_PB_TYPE_ADV;
    pb_adv_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

static void pb_adv_emit_pdu_sent(uint8_t status){
    uint8_t event[] = { HCI_EVENT_MESH_META, 2, MESH_SUBEVENT_PB_TRANSPORT_PDU_SENT, status};
    pb_adv_packet_handler(HCI_EVENT_PACKET, 0, event, sizeof(event));
}

void pb_adv_init(void){}
void pb_gatt_init(void){}

void pb_adv_close_link(uint16_t pb_adv_cid, uint8_t reason){
    UNUSED(pb_adv_cid);
    UNUSED(reason);
}

void pb_adv_register_provisioner_packet_handler(btstack_packet_handler_t packet_handler){
    pb_adv_packet_handler = packet_handler;
}

void pb_gatt_register_packet_handler(btstack_packet_handler_t packet_handler){
    UNUSED(packet_handler);
}

void pb_adv_send_pdu(uint16_t pb_transport_cid, const uint8_t * pdu, uint16_t size){
    UNUSED(pb_transport_cid);
    pdu_data = (uint8_t*) pdu;
    pdu_size = size;
    // dump_data((uint8_t*)pdu,size);
    // printf_hexdump(pdu, size);
}
void pb_gatt_send_pdu(uint16_t con_handle, const uint8_t * pdu, uint16_t _pdu_size){
    UNUSED(con_handle);
    UNUSED(pdu);
    UNUSED(_pdu_size);
}

uint16_t pb_adv_create_link(const uint8_t * device_uuid){
    UNUSED(device_uuid);
    // just simluate opened
    pb_adv_emit_link_open(0, 1);
    return 1;
}

//
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

static uint8_t      prov_static_oob_data[16];
static const char * prov_static_oob_string = "00000000000000000102030405060708";

static void provisioning_handle_pdu(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size){
    UNUSED(channel);
    UNUSED(size);
    if (packet_type != HCI_EVENT_PACKET) return;
    switch(packet[0]){
        case HCI_EVENT_MESH_META:
            switch(packet[2]){
                case MESH_SUBEVENT_PB_PROV_CAPABILITIES:
                    printf("Provisioner capabilities\n");
                    provisioning_provisioner_select_authentication_method(1, 0, 0, 0, 0, 0);
                    break;
                default:
                    break;
            }
        default:
            break;
    }
}

TEST_GROUP(Provisioning){
    void setup(void){
        mock_init();
        btstack_crypto_init();
        provisioning_provisioner_init();
        btstack_parse_hex(prov_static_oob_string, 16, prov_static_oob_data);
        provisioning_provisioner_register_packet_handler(&provisioning_handle_pdu);
        // provisioning_device_set_static_oob(16, prov_static_oob_data);
        // provisioning_device_set_output_oob_actions(0x08, 0x08);
        // provisioning_device_set_input_oob_actions(0x08, 0x08);
        perform_crypto_operations();
    }
};

const static uint8_t device_uuid[] = { 0x00, 0x1B, 0xDC, 0x08, 0x10, 0x21, 0x0B, 0x0E, 0x0A, 0x0C, 0x00, 0x0B, 0x0E, 0x0A, 0x0C, 0x00 };

uint8_t prov_invite[] = { 0x00, 0x00 };
uint8_t prov_capabilities[] = { 0x01, 0x01, 0x00, 0x01, 0x00, 0x01, 0x08, 0x00, 0x08, 0x08, 0x00, 0x08, };
uint8_t prov_start[] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t prov_public_key[] = { 0x03,
                              0x8E, 0x9D, 0xEE, 0x76, 0x17, 0x67, 0x4A, 0xCD, 0xBE, 0xA5, 0x4D, 0xCD, 0xAB, 0x7E, 0xA3, 0x83,
                              0x26, 0xF6, 0x87, 0x22, 0x7F, 0x97, 0xE7, 0xC4, 0xEE, 0xF5, 0x2F, 0x7C, 0x80, 0x5A, 0xEB, 0x2A,
                              0x59, 0x46, 0x99, 0xDF, 0x60, 0x54, 0x53, 0x47, 0x48, 0xF8, 0xA1, 0x99, 0xF0, 0x02, 0xE7, 0x23,
                              0x2C, 0x52, 0x9C, 0xBA, 0x0F, 0x54, 0x94, 0x1F, 0xE4, 0xB7, 0x02, 0x89, 0x9E, 0x03, 0xFA, 0x43 };
uint8_t prov_confirm[] = {   0x05, 0xF3, 0x73, 0xAB, 0xD8, 0xA6, 0x51, 0xD4, 0x18, 0x78, 0x47, 0xBA, 0xD8, 0x07, 0x6E, 0x09, 0x05 };
uint8_t prov_random[]  = {   0x06, 0xED, 0x77, 0xBA, 0x5D, 0x2F, 0x16, 0x0B, 0x84, 0xC2, 0xE1, 0xF1, 0xF9, 0x7D, 0x3F, 0x9E, 0xCF };
uint8_t prov_data[] = {
        0x07, 0x89, 0x8B, 0xBF, 0x53, 0xDC, 0xA6, 0xD8, 0x55, 0x1B, 0x9A, 0xA9, 0x57, 0x8C, 0x8E, 0xAC, 0x25, 0x13, 0x80, 0xC3,
        0x8B, 0xA2, 0xF8, 0xAA, 0x13, 0xAF, 0x60, 0x1A, 0x5B, 0x12, 0xE9, 0xD5, 0x5F,
};
uint8_t prov_complete[] = { 0x08, };

static void expect_packet(const char * packet_name, const uint8_t * packet_data, uint16_t packet_len){
    printf("Expect '%s'\n", packet_name);
    CHECK(pdu_data != NULL);
    CHECK_EQUAL_ARRAY((uint8_t*)packet_data, pdu_data, packet_len);
}

#define EXPECT_PACKET(packet) expect_packet(#packet, packet, sizeof(packet))

TEST(Provisioning, Prov1){
    // simulate unprovisioned device beacon
    provisioning_provisioner_start_provisioning(device_uuid);
    // wait for prov invite
    EXPECT_PACKET(prov_invite);
    pb_adv_emit_pdu_sent(0);
    // send capabilities
    send_prov_pdu(prov_capabilities, sizeof(prov_capabilities));
    // wait for start
    EXPECT_PACKET(prov_start);
    pb_adv_emit_pdu_sent(0);
    // wait for public key
    EXPECT_PACKET(prov_public_key);
    pb_adv_emit_pdu_sent(0);
    // send public key
    send_prov_pdu(prov_public_key, sizeof(prov_public_key));
    // wait for confirm
    EXPECT_PACKET(prov_confirm);
    pb_adv_emit_pdu_sent(0);
    // send prov confirm
    send_prov_pdu(prov_confirm, sizeof(prov_confirm));
    // wait for random
    CHECK_EQUAL_ARRAY(prov_random, pdu_data, sizeof(prov_random));
    pb_adv_emit_pdu_sent(0);
    // send prov random
    send_prov_pdu(prov_random, sizeof(prov_random));
    // wait for prov data
    EXPECT_PACKET(prov_data);
    pb_adv_emit_pdu_sent(0);
    // send prov complete
    send_prov_pdu(prov_complete, sizeof(prov_complete));
}

int main (int argc, const char * argv[]){
    // log into file using HCI_DUMP_PACKETLOGGER format
    const char * log_path = "hci_dump.pklg";
    hci_dump_posix_fs_open(log_path, HCI_DUMP_PACKETLOGGER);
    hci_dump_init(hci_dump_posix_fs_get_instance());
    printf("Packet Log: %s\n", log_path);

    return CommandLineTestRunner::RunAllTests(argc, argv);
}
