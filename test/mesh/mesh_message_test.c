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

void adv_bearer_register_for_mesh_message(btstack_packet_handler_t packet_handler){}
void adv_bearer_request_can_send_now_for_mesh_message(void){}
void adv_bearer_send_mesh_message(const uint8_t * network_pdu, uint16_t size){}

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

static void load_provisioning_data_test_message(void){
    mesh_provisioning_data_t provisioning_data;
    provisioning_data.nid = 0x68;
    mesh_set_iv_index(0x12345678);
    btstack_parse_hex("0953fa93e7caac9638f58820220a398e", 16, provisioning_data.encryption_key);
    btstack_parse_hex("8b84eedec100067d670971dd2aa700cf", 16, provisioning_data.privacy_key);
    mesh_network_key_list_add_from_provisioning_data(&provisioning_data);
#if 0
    uint8_t application_key[16];
    btstack_parse_hex("63964771734fbd76e3b40519d1d94a48", 16, application_key);
    mesh_application_key_set( 0, 0x26, application_key);

    uint8_t device_key[16];
    btstack_parse_hex("9d6dd0e96eb25dc19a40ed9914f8f03f", 16, device_key);
    mesh_transport_set_device_key(device_key);
#endif
}

static mesh_network_pdu_t * received_network_pdu;

#if 0
static void mesh_lower_transport_received_mesage(mesh_network_callback_type_t callback_type, mesh_network_pdu_t * network_pdu){
    printf("mesh_lower_transport_received_mesage, type %u\n", callback_type);
    switch (callback_type){
        case MESH_NETWORK_PDU_RECEIVED:
            received_network_pdu = network_pdu; 
            break;
        case MESH_NETWORK_PDU_SENT:
            break;
        default:
            break;
    }
}
#endif

TEST_GROUP(MessageTest){
    void setup(void){
        btstack_memory_init();
        btstack_crypto_init();
        load_provisioning_data_test_message();
        mesh_network_init();
        mesh_network_set_higher_layer_handler(&mesh_lower_transport_received_mesage);
        received_network_pdu = NULL;
    }
};

static uint8_t transport_pdu_data[32];
static uint16_t transport_pdu_len;
static     uint8_t test_network_pdu_len;
static uint8_t test_network_pdu_data[29];
static  const char * test_network_pdu_string;

TEST(MessageTest, Test1){
    uint8_t test_network_pdu_data[29];
    test_network_pdu_string = "68eca487516765b5e5bfdacbaf6cb7fb6bff871f035444ce83a670df";
    test_network_pdu_len = strlen(test_network_pdu_string) / 2;
    btstack_parse_hex(test_network_pdu_string, test_network_pdu_len, test_network_pdu_data);
    mesh_network_received_message(test_network_pdu_data, test_network_pdu_len);
    //
    while (received_network_pdu == NULL) {
        mock_process_hci_cmd();
    }
    // TODO: check
}

TEST(MessageTest, Test2){
}

TEST(MessageTest, Test3){
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
