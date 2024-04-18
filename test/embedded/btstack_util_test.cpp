#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_debug.h"
#include "btstack_util.h"

TEST_GROUP(BTstackUtil){
    void setup(void){
    }
    void teardown(void){
    }
};

TEST(BTstackUtil, btstack_next_cid_ignoring_zero){
    CHECK_EQUAL(1, btstack_next_cid_ignoring_zero(0));
    CHECK_EQUAL(5, btstack_next_cid_ignoring_zero(4));
    CHECK_EQUAL(1, btstack_next_cid_ignoring_zero(0xFFFF));
}

TEST(BTstackUtil, bd_addr_cmp){
    bd_addr_t a = {0};
    bd_addr_t b = {0};
    CHECK_EQUAL(0, bd_addr_cmp(a, b));
    
    bd_addr_t a1 = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    bd_addr_t b1 = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    CHECK_EQUAL(0, bd_addr_cmp(a1, b1));
}

TEST(BTstackUtil, bd_addr_copy){
    bd_addr_t a = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    bd_addr_t b = {0};
    CHECK(0 != bd_addr_cmp(a, b));

    bd_addr_copy(a,b);
    CHECK_EQUAL(0, bd_addr_cmp(a, b));
}

TEST(BTstackUtil, little_endian_read){
    const uint8_t buffer[] = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    uint32_t value;

    value = little_endian_read_16(buffer, 0);
    CHECK_EQUAL(0xECBC, value);

    value = little_endian_read_24(buffer, 0);
    CHECK_EQUAL(0x5DECBC, value);
    
    value = little_endian_read_32(buffer, 0);
    CHECK_EQUAL(0xE65DECBC, value);
}

TEST(BTstackUtil, little_endian_store){
    uint8_t buffer[6];
    uint32_t expected_value = 0xE65DECBC;
    uint32_t value;

    memset(buffer, 0, sizeof(buffer));
    little_endian_store_16(buffer, 0, expected_value);
    value = little_endian_read_16(buffer, 0);
    CHECK_EQUAL(0xECBC, value);

    memset(buffer, 0, sizeof(buffer));
    little_endian_store_24(buffer, 0, expected_value);
    value = little_endian_read_24(buffer, 0);
    CHECK_EQUAL(0x5DECBC, value);

    memset(buffer, 0, sizeof(buffer));
    little_endian_store_32(buffer, 0, expected_value);
    value = little_endian_read_32(buffer, 0);
    CHECK_EQUAL(0xE65DECBC, value);
}

TEST(BTstackUtil, big_endian_read){
    const uint8_t buffer[] = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    uint32_t value;

    value = big_endian_read_16(buffer, 0);
    CHECK_EQUAL(0xBCEC, value);

    value = big_endian_read_24(buffer, 0);
    CHECK_EQUAL(0xBCEC5D, value);
    
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0xBCEC5DE6, value);
}

TEST(BTstackUtil, big_endian_store){
    uint8_t buffer[6];
    uint32_t expected_value = 0xE65DECBC;
    uint32_t value;

    memset(buffer, 0, sizeof(buffer));
    big_endian_store_16(buffer, 0, expected_value);
    value = big_endian_read_16(buffer, 0);
    CHECK_EQUAL(0xECBC, value);

    memset(buffer, 0, sizeof(buffer));
    big_endian_store_24(buffer, 0, expected_value);
    value = big_endian_read_24(buffer, 0);
    CHECK_EQUAL(0x5DECBC, value);

    memset(buffer, 0, sizeof(buffer));
    big_endian_store_32(buffer, 0, expected_value);
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0xE65DECBC, value);
}

