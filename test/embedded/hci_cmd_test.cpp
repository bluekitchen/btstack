#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "hci_cmd.h"
#include "btstack_util.h"

static uint8_t hci_cmd_buffer[350];
static uint8_t input_buffer[350];

static uint16_t create_hci_cmd(const hci_cmd_t *cmd, ...){
    va_list argptr;
    va_start(argptr, cmd);
    uint16_t len = hci_cmd_create_from_template(hci_cmd_buffer, cmd, argptr);
    va_end(argptr);
    return len - 3;
}

const hci_cmd_t cmd_1 = {0x1234, "1"};
const hci_cmd_t cmd_2 = {0x1234, "2"};
const hci_cmd_t cmd_3 = {0x1234, "3"};
const hci_cmd_t cmd_4 = {0x1234, "4"};
const hci_cmd_t cmd_A = {0x1234, "A"};
const hci_cmd_t cmd_B = {0x1234, "B"};
const hci_cmd_t cmd_D = {0x1234, "D"};
const hci_cmd_t cmd_E = {0x1234, "E"};
const hci_cmd_t cmd_F = {0x1234, "F"};
const hci_cmd_t cmd_H = {0x1234, "H"};
const hci_cmd_t cmd_K = {0x1234, "K"};
const hci_cmd_t cmd_M = {0x1234, "M"};
const hci_cmd_t cmd_N = {0x1234, "N"};
const hci_cmd_t cmd_P = {0x1234, "P"};
const hci_cmd_t cmd_Q = {0x1234, "Q"};
const hci_cmd_t cmd_JV = {0x1234, "JV"};
const hci_cmd_t cmd_number_12 = {0x1234, "a[12]"};
const hci_cmd_t cmd_bits_12 = {0x1234, "b[12]"};
const hci_cmd_t cmd_INVALID = {0x1234, "!"};

TEST_GROUP(HCI_Command){
    void setup(void){
    }
    void setup_input_buffer(uint16_t size){
        uint16_t i;
        for (i = 0; i < size; i++){
            input_buffer[i] = i + 1;
        }
    }

    void setup_input_buffer_with_alphabet(uint16_t size){
        uint16_t i;
        uint8_t a = 0x41;
        uint8_t z = 0x5A;

        for (i = 0; i < size; i++){
            input_buffer[i] = a + i%(z-a);
        }
    }
    void teardown(void){
    }
};

TEST(HCI_Command, format_1){
    const uint8_t expected_buffer[] = {0x55};
    uint16_t size = create_hci_cmd(&cmd_1, expected_buffer[0]);
    CHECK_EQUAL(sizeof(expected_buffer), size);
    MEMCMP_EQUAL(expected_buffer, &hci_cmd_buffer[3], sizeof(expected_buffer));
}

TEST(HCI_Command, format_2){
    const uint8_t expected_buffer[] = {0x55, 0x66};
    uint16_t size = create_hci_cmd(&cmd_2, 0x6655);
    CHECK_EQUAL(sizeof(expected_buffer), size);
    MEMCMP_EQUAL(expected_buffer, &hci_cmd_buffer[3], sizeof(expected_buffer));
}

TEST(HCI_Command, format_3){
    const uint8_t expected_buffer[] = {0x55, 0x66, 0x77};
    uint16_t size = create_hci_cmd(&cmd_3, 0x776655);
    CHECK_EQUAL(sizeof(expected_buffer), size);
    MEMCMP_EQUAL(expected_buffer, &hci_cmd_buffer[3], sizeof(expected_buffer));
}

TEST(HCI_Command, format_4){
    const uint8_t expected_buffer[] = {0x55, 0x66, 0x77, 0x88};
    uint16_t size = create_hci_cmd(&cmd_4, 0x88776655);
    CHECK_EQUAL(sizeof(expected_buffer), size);
    MEMCMP_EQUAL(expected_buffer, &hci_cmd_buffer[3], sizeof(expected_buffer));
}

TEST(HCI_Command, format_A){
    uint16_t expected_size = 31;
    setup_input_buffer(expected_size);

    uint16_t size = create_hci_cmd(&cmd_A, input_buffer);
    CHECK_EQUAL(expected_size, size);
    MEMCMP_EQUAL(input_buffer, &hci_cmd_buffer[3], expected_size);
}

