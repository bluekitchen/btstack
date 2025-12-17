
#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_config.h"

#include "btstack_util.h"
#include "btstack_debug.h"
#include <stdio.h>

static bool assert_failed;

void test_assert(bool condition){
    if (condition != true){
        assert_failed = true;
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

TEST_GROUP(btstack_util){
    char buffer[8];
    void setup(void){
        assert_failed = false;
    }
};

TEST(btstack_util, some_functions) {
    uint8_t buf[4] = {0};
    little_endian_store_08(buf, 0, 42);
    uint8_t value = little_endian_read_08(buf, 0);
    CHECK_EQUAL( 42, value );
    buf[0] = 0;
    big_endian_store_08(buf, 0, 42);
    value = big_endian_read_08(buf, 0);
    CHECK_EQUAL( 42, value );
    const char *str = "Test, 123, Test, 123, Test, 123, Test, 123, Test, 123, "
        "Test, 123, Test, 123, Test, 123, Test, 123, Test, 123, Done";
    int str_len = strlen(str);
    log_debug_hexdump(str, str_len);

    uint8_t out[4];
    btstack_bytes_to_hex((char*)buf, (uint8_t*)"\xde\xad\xbe\xef", 4);
    btstack_hex_to_bytes(out, 4, (char*)buf);
    MEMCMP_EQUAL("\xde\xad\xbe\xef", out, 4);
    btstack_hex_to_bytes(out, 4, (char*)"\xff\xff\xff\xff");

    CHECK_EQUAL(btstack_clz(0x80000000), 0);
}

TEST(btstack_util, btstack_printf_strlen){
    CHECK_EQUAL(4, btstack_printf_strlen("%04x", 0x1234));
}

TEST(btstack_util, btstack_snprintf_assert_complete_ok){
    uint16_t len = btstack_snprintf_assert_complete(buffer, sizeof(buffer), "%04x", 0x1234);
    CHECK_EQUAL(4, len);
    CHECK_EQUAL_ARRAY((uint8_t*) "1234", (uint8_t*) buffer, 4);
}

TEST(btstack_util, btstack_snprintf_assert_complete_too_long){
    (void) btstack_snprintf_assert_complete(buffer, sizeof(buffer), "%08x", 0x1234);
    CHECK_TRUE(assert_failed);
}

TEST(btstack_util, btstack_snprintf_best_effort_complete){
    uint16_t len = btstack_snprintf_best_effort(buffer, sizeof(buffer), "%04x", 0x1234);
    CHECK_EQUAL(4, len);
    CHECK_EQUAL_ARRAY((uint8_t*) "1234", (uint8_t*) buffer, 4);
}

TEST(btstack_util, btstack_snprintf_best_effort_incomplete){
    uint16_t len = btstack_snprintf_best_effort(buffer, sizeof(buffer), "%08x", 0x1234);
    CHECK_EQUAL(7, len);
    CHECK_EQUAL_ARRAY((uint8_t*) "0000000", (uint8_t*) buffer, 4);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