TEST(BTstackUtil, reverse_bytes){
    uint8_t src[32];
    uint8_t buffer[32];
    uint32_t value;

    int i;
    for (i = 0; i < sizeof(src); i++){
        src[i] = i + 1;
    }
    
    memset(buffer, 0, sizeof(buffer));
    reverse_bytes(src, buffer, sizeof(buffer));
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0x201F1E1D, value);

    memset(buffer, 0, sizeof(buffer));
    reverse_24(src, buffer);
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0x03020100, value);

    memset(buffer, 0, sizeof(buffer));
    reverse_48(src, buffer);
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0x06050403, value);

    memset(buffer, 0, sizeof(buffer));
    reverse_56(src, buffer);
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0x07060504, value);

    memset(buffer, 0, sizeof(buffer));
    reverse_64(src, buffer);
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0x08070605, value);

    memset(buffer, 0, sizeof(buffer));
    reverse_128(src, buffer);
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0x100F0E0D, value);

    memset(buffer, 0, sizeof(buffer));
    reverse_256(src, buffer);
    value = big_endian_read_32(buffer, 0);
    CHECK_EQUAL(0x201F1E1D, value);
}

TEST(BTstackUtil, reverse_bd_addr){
    bd_addr_t src = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    bd_addr_t dest = {0};
    reverse_bd_addr(src, dest);

    uint32_t value = big_endian_read_32(dest, 0);
    CHECK_EQUAL(0x0315E65D, value);
}

TEST(BTstackUtil, btstack_min_max){
    uint32_t a = 30;
    uint32_t b = 100;

    CHECK_EQUAL(a, btstack_min(a,b));
    CHECK_EQUAL(a, btstack_min(b,a));

    CHECK_EQUAL(b, btstack_max(a,b));
    CHECK_EQUAL(b, btstack_max(b,a));
}

TEST(BTstackUtil, char_for_nibble){
    CHECK_EQUAL('A', char_for_nibble(10));
    CHECK_EQUAL('?', char_for_nibble(20));
    // CHECK_EQUAL('?', char_for_nibble(-5));
}

TEST(BTstackUtil, nibble_for_char){
    CHECK_EQUAL(0, nibble_for_char('0'));
    CHECK_EQUAL(5, nibble_for_char('5'));
    CHECK_EQUAL(9, nibble_for_char('9'));

    CHECK_EQUAL(10, nibble_for_char('a'));
    CHECK_EQUAL(12, nibble_for_char('c'));
    CHECK_EQUAL(15, nibble_for_char('f'));
    
    CHECK_EQUAL(10, nibble_for_char('A'));
    CHECK_EQUAL(12, nibble_for_char('C'));
    CHECK_EQUAL(15, nibble_for_char('F'));

    CHECK_EQUAL(-1, nibble_for_char('-'));
}

TEST(BTstackUtil, logging){
    uint8_t data[6 * 16 + 5] = {0x54, 0x65, 0x73, 0x74, 0x20, 0x6c, 0x6f, 0x67, 0x20, 0x69, 0x6e, 0x66, 0x6f};
#ifdef ENABLE_LOG_DEBUG
    log_debug_hexdump(data, sizeof(data));
#endif

#ifdef ENABLE_LOG_INFO
    log_info_hexdump(data, sizeof(data));
    sm_key_t key = {0x54, 0x65, 0x73, 0x74, 0x20, 0x6c, 0x6f, 0x67, 0x20, 0x69, 0x6e, 0x66, 0x6f, 0x01, 0x014, 0xFF};
    log_info_key("test key", key);
#endif
}

TEST(BTstackUtil, uuids){
    uint32_t shortUUID = 0x44445555;
    uint8_t uuid[16];
    uint8_t uuid128[] = {0x44, 0x44, 0x55, 0x55, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB};
    
    memset(uuid, 0, sizeof(uuid));
    uuid_add_bluetooth_prefix(uuid, shortUUID);
    MEMCMP_EQUAL(uuid128, uuid, sizeof(uuid));

    CHECK_EQUAL(1, uuid_has_bluetooth_prefix(uuid128));

    uuid128[5] = 0xff;
    CHECK(1 != uuid_has_bluetooth_prefix(uuid128));

    char * uuid128_str = uuid128_to_str(uuid128);
    STRCMP_EQUAL("44445555-00FF-1000-8000-00805F9B34FB", uuid128_str);
}