TEST(HCI_Command, format_B){
    uint8_t expected_buffer[6];
    uint16_t expected_size = 6;
    setup_input_buffer(expected_size);
    reverse_bytes(input_buffer, expected_buffer, expected_size);

    uint16_t size = create_hci_cmd(&cmd_B, input_buffer);
    CHECK_EQUAL(expected_size, size);
    MEMCMP_EQUAL(expected_buffer, &hci_cmd_buffer[3], expected_size);
}

TEST(HCI_Command, format_D){
    uint16_t expected_size = 8;
    setup_input_buffer(expected_size);

    uint16_t size = create_hci_cmd(&cmd_D, input_buffer);
    CHECK_EQUAL(expected_size, size);
    MEMCMP_EQUAL(input_buffer, &hci_cmd_buffer[3], expected_size);
}

TEST(HCI_Command, format_E){
    uint16_t expected_size = 240;
    setup_input_buffer(expected_size);

    uint16_t size = create_hci_cmd(&cmd_E, input_buffer);
    CHECK_EQUAL(expected_size, size);
    MEMCMP_EQUAL(input_buffer, &hci_cmd_buffer[3], expected_size);
}

TEST(HCI_Command, format_F){
    uint16_t expected_size = sizeof(le_cs_fae_table_t);
    setup_input_buffer(expected_size);

    uint16_t size = create_hci_cmd(&cmd_F, input_buffer);
    CHECK_EQUAL(expected_size, size);
    MEMCMP_EQUAL(input_buffer, &hci_cmd_buffer[3], expected_size);
}

TEST(HCI_Command, format_H){
    const uint8_t expected_buffer[] = {0x55, 0x66};
    uint16_t size = create_hci_cmd(&cmd_H, 0x6655);
    CHECK_EQUAL(sizeof(expected_buffer), size);
    MEMCMP_EQUAL(expected_buffer, &hci_cmd_buffer[3], sizeof(expected_buffer));
}

TEST(HCI_Command, format_K){
    uint8_t expected_buffer[16];
    uint16_t expected_size = 16;
    setup_input_buffer(expected_size);
    reverse_bytes(input_buffer, expected_buffer, expected_size);

    uint16_t size = create_hci_cmd(&cmd_K, input_buffer);
    CHECK_EQUAL(expected_size, size);
    MEMCMP_EQUAL(expected_buffer, &hci_cmd_buffer[3], expected_size);
}

TEST(HCI_Command, format_M){
    uint16_t expected_size = sizeof(le_cs_channel_map_t);
    setup_input_buffer(expected_size);

    uint16_t size = create_hci_cmd(&cmd_M, input_buffer);
    CHECK_EQUAL(expected_size, size);
    MEMCMP_EQUAL(input_buffer, &hci_cmd_buffer[3], expected_size);
}

TEST(HCI_Command, format_N_less_then_248){
    uint16_t expected_size = 248;
    // last (248 - 160) bytes are set to 0
    memset(input_buffer, 0u, 248u);
    setup_input_buffer_with_alphabet(160);

    uint16_t size = create_hci_cmd(&cmd_N, input_buffer);
    CHECK_EQUAL(expected_size, size);
    MEMCMP_EQUAL(input_buffer, &hci_cmd_buffer[3], expected_size);
}

TEST(HCI_Command, format_N_more_then_248){
    uint16_t expected_size = 248;
    // last (248 - 160) bytes are set to 0
    setup_input_buffer_with_alphabet(300);

    uint16_t size = create_hci_cmd(&cmd_N, input_buffer);
    CHECK_EQUAL(expected_size, size);
    MEMCMP_EQUAL(input_buffer, &hci_cmd_buffer[3], expected_size);
}

TEST(HCI_Command, format_P){
    uint16_t expected_size = 16;
    setup_input_buffer(expected_size);

    uint16_t size = create_hci_cmd(&cmd_P, input_buffer);
    CHECK_EQUAL(expected_size, size);
    MEMCMP_EQUAL(input_buffer, &hci_cmd_buffer[3], expected_size);
}

TEST(HCI_Command, format_Q){
    uint8_t expected_buffer[32];
    uint16_t expected_size = 32;
    setup_input_buffer(expected_size);
    reverse_bytes(input_buffer, expected_buffer, expected_size);

    uint16_t size = create_hci_cmd(&cmd_Q, input_buffer);
    CHECK_EQUAL(expected_size, size);
    MEMCMP_EQUAL(expected_buffer, &hci_cmd_buffer[3], expected_size);
}

