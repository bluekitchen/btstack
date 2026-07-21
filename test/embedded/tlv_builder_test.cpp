#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_ltv_builder.h"
#include "btstack_tlv_builder.h"

TEST_GROUP(TLVBuilder){};

TEST(TLVBuilder, ltv_add_bytes_uses_reserved_space){
    uint8_t buffer[5] = { 0, 0, 0, 0, 0x55 };
    const uint8_t value[] = { 0x10, 0x20 };
    btstack_ltv_builder_context_t context;

    btstack_ltv_builder_init(&context, buffer, sizeof(buffer));
    btstack_ltv_builder_add_tag(&context, 0xaa);
    btstack_ltv_builder_add_bytes(&context, value, sizeof(value));

    const uint8_t expected[] = { 3, 0xaa, 0x10, 0x20, 0x55 };
    CHECK_EQUAL(4, btstack_ltv_builder_get_length(&context));
    MEMCMP_EQUAL(expected, buffer, sizeof(buffer));
}

TEST(TLVBuilder, tlv_add_bytes_uses_reserved_space){
    uint8_t buffer[5] = { 0, 0, 0, 0, 0x55 };
    const uint8_t value[] = { 0x10, 0x20 };
    btstack_tlv_builder_context_t context;

    btstack_tlv_builder_init(&context, buffer, sizeof(buffer));
    btstack_tlv_builder_add_tag(&context, 0xaa);
    btstack_tlv_builder_add_bytes(&context, value, sizeof(value));

    const uint8_t expected[] = { 0xaa, 2, 0x10, 0x20, 0x55 };
    CHECK_EQUAL(4, btstack_tlv_builder_get_length(&context));
    MEMCMP_EQUAL(expected, buffer, sizeof(buffer));
}

int main(int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