TEST(BTstackUtil, bd_addr_utils){
    const bd_addr_t addr = {0xBC, 0xEC, 0x5D, 0xE6, 0x15, 0x03};
    char * device_addr_string = (char *)"BC:EC:5D:E6:15:03";

    char * addr_str = bd_addr_to_str(addr);
    STRCMP_EQUAL(device_addr_string, addr_str);

    uint8_t device_name[50];
    strcpy((char *)device_name, "Device name 00:00:00:00:00:00");
    btstack_replace_bd_addr_placeholder(device_name, strlen((const char*) device_name), addr);
    STRCMP_EQUAL("Device name BC:EC:5D:E6:15:03", (const char *) device_name);

    bd_addr_t device_addr;
    CHECK_EQUAL(1, sscanf_bd_addr(device_addr_string, device_addr));
    MEMCMP_EQUAL(addr, device_addr, sizeof(addr));

    CHECK_EQUAL(1, sscanf_bd_addr("BC EC 5D E6 15 03", device_addr));
    CHECK_EQUAL(1, sscanf_bd_addr("BC-EC-5D-E6-15-03", device_addr));

    CHECK_EQUAL(0, sscanf_bd_addr("", device_addr));
    CHECK_EQUAL(0, sscanf_bd_addr("GG-EC-5D-E6-15-03", device_addr));
    CHECK_EQUAL(0, sscanf_bd_addr("8G-EC-5D-E6-15-03", device_addr));
    CHECK_EQUAL(0, sscanf_bd_addr("BCxECx5DxE6:15:03", device_addr));

}

TEST(BTstackUtil, atoi){
    CHECK_EQUAL(102, btstack_atoi("102"));
    CHECK_EQUAL(0, btstack_atoi("-102"));
    CHECK_EQUAL(1, btstack_atoi("1st"));
    CHECK_EQUAL(1, btstack_atoi("1st2"));
}

TEST(BTstackUtil, string_len_for_uint32){
    CHECK_EQUAL(1, string_len_for_uint32(9)); 
    CHECK_EQUAL(2, string_len_for_uint32(19)); 
    CHECK_EQUAL(3, string_len_for_uint32(109)); 
    CHECK_EQUAL(4, string_len_for_uint32(1009)); 
    CHECK_EQUAL(5, string_len_for_uint32(10009)); 
    CHECK_EQUAL(6, string_len_for_uint32(100009)); 
    CHECK_EQUAL(7, string_len_for_uint32(1000009)); 
    CHECK_EQUAL(8, string_len_for_uint32(10000009)); 
    CHECK_EQUAL(9, string_len_for_uint32(100000009)); 
    CHECK_EQUAL(10, string_len_for_uint32(1000000000)); 
}

TEST(BTstackUtil, count_set_bits_uint32){
    CHECK_EQUAL(4, count_set_bits_uint32(0x0F));
}

TEST(BTstackUtil, crc8){
    uint8_t data[] = {1,2,3,4,5,6,7,8,9};
    CHECK_EQUAL(84, btstack_crc8_calc(data, sizeof(data)));

    CHECK_EQUAL(0, btstack_crc8_check(data, sizeof(data), 84));
    CHECK_EQUAL(1, btstack_crc8_check(data, sizeof(data), 74));
}

TEST(BTstackUtil, strcat){
    char summaries[3][7 * 8 + 1];
    CHECK_EQUAL((7*8+1), sizeof(summaries[0]));
    summaries[0][0] = 0;
    char item_text[10];
    sprintf(item_text, "%04x:%02d ", 1 ,2);
    btstack_strcat(summaries[0], sizeof(summaries[0]), item_text);
    sprintf(item_text, "%04x:%02d ", 3 ,4);
    btstack_strcat(summaries[0], sizeof(summaries[0]), item_text);
    STRCMP_EQUAL("0001:02 0003:04 ", summaries[0]);
}

