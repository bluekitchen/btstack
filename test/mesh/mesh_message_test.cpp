#include <stdio.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_debug.h"
#include "btstack_memory.h"
#include "btstack_util.h"
#include "mesh/adv_bearer.h"
#include "mesh/gatt_bearer.h"
#include "mesh/mesh_access.h"
#include "mesh/mesh_crypto.h"
#include "mesh/mesh_foundation.h"
#include "mesh/mesh_iv_index_seq_number.h"
#include "mesh/mesh_lower_transport.h"
#include "mesh/mesh_network.h"
#include "mesh/mesh_upper_transport.h"
#include "mesh/provisioning.h"
#include "mesh/mesh_peer.h"
#include "mock.h"


static mesh_network_pdu_t * received_network_pdu;
static mesh_network_pdu_t * received_proxy_pdu;

static uint8_t outgoing_gatt_network_pdu_data[29];
static uint8_t outgoing_gatt_network_pdu_len;

static uint8_t outgoing_adv_network_pdu_data[29];
static uint8_t outgoing_adv_network_pdu_len;

static uint8_t  recv_upper_transport_pdu_data[100];
static uint16_t recv_upper_transport_pdu_len;

#ifdef ENABLE_MESH_ADV_BEARER
static btstack_packet_handler_t adv_packet_handler;
void adv_bearer_register_for_network_pdu(btstack_packet_handler_t packet_handler){
    adv_packet_handler = packet_handler;
}
void adv_bearer_request_can_send_now_for_network_pdu(void){
    // simulate can send now
    uint8_t event[3];
    event[0] = HCI_EVENT_MESH_META;
    event[1] = 1;
    event[2] = MESH_SUBEVENT_CAN_SEND_NOW;
    (*adv_packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}
void adv_bearer_send_network_pdu(const uint8_t * network_pdu, uint16_t size, uint8_t count, uint16_t interval){
    (void) count;
    (void) interval;
    // printf("ADV Network PDU: ");
    // printf_hexdump(network_pdu, size);
    memcpy(outgoing_adv_network_pdu_data, network_pdu, size);
    outgoing_adv_network_pdu_len = size;
}
static void adv_bearer_emit_sent(void){
    uint8_t event[3];
    event[0] = HCI_EVENT_MESH_META;
    event[1] = 1;
    event[2] = MESH_SUBEVENT_MESSAGE_SENT;
    (*adv_packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}
#endif

#ifdef ENABLE_MESH_GATT_BEARER
static btstack_packet_handler_t gatt_packet_handler;
void gatt_bearer_register_for_network_pdu(btstack_packet_handler_t packet_handler){
    gatt_packet_handler = packet_handler;
}
void gatt_bearer_register_for_mesh_proxy_configuration(btstack_packet_handler_t packet_handler){
    UNUSED(packet_handler);
}
void gatt_bearer_request_can_send_now_for_network_pdu(void){
    // simulate can send now
    uint8_t event[3];
    event[0] = HCI_EVENT_MESH_META;
    event[1] = 1;
    event[2] = MESH_SUBEVENT_CAN_SEND_NOW;
    (*gatt_packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}
void gatt_bearer_send_network_pdu(const uint8_t * network_pdu, uint16_t size){
    // printf("ADV Network PDU: ");
    // printf_hexdump(network_pdu, size);
    memcpy(outgoing_gatt_network_pdu_data, network_pdu, size);
    outgoing_gatt_network_pdu_len = size;
}
static void gatt_bearer_emit_sent(void){
    uint8_t event[3];
    event[0] = HCI_EVENT_MESH_META;
    event[1] = 1;
    event[2] = MESH_SUBEVENT_MESSAGE_SENT;
    (*gatt_packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}
static void gatt_bearer_emit_connected(void){
    uint8_t event[5];
    event[0] = HCI_EVENT_MESH_META;
    event[1] = 1;
    event[2] = MESH_SUBEVENT_PROXY_CONNECTED;
    little_endian_store_16(event, 3, 0x1234);
    (*gatt_packet_handler)(HCI_EVENT_PACKET, 0, &event[0], sizeof(event));
}
#endif

// copy from mesh_message.c for now
uint16_t mesh_pdu_dst(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_UNSEGMENTED:
        case MESH_PDU_TYPE_NETWORK:
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_CONTROL:
            return mesh_network_dst((mesh_network_pdu_t *) pdu);
        case MESH_PDU_TYPE_ACCESS: {
            return ((mesh_access_pdu_t *) pdu)->dst;
        }
        case MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS:
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS:
            return ((mesh_upper_transport_pdu_t *) pdu)->dst;
        default:
            btstack_assert(false);
            return MESH_ADDRESS_UNSASSIGNED;
    }
}
uint16_t mesh_pdu_ctl(mesh_pdu_t * pdu){
    switch (pdu->pdu_type){
        case MESH_PDU_TYPE_NETWORK:
        case MESH_PDU_TYPE_UPPER_UNSEGMENTED_CONTROL:
            return mesh_network_control((mesh_network_pdu_t *) pdu);
        case MESH_PDU_TYPE_ACCESS: {
            return ((mesh_access_pdu_t *) pdu)->ctl_ttl >> 7;
        }
        default:
            btstack_assert(false);
            return 0;
    }
}

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

static int scan_hex_byte(const char * byte_string){
    uint8_t upper_nibble = nibble_for_char(*byte_string++);
    if (upper_nibble < 0) return -1;
    uint8_t lower_nibble = nibble_for_char(*byte_string);
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
        // don't check separator after last byte
        if (i == len - 1) {
            return 1;
        }
        // optional seperator
        char separator = *string;
        if (separator == ':' || separator == '-' || separator == ' ') {
            string++;
        }
    }
    return 1;
}

#if 0
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
#endif

static mesh_transport_key_t   test_application_key;
static void mesh_application_key_set(uint16_t netkey_index, uint16_t appkey_index, uint8_t aid, const uint8_t *application_key) {
    test_application_key.netkey_index = netkey_index;
    test_application_key.appkey_index = appkey_index;
    test_application_key.aid   = aid;
    test_application_key.akf   = 1;
    memcpy(test_application_key.key, application_key, 16);
    mesh_transport_key_add(&test_application_key);
}

static void load_network_key_nid_68(void){
    mesh_network_key_t * network_key = btstack_memory_mesh_network_key_get();
    network_key->nid = 0x68;
    btstack_parse_hex("0953fa93e7caac9638f58820220a398e", 16, network_key->encryption_key);
    btstack_parse_hex("8b84eedec100067d670971dd2aa700cf", 16, network_key->privacy_key);
    mesh_network_key_add(network_key);
    mesh_subnet_setup_for_netkey_index(network_key->netkey_index);
}

static void load_network_key_nid_5e(void){
    mesh_network_key_t * network_key = btstack_memory_mesh_network_key_get();
    network_key->nid = 0x5e;
    btstack_parse_hex("be635105434859f484fc798e043ce40e", 16, network_key->encryption_key);
    btstack_parse_hex("5d396d4b54d3cbafe943e051fe9a4eb8", 16, network_key->privacy_key);
    mesh_network_key_add(network_key);
    mesh_subnet_setup_for_netkey_index(network_key->netkey_index);
}

static void load_network_key_nid_10(void){
    mesh_network_key_t * network_key = btstack_memory_mesh_network_key_get();
    network_key->nid = 0x10;
    btstack_parse_hex("3a4fe84a6cc2c6a766ea93f1084d4039", 16, network_key->encryption_key);
    btstack_parse_hex("f695fcce709ccface4d8b7a1e6e39d25", 16, network_key->privacy_key);
    mesh_network_key_add(network_key);
    mesh_subnet_setup_for_netkey_index(network_key->netkey_index);
}

static void load_provisioning_data_test_message(void){
    uint8_t application_key[16];
    btstack_parse_hex("63964771734fbd76e3b40519d1d94a48", 16, application_key);
    mesh_application_key_set( 0, 0, 0x26, application_key);

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
            mesh_lower_transport_received_message(MESH_NETWORK_PDU_SENT, network_pdu);
            break;
        default:
            break;
    }
}

static void test_proxy_server_callback_handler(mesh_network_callback_type_t callback_type, mesh_network_pdu_t * network_pdu){
    switch (callback_type){
        case MESH_NETWORK_PDU_RECEIVED:
            printf("test MESH_PROXY_PDU_RECEIVED\n");
            received_proxy_pdu = network_pdu;
            break;
        case MESH_NETWORK_PDU_SENT:
            // printf("test MESH_PROXY_PDU_SENT\n");
            // mesh_lower_transport_received_mesage(MESH_NETWORK_PDU_SENT, network_pdu);
            break;
        case MESH_NETWORK_PDU_ENCRYPTED:
            printf("test MESH_NETWORK_PDU_ENCRYPTED\n");
            received_proxy_pdu = network_pdu;
            break;
        default:
            break;
    }
}

static void test_upper_transport_access_message_handler(mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu){
    UNUSED(status);

    // free sent pdus
    if (callback_type == MESH_TRANSPORT_PDU_SENT) {
        mesh_upper_transport_pdu_free(pdu);
        return;
    }

    // process pdu received
    mesh_access_pdu_t    * access_pdu;
    mesh_network_pdu_t   * network_pdu;
    mesh_segmented_pdu_t   * message_pdu;

    switch(pdu->pdu_type){
        case MESH_PDU_TYPE_ACCESS:
            access_pdu = (mesh_access_pdu_t *) pdu;
            printf("test access handler MESH_PDU_TYPE_ACCESS received\n");
            recv_upper_transport_pdu_len = access_pdu->len;
            memcpy(recv_upper_transport_pdu_data, access_pdu->data, recv_upper_transport_pdu_len);
            mesh_upper_transport_message_processed_by_higher_layer(pdu);
            break;
        case MESH_PDU_TYPE_SEGMENTED:
            message_pdu = (mesh_segmented_pdu_t *) pdu;
            printf("test access handler MESH_PDU_TYPE_SEGMENTED received\n");
            network_pdu = (mesh_network_pdu_t *) btstack_linked_list_get_first_item(&message_pdu->segments);
            recv_upper_transport_pdu_len = mesh_network_pdu_len(network_pdu)  - 1;
            memcpy(recv_upper_transport_pdu_data, mesh_network_pdu_data(network_pdu) + 1, recv_upper_transport_pdu_len);
            mesh_upper_transport_message_processed_by_higher_layer(pdu);
            break;
        default:
            btstack_assert(0);
            break;
    }
}

static void test_upper_transport_control_message_handler(mesh_transport_callback_type_t callback_type, mesh_transport_status_t status, mesh_pdu_t * pdu){
    UNUSED(status);

    // ignore pdu sent
    if (callback_type == MESH_TRANSPORT_PDU_SENT) return;

    // process pdu received
    mesh_control_pdu_t     * control_pdu;
    switch(pdu->pdu_type){
        case MESH_PDU_TYPE_CONTROL:
            control_pdu = (mesh_control_pdu_t *) pdu;
            printf("test MESH_PDU_TYPE_CONTROL\n");
            recv_upper_transport_pdu_len = control_pdu->len + 1;
            recv_upper_transport_pdu_data[0] = control_pdu->akf_aid_control;
            memcpy(&recv_upper_transport_pdu_data[1], control_pdu->data, control_pdu->len);
            mesh_upper_transport_message_processed_by_higher_layer(pdu);
            break;
        default:
            btstack_assert(0);
            break;
    }
}

TEST_GROUP(MessageTest){
    void setup(void){
        btstack_memory_init();
        btstack_crypto_init();
        load_provisioning_data_test_message();
        mesh_network_init();
        mesh_lower_transport_init();
        mesh_upper_transport_init();
        mesh_network_key_init();
        // intercept messages between network and lower layer
        mesh_network_set_higher_layer_handler(&test_lower_transport_callback_handler);
        mesh_network_set_proxy_message_handler(&test_proxy_server_callback_handler);
        // register to receive upper transport messages
        mesh_upper_transport_register_access_message_handler(&test_upper_transport_access_message_handler);
        mesh_upper_transport_register_control_message_handler(&test_upper_transport_control_message_handler);
        mesh_seq_auth_reset();
#ifdef ENABLE_MESH_GATT_BEARER
        mesh_foundation_gatt_proxy_set(1);
        gatt_bearer_emit_connected();
#endif
        outgoing_gatt_network_pdu_len = 0;
        outgoing_adv_network_pdu_len = 0;
        received_network_pdu = NULL;
        recv_upper_transport_pdu_len =0;
    }
    void teardown(void){
        // printf("-- teardown start --\n\n");
        btstack_crypto_reset();
        mesh_network_reset();
        mesh_lower_transport_reset();
        mesh_upper_transport_dump();
        mesh_upper_transport_reset();
        // mesh_network_dump();
        // mesh_transport_dump();
        printf("-- teardown complete --\n\n");
    }
};

static uint8_t transport_pdu_data[64];
static uint16_t transport_pdu_len;

static     uint8_t test_network_pdu_len;
static uint8_t test_network_pdu_data[29];

static void test_receive_network_pdus(int count, char ** network_pdus, char ** lower_transport_pdus, char * access_pdu){
    int i;
    for (i=0;i<count;i++){
        test_network_pdu_len = strlen(network_pdus[i]) / 2;
        btstack_parse_hex(network_pdus[i], test_network_pdu_len, test_network_pdu_data);
    
        mesh_network_received_message(test_network_pdu_data, test_network_pdu_len, 0);

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
        mesh_lower_transport_received_message(MESH_NETWORK_PDU_RECEIVED, received_network_pdu);

        // done
        received_network_pdu = NULL;
    }

    // wait for transport pdu
    while (recv_upper_transport_pdu_len == 0) {
        mock_process_hci_cmd();

        // check for acks
        if (outgoing_gatt_network_pdu_len != 0){
            outgoing_gatt_network_pdu_len = 0;
            gatt_bearer_emit_sent();
        }
        if (outgoing_adv_network_pdu_len != 0){
            outgoing_adv_network_pdu_len = 0;
            adv_bearer_emit_sent();
        }
    }

    transport_pdu_len = strlen(access_pdu) / 2;
    btstack_parse_hex(access_pdu, transport_pdu_len, transport_pdu_data);

    printf("UpperTransportPDU: ");
    printf_hexdump(recv_upper_transport_pdu_data, recv_upper_transport_pdu_len);
    CHECK_EQUAL( transport_pdu_len, recv_upper_transport_pdu_len);
    CHECK_EQUAL_ARRAY(transport_pdu_data, recv_upper_transport_pdu_data, transport_pdu_len);
}

static void expect_gatt_network_pdu(void){

        while (outgoing_gatt_network_pdu_len == 0) {
            mock_process_hci_cmd();
        }

        if (outgoing_gatt_network_pdu_len != test_network_pdu_len){
            printf("Test Network PDU (%u): ", outgoing_gatt_network_pdu_len); printf_hexdump(outgoing_gatt_network_pdu_data, outgoing_gatt_network_pdu_len);
            printf("Expected     PDU (%u): ", test_network_pdu_len); printf_hexdump(test_network_pdu_data, test_network_pdu_len);
        }
        CHECK_EQUAL( outgoing_gatt_network_pdu_len, test_network_pdu_len);
        CHECK_EQUAL_ARRAY(test_network_pdu_data, outgoing_gatt_network_pdu_data, test_network_pdu_len);

        outgoing_gatt_network_pdu_len = 0;
        gatt_bearer_emit_sent();
}

static void expect_adv_network_pdu(void){
        while (outgoing_adv_network_pdu_len == 0) {
            mock_process_hci_cmd();
        }

        if (outgoing_adv_network_pdu_len != test_network_pdu_len){
            printf("Test Network PDU (%u): ", outgoing_adv_network_pdu_len); printf_hexdump(outgoing_adv_network_pdu_data, outgoing_adv_network_pdu_len);
            printf("Expected     PDU (%u): ", test_network_pdu_len); printf_hexdump(test_network_pdu_data, test_network_pdu_len);
        }
        CHECK_EQUAL( outgoing_adv_network_pdu_len, test_network_pdu_len);
        CHECK_EQUAL_ARRAY(test_network_pdu_data, outgoing_adv_network_pdu_data, test_network_pdu_len);

        outgoing_adv_network_pdu_len = 0;
        adv_bearer_emit_sent();
}

static void test_send_access_message(uint16_t netkey_index, uint16_t appkey_index,  uint8_t ttl, uint16_t src, uint16_t dest, uint8_t szmic, char * control_pdu, int count, char ** lower_transport_pdus, char ** network_pdus){

    UNUSED(lower_transport_pdus);

    transport_pdu_len = strlen(control_pdu) / 2;
    btstack_parse_hex(control_pdu, transport_pdu_len, transport_pdu_data);

    mesh_pdu_type_t pdu_type;
    if (count == 1 ){
        // send as unsegmented access pdu
        pdu_type = MESH_PDU_TYPE_UPPER_UNSEGMENTED_ACCESS;
    } else {
        // send as segmented access pdu
        pdu_type = MESH_PDU_TYPE_UPPER_SEGMENTED_ACCESS;
    }
#if 0
    //
    upper_pdu.lower_pdu = NULL;
    upper_pdu.flags = 0;
    upper_pdu.pdu_type = pdu_type;
    mesh_pdu_t * pdu = (mesh_pdu_t *) &upper_pdu;
    mesh_upper_transport_setup_access_pdu(pdu, netkey_index, appkey_index, ttl, src, dest, szmic, transport_pdu_data, transport_pdu_len);
#else
    mesh_upper_transport_builder_t builder;
    mesh_upper_transport_message_init(&builder, pdu_type);
    mesh_upper_transport_message_add_data(&builder, transport_pdu_data, transport_pdu_len);
    mesh_pdu_t * pdu = (mesh_pdu_t *) mesh_upper_transport_message_finalize(&builder);
    mesh_upper_transport_setup_access_pdu_header(pdu, netkey_index, appkey_index, ttl, src, dest, szmic);
#endif
    mesh_upper_transport_send_access_pdu(pdu);

    // check for all network pdus
    int i;
    for (i=0;i<count;i++){
        // parse expected network pdu
        test_network_pdu_len = strlen(network_pdus[i]) / 2;
        btstack_parse_hex(network_pdus[i], test_network_pdu_len, test_network_pdu_data);

#ifdef ENABLE_MESH_GATT_BEARER
        expect_gatt_network_pdu();
#endif

#ifdef ENABLE_MESH_ADV_BEARER
        expect_adv_network_pdu();
#endif
    }
    // mesh_upper_transport_pdu_free(pdu);
}

static void test_send_control_message(uint16_t netkey_index, uint8_t ttl, uint16_t src, uint16_t dest, char * control_pdu, int count, char ** lower_transport_pdus, char ** network_pdus){

    UNUSED(lower_transport_pdus);

    transport_pdu_len = strlen(control_pdu) / 2;
    btstack_parse_hex(control_pdu, transport_pdu_len, transport_pdu_data);

    uint8_t opcode = transport_pdu_data[0];

    mesh_pdu_t * pdu;
    if (transport_pdu_len < 12){
        // send as unsegmented control pdu
        mesh_network_pdu_t * network_pdu = mesh_network_pdu_get();
        mesh_upper_transport_setup_unsegmented_control_pdu(network_pdu, netkey_index, ttl, src, dest, opcode, transport_pdu_data+1, transport_pdu_len-1);
        pdu = (mesh_pdu_t *) network_pdu;
    } else {
        mesh_upper_transport_builder_t builder;
        mesh_upper_transport_message_init(&builder, MESH_PDU_TYPE_UPPER_SEGMENTED_CONTROL);
        mesh_upper_transport_message_add_data(&builder, transport_pdu_data+1, transport_pdu_len-1);
        mesh_upper_transport_pdu_t * final_upper_pdu = (mesh_upper_transport_pdu_t *) mesh_upper_transport_message_finalize(&builder);
        mesh_upper_transport_setup_segmented_control_pdu_header(final_upper_pdu, netkey_index, ttl, src, dest, opcode);
        pdu = (mesh_pdu_t *) final_upper_pdu;
    }
    mesh_upper_transport_send_control_pdu(pdu);

    // check for all network pdus
    int i;
    for (i=0;i<count;i++){
        // expected network pdu
        test_network_pdu_len = strlen(network_pdus[i]) / 2;
        btstack_parse_hex(network_pdus[i], test_network_pdu_len, test_network_pdu_data);

#ifdef ENABLE_MESH_GATT_BEARER
        expect_gatt_network_pdu();
#endif

#ifdef ENABLE_MESH_ADV_BEARER
        expect_adv_network_pdu();
#endif

    }
}
#if 1
// Message 1
char * message1_network_pdus[] = {
    (char *) "68eca487516765b5e5bfdacbaf6cb7fb6bff871f035444ce83a670df"
};
char * message1_lower_transport_pdus[] = {
    (char *) "034b50057e400000010000",
};
char * message1_upper_transport_pdu = (char *) "034b50057e400000010000";
TEST(MessageTest, Message1Receive){
    load_network_key_nid_68();
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(1, message1_network_pdus, message1_lower_transport_pdus, message1_upper_transport_pdu);
}
TEST(MessageTest, Message1Send){
    uint16_t netkey_index = 0;
    uint8_t  ttl          = 0;
    uint16_t src          = 0x1201;
    uint16_t dest         = 0xfffd;
    uint32_t seq          = 1;
    load_network_key_nid_68();
    mesh_set_iv_index(0x12345678);
    mesh_sequence_number_set(seq);
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
    load_network_key_nid_68();
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(1, message2_network_pdus, message2_lower_transport_pdus, message2_upper_transport_pdu);
}
TEST(MessageTest, Message2Send){
    uint16_t netkey_index = 0;
    uint8_t  ttl          = 0;
    uint16_t src          = 0x2345;
    uint16_t dest         = 0x1201;
    uint32_t seq          = 0x014820;
    load_network_key_nid_68();
    mesh_set_iv_index(0x12345678);
    mesh_sequence_number_set(seq);
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
    load_network_key_nid_68();
    mesh_set_iv_index(0x12345678);
    test_receive_network_pdus(1, message3_network_pdus, message3_lower_transport_pdus, message3_upper_transport_pdu);
}
TEST(MessageTest, Message3Send){
    uint16_t netkey_index = 0;
    uint8_t  ttl          = 0;
    uint16_t src          = 0x2fe3;
    uint16_t dest         = 0x1201;
    uint32_t seq          = 0x2b3832;
    load_network_key_nid_68();
    mesh_set_iv_index(0x12345678);
    mesh_sequence_number_set(seq);
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
    mesh_sequence_number_set(seq);
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
    mesh_sequence_number_set(seq);
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
    load_network_key_nid_68();
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

    load_network_key_nid_68();
    mesh_set_iv_index(0x12345678);
    mesh_sequence_number_set(seq);
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
    uint8_t  ttl          = 0x0b;
    uint16_t src          = 0x2345;
    uint16_t dest         = 0x0003;
    uint32_t seq          = 0x014835;

    load_network_key_nid_68();
    mesh_set_iv_index(0x12345678);
    mesh_sequence_number_set(seq);
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

    load_network_key_nid_5e();
    mesh_set_iv_index(0x12345678);
    mesh_sequence_number_set(seq);
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

    load_network_key_nid_5e();
    mesh_set_iv_index(0x12345678);
    mesh_sequence_number_set(seq);
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
TEST(MessageTest, Message14Send){
    uint16_t netkey_index = 0;
    uint8_t  ttl          = 0;
    uint16_t src          = 0x1201;
    uint16_t dest         = 0x2345;
    uint32_t seq          = 0x000005;

    load_network_key_nid_5e();
    mesh_set_iv_index(0x12345678);
    mesh_sequence_number_set(seq);
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
    load_network_key_nid_68();
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

    load_network_key_nid_68();
    mesh_set_iv_index(0x12345678);
    mesh_sequence_number_set(seq);
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
    load_network_key_nid_68();
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

    load_network_key_nid_68();
    mesh_set_iv_index(0x12345678);
    mesh_sequence_number_set(seq);
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
    load_network_key_nid_68();
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

    load_network_key_nid_68();
    mesh_set_iv_index(0x12345678);
    mesh_sequence_number_set(seq);
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
    load_network_key_nid_68();
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

    load_network_key_nid_68();
    mesh_set_iv_index(0x12345677);
    mesh_sequence_number_set(seq);
    test_send_access_message(netkey_index, appkey_index, ttl, src, dest, szmic, message20_upper_transport_pdu, 1, message20_lower_transport_pdus, message20_network_pdus);
}

// Message 21
// The Low Power node sends a vendor command to a group address.
char * message21_network_pdus[] = {
    (char *) "e84e8fbe003f58a4d61157bb76352ea6307eebfe0f30b83500e9",
};
char * message21_lower_transport_pdus[] = {
    (char *) "664d92e9dfcf3ab85b6e8fcf03",
};
char * message21_upper_transport_pdu = (char *) "d50a0048656c6c6f";
TEST(MessageTest, Message21Receive){
    load_network_key_nid_68();
    mesh_set_iv_index(0x12345677);
    test_receive_network_pdus(1, message21_network_pdus, message21_lower_transport_pdus, message21_upper_transport_pdu);
}
TEST(MessageTest, Message21Send){
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;
    uint8_t  ttl          = 3;
    uint16_t src          = 0x1234;
    uint16_t dest         = 0xc105;
    uint32_t seq          = 0x07080a;
    uint8_t  szmic        = 0;

    load_network_key_nid_68();
    mesh_set_iv_index(0x12345677);
    mesh_sequence_number_set(seq);
    test_send_access_message(netkey_index, appkey_index, ttl, src, dest, szmic, message21_upper_transport_pdu, 1, message21_lower_transport_pdus, message21_network_pdus);
}

// Message 22
char * message22_network_pdus[] = {
    (char *) "e8d85caecef1e3ed31f3fdcf88a411135fea55df730b6b28e255",
};
char * message22_lower_transport_pdus[] = {
    (char *) "663871b904d431526316ca48a0",
};
char * message22_upper_transport_pdu = (char *) "d50a0048656c6c6f";
char * message22_label_string = (char *) "0073e7e4d8b9440faf8415df4c56c0e1";

TEST(MessageTest, Message22Receive){
    load_network_key_nid_68();
    mesh_set_iv_index(0x12345677);
    uint8_t label_uuid[16];
    btstack_parse_hex(message22_label_string, 16, label_uuid);
    mesh_virtual_address_register(label_uuid, 0xb529);
    test_receive_network_pdus(1, message22_network_pdus, message22_lower_transport_pdus, message22_upper_transport_pdu);
}

TEST(MessageTest, Message22Send){
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;
    uint8_t  ttl          = 3;
    uint16_t src          = 0x1234;
    uint32_t seq          = 0x07080b;
    uint8_t  szmic        = 0;

    load_network_key_nid_68();
    mesh_set_iv_index(0x12345677);
    mesh_sequence_number_set(seq);
    uint8_t label_uuid[16];
    btstack_parse_hex(message22_label_string, 16, label_uuid);
    mesh_virtual_address_t * virtual_address = mesh_virtual_address_register(label_uuid, 0xb529);
    uint16_t pseudo_dst = virtual_address->pseudo_dst;
    test_send_access_message(netkey_index, appkey_index, ttl, src, pseudo_dst, szmic, message22_upper_transport_pdu, 1, message22_lower_transport_pdus, message22_network_pdus);
}

// Message 23
char * message23_network_pdus[] = {
    (char *) "e877a48dd5fe2d7a9d696d3dd16a75489696f0b70c711b881385",
};
char * message23_lower_transport_pdus[] = {
    (char *) "662456db5e3100eef65daa7a38",
};
char * message23_upper_transport_pdu = (char *) "d50a0048656c6c6f";
char * message23_label_string = (char *) "f4a002c7fb1e4ca0a469a021de0db875";

TEST(MessageTest, Message23Receive){
    load_network_key_nid_68();
    mesh_set_iv_index(0x12345677);
    uint8_t label_uuid[16];
    btstack_parse_hex(message23_label_string, 16, label_uuid);
    mesh_virtual_address_register(label_uuid, 0x9736);
    test_receive_network_pdus(1, message23_network_pdus, message23_lower_transport_pdus, message23_upper_transport_pdu);
}
TEST(MessageTest, Message23Send){
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;
    uint8_t  ttl          = 3;
    uint16_t src          = 0x1234;
    uint32_t seq          = 0x07080c;
    uint8_t  szmic        = 0;

    load_network_key_nid_68();
    mesh_set_iv_index(0x12345677);
    mesh_sequence_number_set(seq);
    uint8_t label_uuid[16];
    btstack_parse_hex(message23_label_string, 16, label_uuid);
    mesh_virtual_address_t * virtual_address = mesh_virtual_address_register(label_uuid, 0x9736);
    uint16_t pseudo_dst = virtual_address->pseudo_dst;
    test_send_access_message(netkey_index, appkey_index, ttl, src, pseudo_dst, szmic, message23_upper_transport_pdu, 1, message23_lower_transport_pdus, message23_network_pdus);
}
#endif

// Message 24
char * message24_network_pdus[] = {
    (char *) "e8624e65bb8c1794e998b4081f47a35251fdd3896d99e4db489b918599",
    (char *) "e8a7d0f0a2ea42dc2f4dd6fb4db33a6c088d023b47",
};
char * message24_lower_transport_pdus[] = {
    (char *) "e6a03401c3c51d8e476b28e3aa5001f3",
    (char *) "e6a034211c01cea6",
};
char * message24_upper_transport_pdu = (char *) "ea0a00576f726c64";
char * message24_label_string = (char *) "f4a002c7fb1e4ca0a469a021de0db875";
TEST(MessageTest, Message24Receive){
    load_network_key_nid_68();
    mesh_set_iv_index(0x12345677);
    uint8_t label_uuid[16];
    btstack_parse_hex(message24_label_string, 16, label_uuid);
    mesh_virtual_address_register(label_uuid, 0x9736);
    test_receive_network_pdus(2, message24_network_pdus, message24_lower_transport_pdus, message24_upper_transport_pdu);
}
TEST(MessageTest, Message24Send){
    uint16_t netkey_index = 0;
    uint16_t appkey_index = 0;
    uint8_t  ttl          = 3;
    uint16_t src          = 0x1234;
    uint32_t seq          = 0x07080d;
    uint8_t  szmic        = 1;

    load_network_key_nid_68();
    mesh_set_iv_index(0x12345677);
    mesh_sequence_number_set(seq);
    uint8_t label_uuid[16];
    btstack_parse_hex(message24_label_string, 16, label_uuid);
    mesh_virtual_address_t * virtual_address = mesh_virtual_address_register(label_uuid, 0x9736);
    uint16_t pseudo_dst = virtual_address->pseudo_dst;
    test_send_access_message(netkey_index, appkey_index, ttl, src, pseudo_dst, szmic, message24_upper_transport_pdu, 2, message24_lower_transport_pdus, message24_network_pdus);
}

// Proxy Configuration Test
char * proxy_config_pdus[] = {
    (char *) "0210386bd60efbbb8b8c28512e792d3711f4b526",
};
char * proxy_config_lower_transport_pdus[] = {
    (char *) "0000",
};
char * proxy_config_upper_transport_pdu = (char *) "ea0a00576f726c64";
TEST(MessageTest, ProxyConfigReceive){
    mesh_set_iv_index(0x12345678);
    load_network_key_nid_10();
    int i = 0;
    char ** network_pdus = proxy_config_pdus;
    test_network_pdu_len = strlen(network_pdus[i]) / 2;
    btstack_parse_hex(network_pdus[i], test_network_pdu_len, test_network_pdu_data);
    mesh_network_process_proxy_configuration_message(&test_network_pdu_data[1], test_network_pdu_len-1);
    while (received_proxy_pdu == NULL) {
        mock_process_hci_cmd();
    }
    char ** lower_transport_pdus = proxy_config_lower_transport_pdus;
    transport_pdu_len = strlen(lower_transport_pdus[i]) / 2;
    btstack_parse_hex(lower_transport_pdus[i], transport_pdu_len, transport_pdu_data);

    uint8_t * lower_transport_pdu     = mesh_network_pdu_data(received_proxy_pdu);
    uint8_t   lower_transport_pdu_len = mesh_network_pdu_len(received_proxy_pdu);

    // printf_hexdump(lower_transport_pdu, lower_transport_pdu_len);

    CHECK_EQUAL( transport_pdu_len, lower_transport_pdu_len);
    CHECK_EQUAL_ARRAY(transport_pdu_data, lower_transport_pdu, transport_pdu_len);

    // done
    mesh_network_message_processed_by_higher_layer(received_proxy_pdu);
    received_proxy_pdu = NULL;
}


TEST(MessageTest, ProxyConfigSend){
    uint16_t netkey_index = 0;
    uint8_t  ctl          = 1;
    uint8_t  ttl          = 0;
    uint16_t src          = 1;
    uint16_t dest         = 0;
    uint32_t seq          = 1;
    uint8_t  nid          = 0x10;
    mesh_set_iv_index(0x12345678);
    load_network_key_nid_10();
    mesh_network_pdu_t * network_pdu = mesh_network_pdu_get();
    uint8_t data[] = { 0 , 0 };
    mesh_network_setup_pdu(network_pdu, netkey_index, nid, ctl, ttl, seq, src, dest, data, sizeof(data));
    mesh_network_encrypt_proxy_configuration_message(network_pdu);
    while (received_proxy_pdu == NULL) {
        mock_process_hci_cmd();
    }
    uint8_t * proxy_pdu_data  = received_proxy_pdu->data;
    uint8_t   proxy_pdu_len   = received_proxy_pdu->len;

    int i = 0;
    char ** network_pdus = proxy_config_pdus;
    transport_pdu_len = strlen(network_pdus[i]) / 2;
    btstack_parse_hex(network_pdus[i], transport_pdu_len, transport_pdu_data);

    CHECK_EQUAL( transport_pdu_len-1, proxy_pdu_len);
    CHECK_EQUAL_ARRAY(transport_pdu_data+1, proxy_pdu_data, transport_pdu_len-1);

    received_proxy_pdu = NULL;

    mesh_network_pdu_free(network_pdu);
}

static btstack_crypto_aes128_t crypto_request_aes128;
static uint8_t plaintext[16];
static uint8_t identity_key[16];
static uint8_t hash[16];
static uint8_t random_value[8];

static void mesh_proxy_handle_get_aes128(void * arg){
    UNUSED(arg);
    uint8_t expected_hash[8];
    uint8_t expected_random_value[8];

    btstack_parse_hex("00861765aefcc57b", 8, expected_hash);
    CHECK_EQUAL_ARRAY(&hash[8], expected_hash, 8);
    
    btstack_parse_hex("34ae608fbbc1f2c6", 8, expected_random_value);
    CHECK_EQUAL_ARRAY(random_value, expected_random_value, 8);
}
 
TEST(MessageTest, ServiceDataUsingNodeIdentityTest){
    btstack_parse_hex("34ae608fbbc1f2c6", 8, random_value);
    memset(plaintext, 0, sizeof(plaintext));
    memcpy(&plaintext[6] , random_value, 8);
    big_endian_store_16(plaintext, 14, 0x1201);
    // 84396c435ac48560b5965385253e210c
    btstack_parse_hex("84396c435ac48560b5965385253e210c", 16, identity_key);
    btstack_crypto_aes128_encrypt(&crypto_request_aes128, identity_key, plaintext, hash, mesh_proxy_handle_get_aes128, NULL);
}

// Mesh v1.0, 8.2.1 
static btstack_crypto_aes128_cmac_t aes_cmac_request;
static uint8_t k4_result[1];
static void handle_k4_result(void *arg){
    UNUSED(arg);
    printf("ApplicationkeyIDTest: %02x\n", k4_result[0]);
    CHECK_EQUAL( 0x26, k4_result[0]);
}
TEST(MessageTest, ApplicationkeyIDTest){
    static uint8_t application_key[16];
    btstack_parse_hex("63964771734fbd76e3b40519d1d94a48", 16, application_key);
    mesh_k4(&aes_cmac_request, application_key, &k4_result[0], &handle_k4_result, NULL);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