TEST(HCI_Command, format_JV){
    uint8_t expected_buffer[4];
    uint16_t expected_size = 4;
    uint8_t var_len_arg = expected_size - 1;
    setup_input_buffer(var_len_arg);
    expected_buffer[0] = var_len_arg;
    memcpy(&expected_buffer[1], input_buffer, var_len_arg);

    create_hci_cmd(&cmd_JV, 0, NULL);
    uint16_t size = create_hci_cmd(&cmd_JV, var_len_arg, input_buffer);
    CHECK_EQUAL(expected_size, size);
    MEMCMP_EQUAL(expected_buffer, &hci_cmd_buffer[3], expected_size);
}

TEST(HCI_Command, format_number_12){
    uint8_t num_elements = 2;
    uint8_t expected_buffer[7];
    uint16_t expected_size = 7;
    uint8_t  input_a[2] = { 1, 2};
    uint16_t input_b[2] = { 3, 4};
    expected_buffer[0] = num_elements;
    expected_buffer[1] = 1;
    little_endian_store_16(expected_buffer, 2, 3);
    expected_buffer[4] = 2;
    little_endian_store_16(expected_buffer, 5, 4);
    uint16_t size = create_hci_cmd(&cmd_number_12, num_elements, input_a, input_b);
    CHECK_EQUAL(expected_size, size);
    MEMCMP_EQUAL(expected_buffer, &hci_cmd_buffer[3], expected_size);
}

TEST(HCI_Command, format_bits_12){
    uint8_t bits_elements = 3;
    uint8_t expected_buffer[7];
    uint16_t expected_size = 7;
    uint8_t  input_a[2] = { 1, 2};
    uint16_t input_b[2] = { 3, 4};
    expected_buffer[0] = bits_elements;
    expected_buffer[1] = 1;
    little_endian_store_16(expected_buffer, 2, 3);
    expected_buffer[4] = 2;
    little_endian_store_16(expected_buffer, 5, 4);
    uint16_t size = create_hci_cmd(&cmd_bits_12, bits_elements, input_a, input_b);
    CHECK_EQUAL(expected_size, size);
    MEMCMP_EQUAL(expected_buffer, &hci_cmd_buffer[3], expected_size);
}

TEST(HCI_Command, format_INVALID){
    uint16_t expected_size = 0;
    setup_input_buffer(16);

    uint16_t size = create_hci_cmd(&cmd_INVALID, input_buffer);
    CHECK_EQUAL(expected_size, size);
}

TEST(HCI_Command, hci_le_cs_write_cached_remote_fae_table_uses_F){
    const uint16_t connection_handle = 0x1234;
    le_cs_fae_table_t remote_fae_table = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
        0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47
    };
    uint8_t expected_buffer[2 + sizeof(remote_fae_table)];
    little_endian_store_16(expected_buffer, 0, connection_handle);
    memcpy(&expected_buffer[2], remote_fae_table, sizeof(remote_fae_table));

    uint16_t size = create_hci_cmd(&hci_le_cs_write_cached_remote_fae_table, connection_handle, remote_fae_table);
    CHECK_EQUAL(sizeof(expected_buffer), size);
    MEMCMP_EQUAL(expected_buffer, &hci_cmd_buffer[3], sizeof(expected_buffer));
}

TEST(HCI_Command, hci_le_cs_create_config_uses_M){
    const uint16_t connection_handle = 0x1234;
    le_cs_channel_map_t channel_map = {
        0x11, 0x22, 0x33, 0x44, 0x55,
        0x66, 0x77, 0x88, 0x99, 0xaa
    };
    const uint8_t expected_buffer[] = {
        0x34, 0x12,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa,
        0xbb, 0xcc, 0xdd, 0xee, 0xff
    };

    uint16_t size = create_hci_cmd(&hci_le_cs_create_config,
        connection_handle,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
        channel_map,
        0xbb, 0xcc, 0xdd, 0xee, 0xff);
    CHECK_EQUAL(sizeof(expected_buffer), size);
    MEMCMP_EQUAL(expected_buffer, &hci_cmd_buffer[3], sizeof(expected_buffer));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