TEST(BTstackUtil, btstack_is_null){
    CHECK_EQUAL(btstack_is_null(NULL, 0), true);

    const uint8_t addr[] = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
    CHECK_EQUAL(btstack_is_null(addr, sizeof(addr)), false);

    const uint8_t null_addr[] = {0,0,0,0,0,0};
    CHECK_EQUAL(btstack_is_null(null_addr, sizeof(null_addr)), true);

}

TEST(BTstackUtil, btstack_is_null_bd_addr){
    const bd_addr_t addr = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
    CHECK_EQUAL(btstack_is_null_bd_addr(addr), false);

    const bd_addr_t null_addr = {0,0,0,0,0,0};
    CHECK_EQUAL(btstack_is_null_bd_addr(null_addr), true);
}

TEST(BTstackUtil, btstack_time16_delta){
    CHECK_EQUAL(btstack_time16_delta(25, 26), -1);
}

TEST(BTstackUtil, btstack_strcpy){
    static char * field_data = "btstack";
    char buffer[10];
    
    btstack_strcpy(buffer, sizeof(buffer), field_data);
    MEMCMP_EQUAL(buffer, field_data, strlen(field_data));
}

TEST(BTstackUtil, btstack_virtual_memcpy){
    uint16_t bytes_copied;
    const uint8_t field_data[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    uint16_t field_len = sizeof(field_data);
    uint16_t field_offset = 0;

    uint8_t   buffer[100];
    
    btstack_virtual_memcpy(field_data, field_len, 0, buffer, sizeof(buffer), 0);
    MEMCMP_EQUAL(buffer, field_data, field_len);

    btstack_virtual_memcpy(field_data, field_len, 3, buffer, sizeof(buffer), 0);
    MEMCMP_EQUAL(buffer, field_data, field_len - 3);

    // bail before buffer
    bytes_copied = btstack_virtual_memcpy(field_data, field_len, 0, buffer, sizeof(buffer), sizeof(buffer));
    CHECK_EQUAL(bytes_copied, 0);

    // bail after buffer
    bytes_copied = btstack_virtual_memcpy(field_data, field_len, sizeof(field_data), buffer, 0, 0);
    CHECK_EQUAL(bytes_copied, 0);
}

TEST(BTstackUtil, btstack_virtual_memcpy_two){
    uint16_t bytes_copied;
    const uint8_t field_data[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    uint16_t field_len = sizeof(field_data);
    uint16_t field_offset = 0;

    uint8_t   buffer[14];
    uint16_t  buffer_size = sizeof(buffer);
    uint16_t  records_offset = 0;

    bytes_copied = btstack_virtual_memcpy(field_data, field_len, records_offset, buffer, sizeof(buffer), 0);
    records_offset += bytes_copied;
    CHECK_EQUAL(bytes_copied, field_len);

    bytes_copied = btstack_virtual_memcpy(field_data, field_len, records_offset, buffer, sizeof(buffer), 0);
    records_offset += bytes_copied;
    CHECK_EQUAL(bytes_copied, field_len);

    // buffer can store only a fragment of the record, and will skip remaining bytes
    // skip_at_end
    bytes_copied = btstack_virtual_memcpy(field_data, field_len, records_offset, buffer, sizeof(buffer), 0);
    records_offset += bytes_copied;
    CHECK_EQUAL(bytes_copied, 2);
}

TEST(BTstackUtil, crc_updated){
    uint32_t crc_initial = btstack_crc32_init();
    CHECK_EQUAL(crc_initial, 0xffffffff);

    const uint8_t data[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    uint32_t crc_updated = btstack_crc32_update(crc_initial, data, sizeof(data));
    CHECK_EQUAL(crc_updated, 1648583859);

    uint32_t crc_final = btstack_crc32_finalize(crc_updated);
    CHECK_EQUAL(crc_final, crc_updated ^ 0xffffffff);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
