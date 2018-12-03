#include <stdio.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "ble/mesh/adv_bearer.h"
#include "ble/mesh/mesh_network.h"
#include "mesh_transport.h"
#include "btstack_util.h"
#include "provisioning.h"
#include "btstack_memory.h"

extern "C" int mock_process_hci_cmd(void);

static mesh_network_pdu_t * received_network_pdu;

static uint8_t sent_network_pdu_data[29];
static uint8_t sent_network_pdu_len;

static uint8_t  recv_upper_transport_pdu_data[100];
static uint16_t recv_upper_transport_pdu_len;

static btstack_packet_handler_t mesh_packet_handler;
void adv_bearer_register_for_mesh_message(btstack_packet_handler_t packet_handler){
    mesh_packet_handler = packet_handler;
}
void adv_bearer_request_can_send_now_for_mesh_message(void){
    printf("adv_bearer_request_can_send_now_for_mesh_message\n");
    // simulate can send now
    uint8_t event[3];
    event[0] = HCI_EVENT_MESH_META;
    event[1] = 1;
    event[2] = MESH_SUBEVENT_CAN_SEND_NOW;
    (*mesh_packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}
void adv_bearer_send_mesh_message(const uint8_t * network_pdu, uint16_t size){
    printf("adv_bearer_send_mesh_message: \n");
    printf_hexdump(network_pdu, size);
    memcpy(sent_network_pdu_data, network_pdu, size);
    sent_network_pdu_len = size;
}
void adv_bearer_emit_sent(void){
    printf("adv_bearer_emit_sent\n");
    uint8_t event[3];
    event[0] = HCI_EVENT_MESH_META;
    event[1] = 1;
    event[2] = MESH_SUBEVENT_CAN_SEND_NOW;
    (*mesh_packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}

void CHECK_EQUAL_ARRAY(uint8_t * expected, uint8_t * actual, int size){
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

static void btstack_print_hex(const uint8_t * data, uint16_t len, char separator){
    int i;
    for (i=0;i<len;i++){
        printf("%02x", data[i]);
        if (separator){
            printf("%c", separator);
        }
    }
    printf("\n");
}

static void load_network_key_nid_68(void){
    mesh_provisioning_data_t provisioning_data;
    provisioning_data.nid = 0x68;
    btstack_parse_hex("0953fa93e7caac9638f58820220a398e", 16, provisioning_data.encryption_key);
    btstack_parse_hex("8b84eedec100067d670971dd2aa700cf", 16, provisioning_data.privacy_key);
    mesh_network_key_list_add_from_provisioning_data(&provisioning_data);
}

static void load_network_key_nid_5e(void){
    mesh_provisioning_data_t provisioning_data;
    provisioning_data.nid = 0x5e;
    btstack_parse_hex("be635105434859f484fc798e043ce40e", 16, provisioning_data.encryption_key);
    btstack_parse_hex("5d396d4b54d3cbafe943e051fe9a4eb8", 16, provisioning_data.privacy_key);
    mesh_network_key_list_add_from_provisioning_data(&provisioning_data);
}

static void load_provisioning_data_test_message(void){
    load_network_key_nid_68();
    uint8_t application_key[16];
    btstack_parse_hex("63964771734fbd76e3b40519d1d94a48", 16, application_key);
    mesh_application_key_set( 0, 0x26, application_key);

    uint8_t device_key[16];
    btstack_parse_hex("9d6dd0e96eb25dc19a40ed9914f8f03f", 16, device_key);
    mesh_transport_set_device_key(device_key);
}

static void test_lower_transport_callback_handler(mesh_network_callback_type_t callback_type, mesh_network_pdu_t * network_pdu){
    switch (callback_type){
        case MESH_NETWORK_PDU_RECEIVED:
        printf("test MESH_NETWORK_PDU_RECEIVED\n");
            received_network_pdu = network_pdu;
            break;
        case MESH_NETWORK_PDU_SENT:
            printf("test MESH_NETWORK_PDU_SENT\n");
            mesh_lower_transport_received_mesage(MESH_NETWORK_PDU_SENT, network_pdu);
            break;
        default:
            break;
    }
}

static void test_upper_transport_unsegmented_callback_handler(mesh_network_pdu_t * network_pdu){
    if (mesh_network_control(network_pdu)){
        recv_upper_transport_pdu_len = mesh_network_pdu_len(network_pdu);
        memcpy(recv_upper_transport_pdu_data, mesh_network_pdu_data(network_pdu), recv_upper_transport_pdu_len);
    } else {
        recv_upper_transport_pdu_len = mesh_network_pdu_len(network_pdu)  - 1;
        memcpy(recv_upper_transport_pdu_data, mesh_network_pdu_data(network_pdu) + 1, recv_upper_transport_pdu_len);
    }
}

static void test_upper_transport_segmented_callback_handler(mesh_transport_pdu_t * transport_pdu){
    recv_upper_transport_pdu_len = transport_pdu->len;
    memcpy(recv_upper_transport_pdu_data, transport_pdu->data, recv_upper_transport_pdu_len);
}

TEST_GROUP(MessageTest){
    void setup(void){
        btstack_memory_init();
        btstack_crypto_init();
        load_provisioning_data_test_message();
        mesh_network_init();
        mesh_network_set_higher_layer_handler(&test_lower_transport_callback_handler);
        mesh_upper_transport_register_unsegemented_message_handler(&test_upper_transport_unsegmented_callback_handler);
        mesh_upper_transport_register_segemented_message_handler(&test_upper_transport_segmented_callback_handler);
        received_network_pdu = NULL;
        recv_upper_transport_pdu_len =0;
        sent_network_pdu_len = 0;
    }
    void teardown(void){
        // printf("-- teardown start --\n\n");
        while (!btstack_crypto_idle()){
            mock_process_hci_cmd();
        }
        // mesh_network_reset();
        // mesh_transport_reset();
        // mesh_network_dump();
        // mesh_transport_dump();
        printf("-- teardown complete --\n\n");
    }
};

static uint8_t transport_pdu_data[64];
static uint16_t transport_pdu_len;

static     uint8_t test_network_pdu_len;
static uint8_t test_network_pdu_data[29];

void test_receive_network_pdus(int count, char ** network_pdus, char ** lower_transport_pdus, char * access_pdu){
    int i;
    for (i=0;i<count;i++){
        test_network_pdu_len = strlen(network_pdus[i]) / 2;
        btstack_parse_hex(network_pdus[i], test_network_pdu_len, test_network_pdu_data);
    
        mesh_network_received_message(test_network_pdu_data, test_network_pdu_len);

        while (received_network_pdu == NULL) {
            mock_process_hci_cmd();
        }

        transport_pdu_len = strlen(lower_transport_pdus[i]) / 2;
        btstack_parse_hex(lower_transport_pdus[i], transport_pdu_len, transport_pdu_data);

        uint8_t * lower_transport_pdu     = mesh_network_pdu_data(received_network_pdu);
        uint8_t   lower_transport_pdu_len = mesh_network_pdu_len(received_network_pdu);

        // printf_hexdump(lower_transport_pdu, lower_transport_pdu_len);

        CHECK_EQUAL( transport_pdu_len, lower_transport_pdu_len);
        CHECK_EQUAL_ARRAY(transport_pdu_data, lower_transport_pdu, transport_pdu_len);

        // forward to mesh_transport
        mesh_lower_transport_received_mesage(MESH_NETWORK_PDU_RECEIVED, received_network_pdu);

        // done
        received_network_pdu = NULL;
    }

    // wait for tranport pdu
    while (recv_upper_transport_pdu_len == 0) {
        mock_process_hci_cmd();
    }

    transport_pdu_len = strlen(access_pdu) / 2;
    btstack_parse_hex(access_pdu, transport_pdu_len, transport_pdu_data);

    printf("UpperTransportPDU: ");
    printf_hexdump(recv_upper_transport_pdu_data, recv_upper_transport_pdu_len);
    CHECK_EQUAL( transport_pdu_len, recv_upper_transport_pdu_len);
    CHECK_EQUAL_ARRAY(transport_pdu_data, recv_upper_transport_pdu_data, transport_pdu_len);
}

void test_send_access_message(uint16_t netkey_index, uint16_t appkey_index,  uint8_t ttl, uint16_t src, uint16_t dest, uint8_t szmic, char * control_pdu, int count, char ** lower_transport_pdus, char ** network_pdus){

    transport_pdu_len = strlen(control_pdu) / 2;
    btstack_parse_hex(control_pdu, transport_pdu_len, transport_pdu_data);

    mesh_upper_transport_access_send(netkey_index, appkey_index, ttl, src, dest, transport_pdu_data, transport_pdu_len, szmic);

    // check for all network pdus
    int i;
    for (i=0;i<count;i++){
        // parse expected network pdu
        test_network_pdu_len = strlen(network_pdus[i]) / 2;
        btstack_parse_hex(network_pdus[i], test_network_pdu_len, test_network_pdu_data);

        while (sent_network_pdu_len == 0) {
            mock_process_hci_cmd();
        }

        if (sent_network_pdu_len != test_network_pdu_len){
            printf("Test Network PDU (%u): ", sent_network_pdu_len); printf_hexdump(sent_network_pdu_data, sent_network_pdu_len);
            printf("Expected     PDU (%u): ", test_network_pdu_len); printf_hexdump(test_network_pdu_data, test_network_pdu_len);
        }
        CHECK_EQUAL( sent_network_pdu_len, test_network_pdu_len);
        CHECK_EQUAL_ARRAY(test_network_pdu_data, sent_network_pdu_data, test_network_pdu_len);

        sent_network_pdu_len = 0;

        // trigger next
        adv_bearer_emit_sent();
    }
}

void test_send_control_message(uint16_t netkey_index, uint8_t ttl, uint16_t src, uint16_t dest, char * control_pdu, int count, char ** lower_transport_pdus, char ** network_pdus){

    sent_network_pdu_len = 0;

    transport_pdu_len = strlen(control_pdu) / 2;
    btstack_parse_hex(control_pdu, transport_pdu_len, transport_pdu_data);

    uint8_t opcode = transport_pdu_data[0];
    mesh_upper_transport_send_control_pdu(netkey_index, ttl, src, dest, opcode, &transport_pdu_data[1], transport_pdu_len - 1);

    // check for all network pdus
    int i;
    for (i=0;i<count;i++){
        // expected network pdu
        test_network_pdu_len = strlen(network_pdus[i]) / 2;
        btstack_parse_hex(network_pdus[i], test_network_pdu_len, test_network_pdu_data);

        while (sent_network_pdu_len == 0) {
            mock_process_hci_cmd();
        }

        if (sent_network_pdu_len != test_network_pdu_len){
            printf("Test Network PDU (%u): ", sent_network_pdu_len); printf_hexdump(sent_network_pdu_data, sent_network_pdu_len);
            printf("Expected     PDU (%u): ", test_network_pdu_len); printf_hexdump(test_network_pdu_data, test_network_pdu_len);
        }
        CHECK_EQUAL( sent_network_pdu_len, test_network_pdu_len);
        CHECK_EQUAL_ARRAY(test_network_pdu_data, sent_network_pdu_data, test_network_pdu_len);

        sent_network_pdu_len = 0;

        // trigger next
        adv_bearer_emit_sent();
    }
}

// Message 1
char * message1_network_pdus[] = {
    (char *) "68eca487516765b5e5bfdacbaf6cb7fb6bff871f035444ce83a670df"
};
char * message1_lower_transport_pdus[] = {
    (char *) "034b50057e400000010000",
};
char * message1_upper_transport_pdu = (char *) "034b50057e400000010000";
TEST(MessageTest, Message1Receive){
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(1, message1_network_pdus, message1_lower_transport_pdus, message1_upper_transport_pdu);
}
TEST(MessageTest, Message1Send){
    uint16_t netkey_index = 0;
    uint8_t  ttl          = 0;
    uint16_t src          = 0x1201;
    uint16_t dest         = 0xfffd;
    uint32_t seq          = 1;
    mesh_set_iv_index(0x12345678);
    mesh_upper_transport_set_seq(seq);
    test_send_control_message(netkey_index, ttl, src, dest, message1_upper_transport_pdu, 1, message1_lower_transport_pdus, message1_network_pdus);
}

// Message 2
char * message2_network_pdus[] = {
    (char *) "68d4c826296d7979d7dbc0c9b4d43eebec129d20a620d01e"
};
char * message2_lower_transport_pdus[] = {
    (char *) "04320308ba072f",
};
char * message2_upper_transport_pdu = (char *) "04320308ba072f";
TEST(MessageTest, Message2Receive){
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(1, message2_network_pdus, message2_lower_transport_pdus, message2_upper_transport_pdu);
}
TEST(MessageTest, Message2Send){
    uint16_t netkey_index = 0;
    uint8_t  ttl          = 0;
    uint16_t src          = 0x2345;
    uint16_t dest         = 0x1201;
    uint32_t seq          = 0x014820;
    mesh_set_iv_index(0x12345678);
    mesh_upper_transport_set_seq(seq);
    test_send_control_message(netkey_index, ttl, src, dest, message2_upper_transport_pdu, 1, message2_lower_transport_pdus, message2_network_pdus);
}

// Message 3
char * message3_network_pdus[] = {
    (char *) "68da062bc96df253273086b8c5ee00bdd9cfcc62a2ddf572"
};
char * message3_lower_transport_pdus[] = {
    (char *) "04fa0205a6000a",
};
char * message3_upper_transport_pdu = (char *) "04fa0205a6000a";
TEST(MessageTest, Message3Receive){
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(1, message3_network_pdus, message3_lower_transport_pdus, message3_upper_transport_pdu);
}
TEST(MessageTest, Message3Send){
    uint16_t netkey_index = 0;
    uint8_t  ttl          = 0;
    uint16_t src          = 0x2fe3;
    uint16_t dest         = 0x1201;
    uint32_t seq          = 0x2b3832;
    mesh_set_iv_index(0x12345678);
    mesh_upper_transport_set_seq(seq);
    test_send_control_message(netkey_index, ttl, src, dest, message3_upper_transport_pdu, 1, message3_lower_transport_pdus, message3_network_pdus);
}

// Message 4
char * message4_network_pdus[] = {
    (char *) "5e84eba092380fb0e5d0ad970d579a4e88051c"
};
char * message4_lower_transport_pdus[] = {
    (char *) "0100",
};
char * message4_upper_transport_pdu = (char *) "0100";
TEST(MessageTest, Message4Receive){
    load_network_key_nid_5e();
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(1, message4_network_pdus, message4_lower_transport_pdus, message4_upper_transport_pdu);
}
TEST(MessageTest, Message4Send){
    uint16_t netkey_index = 0;
    uint8_t  ttl          = 0;
    uint16_t src          = 0x1201;
    uint16_t dest         = 0x2345;
    uint32_t seq          = 0x000002;
    load_network_key_nid_5e();
    mesh_set_iv_index(0x12345678);
    mesh_upper_transport_set_seq(seq);
    test_send_control_message(netkey_index, ttl, src, dest, message4_upper_transport_pdu, 1, message4_lower_transport_pdus, message4_network_pdus);
}

// Message 5
char * message5_network_pdus[] = {
    (char *) "5eafd6f53c43db5c39da1792b1fee9ec74b786c56d3a9dee",
};
char * message5_lower_transport_pdus[] = {
    (char *) "02001234567800",
};
char * message5_upper_transport_pdu = (char *) "02001234567800";
TEST(MessageTest, Message5Receive){
    load_network_key_nid_5e();
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(1, message5_network_pdus, message5_lower_transport_pdus, message5_upper_transport_pdu);
}
TEST(MessageTest, Message5Send){
    uint16_t netkey_index = 0;
    uint8_t  ttl          = 0;
    uint16_t src          = 0x2345;
    uint16_t dest         = 0x1201;
    uint32_t seq          = 0x014834;
    load_network_key_nid_5e();
    mesh_set_iv_index(0x12345678);
    mesh_upper_transport_set_seq(seq);
    test_send_control_message(netkey_index, ttl, src, dest, message5_upper_transport_pdu, 1, message5_lower_transport_pdus, message5_network_pdus);
}

// Message 6
char * message6_network_pdus[] = {
    (char *) "68cab5c5348a230afba8c63d4e686364979deaf4fd40961145939cda0e",
    (char *) "681615b5dd4a846cae0c032bf0746f44f1b8cc8ce5edc57e55beed49c0",
};
char * message6_lower_transport_pdus[] = {
    (char *) "8026ac01ee9dddfd2169326d23f3afdf",
    (char *) "8026ac21cfdc18c52fdef772e0e17308",
};
char * message6_upper_transport_pdu = (char *) "0056341263964771734fbd76e3b40519d1d94a48";
TEST(MessageTest, Message6Receive){
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(2, message6_network_pdus, message6_lower_transport_pdus, message6_upper_transport_pdu);
}
TEST(MessageTest, Message6Send){
    uint16_t netkey_index = 0;
    uint16_t appkey_index = MESH_DEVICE_KEY_INDEX;
    uint8_t  ttl          = 4;
    uint16_t src          = 0x0003;
    uint16_t dest         = 0x1201;
    uint32_t seq          = 0x3129ab;
    uint8_t  szmic        = 0;

    mesh_set_iv_index(0x12345678);
    mesh_upper_transport_set_seq(seq);
    test_send_access_message(netkey_index, appkey_index, ttl, src, dest, szmic, message6_upper_transport_pdu, 2, message6_lower_transport_pdus, message6_network_pdus);
}

// Message 7 - ACK
char * message7_network_pdus[] = {
    (char *) "68e476b5579c980d0d730f94d7f3509df987bb417eb7c05f",
};
char * message7_lower_transport_pdus[] = {
    (char *) "00a6ac00000002",
};
char * message7_upper_transport_pdu = (char *) "00a6ac00000002";
TEST(MessageTest, Message7Send){
    uint16_t netkey_index = 0;
    uint16_t appkey_index = MESH_DEVICE_KEY_INDEX;
    uint8_t  ttl          = 0x0b;
    uint16_t src          = 0x2345;
    uint16_t dest         = 0x0003;
    uint32_t seq          = 0x014835;
    uint8_t  szmic        = 0;

    mesh_set_iv_index(0x12345678);
    mesh_upper_transport_set_seq(seq);
    test_send_control_message(netkey_index, ttl, src, dest, message7_upper_transport_pdu, 1, message7_lower_transport_pdus, message7_network_pdus);
}
// ACK message, handled in mesh_transport - can be checked with test_control_receive_network_pdu
// TEST(MessageTest, Message7Receive){
//     mesh_set_iv_index(0x12345678);
//     test_receive_network_pdus(1, message7_network_pdus, message7_lower_transport_pdus, message7_upper_transport_pdu);
// }

// Message 8 - ACK
char * message8_network_pdus[] = {
    (char *) "684daa6267c2cf0e2f91add6f06e66006844cec97f973105ae2534f958",
};
char * message8_lower_transport_pdus[] = {
    (char *) "8026ac01ee9dddfd2169326d23f3afdf",
};
char * message8_upper_transport_pdu = (char *) "8026ac01ee9dddfd2169326d23f3afdf";
// ACK message, handled in mesh_transport - can be checked with test_control_receive_network_pdu
// TEST(MessageTest, Message8Receive){
//     mesh_set_iv_index(0x12345678);
//     test_receive_network_pdus(1, message8_network_pdus, message8_lower_transport_pdus, message8_upper_transport_pdu);
// }

// Message 9 - ACK

// Message 10
char * message10_network_pdus[] = {
    (char *) "5e7b786568759f7777ed355afaf66d899c1e3d",
};
char * message10_lower_transport_pdus[] = {
    (char *) "0101",
};
char * message10_upper_transport_pdu = (char *) "0101";
TEST(MessageTest, Message10Receive){
    load_network_key_nid_5e();
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(1, message10_network_pdus, message10_lower_transport_pdus, message10_upper_transport_pdu);
}
TEST(MessageTest, Message10Send){
    uint16_t netkey_index = 0;
    uint8_t  ttl          = 0;
    uint16_t src          = 0x1201;
    uint16_t dest         = 0x2345;
    uint32_t seq          = 0x000003;
    uint8_t  szmic        = 0;

    load_network_key_nid_5e();
    mesh_set_iv_index(0x12345678);
    mesh_upper_transport_set_seq(seq);
    test_send_control_message(netkey_index, ttl, src, dest, message10_upper_transport_pdu, 1, message10_lower_transport_pdus, message10_network_pdus);
}

// Message 11
// The Friend node responds to this poll with the first segment of the stored message. It also indicates that it has more data.

// Message 12
char * message12_network_pdus[] = {
    (char *) "5e8a18fc6e4d05ae21466087599c2426ce9a35",
};
char * message12_lower_transport_pdus[] = {
    (char *) "0101",
};
char * message12_upper_transport_pdu = (char *) "0101";
TEST(MessageTest, Message12Receive){
    load_network_key_nid_5e();
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(1, message12_network_pdus, message12_lower_transport_pdus, message12_upper_transport_pdu);
}
TEST(MessageTest, Message12Send){
    uint16_t netkey_index = 0;
    uint8_t  ttl          = 0;
    uint16_t src          = 0x1201;
    uint16_t dest         = 0x2345;
    uint32_t seq          = 0x000004;
    uint8_t  szmic        = 0;

    load_network_key_nid_5e();
    mesh_set_iv_index(0x12345678);
    mesh_upper_transport_set_seq(seq);
    test_send_control_message(netkey_index, ttl, src, dest, message12_upper_transport_pdu, 1, message12_lower_transport_pdus, message12_network_pdus);
}

// Message 13
// The Friend node responds with the same message as last time.
// Message 14
// The Low Power node received the retransmitted stored message. As that message has the MD bit set
// it sends another Friend Poll to obtain the next message.
char * message14_network_pdus[] = {
    (char *) "5e0bbaf92b5c8f7d3ae62a3c75dff683dce24e",
};
char * message14_lower_transport_pdus[] = {
    (char *) "0100",
};
char * message14_upper_transport_pdu = (char *) "0100";
TEST(MessageTest, Message14Receive){
    load_network_key_nid_5e();
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(1, message14_network_pdus, message14_lower_transport_pdus, message14_upper_transport_pdu);
}
TEST(MessageTest, Messagee14Send){
    uint16_t netkey_index = 0;
    uint8_t  ttl          = 0;
    uint16_t src          = 0x1201;
    uint16_t dest         = 0x2345;
    uint32_t seq          = 0x000005;
    uint8_t  szmic        = 0;

    load_network_key_nid_5e();
    mesh_set_iv_index(0x12345678);
    mesh_upper_transport_set_seq(seq);
    test_send_control_message(netkey_index, ttl, src, dest, message14_upper_transport_pdu, 1, message14_lower_transport_pdus, message14_network_pdus);
}

// Message 15
// The Friend node responds, with the next message in the friend queue. The Friend node has no more data, so it sets the MD to 0.
char * message15_network_pdus[] = {
    (char *) "5ea8dab50e7ee7f1d29805664d235eacd707217dedfe78497fefec7391",
};
char * message15_lower_transport_pdus[] = {
    (char *) "8026ac21cfdc18c52fdef772e0e17308",
};
char * message15_upper_transport_pdu = (char *) "0100";
// ACK message, handled in mesh_transport - can be checked with test_control_receive_network_pdu
// not sure - no upper access message
// TEST(MessageTest, Message15Receive){
//     load_network_key_nid_5e();
//     mesh_set_iv_index(0x12345678);
//     test_receive_network_pdus(1, message15_network_pdus, message15_lower_transport_pdus, message15_upper_transport_pdu);
// }

// Message 16
char * message16_network_pdus[] = {
    (char *) "68e80e5da5af0e6b9be7f5a642f2f98680e61c3a8b47f228",
};
char * message16_lower_transport_pdus[] = {
    (char *) "0089511bf1d1a81c11dcef",
};
char * message16_upper_transport_pdu = (char *) "800300563412";
TEST(MessageTest, Message16Receive){
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(1, message16_network_pdus, message16_lower_transport_pdus, message16_upper_transport_pdu);
}
TEST(MessageTest, Message16Send){
    uint16_t netkey_index = 0;
    uint16_t appkey_index = MESH_DEVICE_KEY_INDEX;
    uint8_t  ttl          = 0x0b;
    uint16_t src          = 0x1201;
    uint16_t dest         = 0x0003;
    uint32_t seq          = 0x000006;
    uint8_t  szmic        = 0;

    mesh_set_iv_index(0x12345678);
    mesh_upper_transport_set_seq(seq);
    test_send_access_message(netkey_index, appkey_index, ttl, src, dest, szmic, message16_upper_transport_pdu, 1, message16_lower_transport_pdus, message16_network_pdus);
}

// Message 17
// A Relay node receives the message from the Low Power node and relays it, decrementing the TTL value.
// Message 18
char * message18_network_pdus[] = {
    (char *) "6848cba437860e5673728a627fb938535508e21a6baf57",
};
char * message18_lower_transport_pdus[] = {
    (char *) "665a8bde6d9106ea078a",
};
char * message18_upper_transport_pdu = (char *) "0400000000";
TEST(MessageTest, Message18Receive){
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(1, message18_network_pdus, message18_lower_transport_pdus, message18_upper_transport_pdu);
}
TEST(MessageTest, Message18Send){
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;
    uint8_t  ttl          = 3;
    uint16_t src          = 0x1201;
    uint16_t dest         = 0xffff;
    uint32_t seq          = 0x00007;
    uint8_t  szmic        = 0;

    mesh_set_iv_index(0x12345678);
    mesh_upper_transport_set_seq(seq);
    test_send_access_message(netkey_index, appkey_index, ttl, src, dest, szmic, message18_upper_transport_pdu, 1, message18_lower_transport_pdus, message18_network_pdus);
}

// Message 19
// The Low Power node sends another Health Current Status message indicating that there are three faults:
// Battery Low Warning, Power Supply Interrupted Warning, and Supply Voltage Too Low Warning.
char * message19_network_pdus[] = {
    (char *) "68110edeecd83c3010a05e1b23a926023da75d25ba91793736",
};
char * message19_lower_transport_pdus[] = {
    (char *) "66ca6cd88e698d1265f43fc5",
};
char * message19_upper_transport_pdu = (char *) "04000000010703";
TEST(MessageTest, Message19Receive){
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(1, message19_network_pdus, message19_lower_transport_pdus, message19_upper_transport_pdu);
}
TEST(MessageTest, Message19Send){
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;
    uint8_t  ttl          = 3;
    uint16_t src          = 0x1201;
    uint16_t dest         = 0xffff;
    uint32_t seq          = 0x00009;
    uint8_t  szmic        = 0;

    mesh_set_iv_index(0x12345678);
    mesh_upper_transport_set_seq(seq);
    test_send_access_message(netkey_index, appkey_index, ttl, src, dest, szmic, message19_upper_transport_pdu, 1, message19_lower_transport_pdus, message19_network_pdus);
}

// Message 20
char * message20_network_pdus[] = {
    (char *) "e85cca51e2e8998c3dc87344a16c787f6b08cc897c941a5368",
};
char * message20_lower_transport_pdus[] = {
    (char *) "669c9803e110fea929e9542d",
};
char * message20_upper_transport_pdu = (char *) "04000000010703";
TEST(MessageTest, Message20Receive){
    mesh_set_iv_index(0x12345677);
    test_receive_network_pdus(1, message20_network_pdus, message20_lower_transport_pdus, message20_upper_transport_pdu);
}
TEST(MessageTest, Message20Send){
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;
    uint8_t  ttl          = 3;
    uint16_t src          = 0x1234;
    uint16_t dest         = 0xffff;
    uint32_t seq          = 0x070809;
    uint8_t  szmic        = 0;

    mesh_set_iv_index(0x12345677);
    mesh_upper_transport_set_seq(seq);
    test_send_access_message(netkey_index, appkey_index, ttl, src, dest, szmic, message20_upper_transport_pdu, 1, message20_lower_transport_pdus, message20_network_pdus);
}

// Message 21
char * message21_network_pdus[] = {
    (char *) "e8b1051f5e945ae4d611358eaf17796a6c98977f69e5872c4620",
};
char * message21_lower_transport_pdus[] = {
    (char *) "662fa730fd98f6e4bd120ea9d6",
};
char * message21_upper_transport_pdu = (char *) "d50a0048656c6c6f";
TEST(MessageTest, Message21Receive){
    mesh_set_iv_index(0x12345677);
    test_receive_network_pdus(1, message21_network_pdus, message21_lower_transport_pdus, message21_upper_transport_pdu);
}
TEST(MessageTest, Message21Send){
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;
    uint8_t  ttl          = 3;
    uint16_t src          = 0x1234;
    uint16_t dest         = 0x8105;
    uint32_t seq          = 0x07080a;
    uint8_t  szmic        = 0;

    mesh_set_iv_index(0x12345677);
    mesh_upper_transport_set_seq(seq);
    test_send_access_message(netkey_index, appkey_index, ttl, src, dest, szmic, message21_upper_transport_pdu, 1, message21_lower_transport_pdus, message21_network_pdus);
}

#if 0
// Message 22
// char * message22_network_pdus[] = {
//     (char *) "e8d85caecef1e3ed31f3fdcf88a411135fea55df730b6b28e255",
// };
// char * message22_lower_transport_pdus[] = {
//     (char *) "663871b904d431526316ca48a0",
// };
// char * message22_upper_transport_pdu = (char *) "d50a0048656c6c6f";
// TEST(MessageTest, Message22Receive){
//     mesh_set_iv_index(0x12345677);
//     test_receive_network_pdus(1, message22_network_pdus, message22_lower_transport_pdus, message22_upper_transport_pdu);
// }

// buggy?

// Access Payload       D5 0A 00 48 65 6C 6C 6F
// AppOrDevKey          63 96 47 71 73 4F BD 76 E3 B4 05 19 D1 D9 4A 48
// AppNonce             01 00 07 08 0B 12 34 B5 29 12 34 56 77
// EncAccessPayload     38 71 B9 04 D4 31 52 63
// TransMIC             03 55 18 C4

// SPEC: TransMIC       16ca48a0

TEST(MessageTest, Message22Send){
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;
    uint8_t  ttl          = 3;
    uint16_t src          = 0x1234;
    uint16_t dest         = 0xb529;
    uint32_t seq          = 0x07080b;
    uint8_t  szmic        = 0;

    mesh_set_iv_index(0x12345677);
    mesh_upper_transport_set_seq(seq);
    test_send_access_message(netkey_index, appkey_index, ttl, src, dest, szmic, message22_upper_transport_pdu, 1, message22_lower_transport_pdus, message22_network_pdus);
}
#endif

#if 0
// Message 23
char * message23_network_pdus[] = {
    (char *) "e877a48dd5fe2d7a9d696d3dd16a75489696f0b70c711b881385",
};
char * message23_lower_transport_pdus[] = {
    (char *) "662456db5e3100eef65daa7a38",
};
char * message23_upper_transport_pdu = (char *) "d50a0048656c6c6f";
// buggy?

// Access Payload       D5 0A 00 48 65 6C 6C 6F
// AppOrDevKey          63 96 47 71 73 4F BD 76 E3 B4 05 19 D1 D9 4A 48
// AppNonce             01 00 07 08 0C 12 34 97 36 12 34 56 77
// EncAccessPayload     24 56 DB 5E 31 00 EE F6
// TransMIC             26 04 3E 26

// SPEC: TransMIC       5daa7a38

TEST(MessageTest, Message23Receive){
    mesh_set_iv_index(0x12345677);
    test_receive_network_pdus(1, message23_network_pdus, message23_lower_transport_pdus, message23_upper_transport_pdu);
}
TEST(MessageTest, Message23Send){
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;
    uint8_t  ttl          = 3;
    uint16_t src          = 0x1234;
    uint16_t dest         = 0x9736;
    uint32_t seq          = 0x07080c;
    uint8_t  szmic        = 0;

    mesh_set_iv_index(0x12345677);
    mesh_upper_transport_set_seq(seq);
    test_send_access_message(netkey_index, appkey_index, ttl, src, dest, szmic, message23_upper_transport_pdu, 1, message23_lower_transport_pdus, message23_network_pdus);
}
#endif

#if 0
// Message 24
// buggy?
// ApplicationNonce (spec): 010007080d1234973612345677
// ApplicationNonce (test): 018007080D1234973612345677
// ApplicationNonce[1] = (SZMIC << 7)  - SZMIC if a Segmented Access message or 0 for all other message formats
char * message24_network_pdus[] = {
    (char *) "e834586babdef394e998b4081f5a7308ce3edbb3b06cdecd028e307f1c",
    (char *) "e85115af73dcfddc2f4dd6fb4d328701291be4aafe",
};
char * message24_lower_transport_pdus[] = {
    (char *) "e6a03401de1547118463123e5f6a17b9",
    (char *) "e6a034219dbca387",
};
char * message24_upper_transport_pdu = (char *) "ea0a00576f726c64";
TEST(MessageTest, Message24Receive){
    mesh_set_iv_index(0x12345677);
    test_receive_network_pdus(2, message24_network_pdus, message24_lower_transport_pdus, message24_upper_transport_pdu);
}
TEST(MessageTest, Message24Send){
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;
    uint8_t  ttl          = 3;
    uint16_t src          = 0x1234;
    uint16_t dest         = 0x9736;
    uint32_t seq          = 0x07080d;
    uint8_t  szmic        = 1;

    mesh_set_iv_index(0x12345677);
    mesh_upper_transport_set_seq(seq);
    test_send_access_message(netkey_index, appkey_index, ttl, src, dest, szmic, message24_upper_transport_pdu, 2, message24_lower_transport_pdus, message24_network_pdus);
}
#endif

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
