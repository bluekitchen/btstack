#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "btstack_util.h"
#include "btstack_base64_decoder.h"

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

TEST_GROUP(Base64Decoder){
    btstack_base64_decoder_t context;
    void setup(void){
        btstack_base64_decoder_init(&context);
    }
};

TEST(Base64Decoder, InvalidChar){
    int result = btstack_base64_decoder_process_byte(&context, (uint8_t) '@'); 
    CHECK_EQUAL(BTSTACK_BASE64_DECODER_INVALID, result);
}

TEST(Base64Decoder, abc){
    const uint8_t input[] = "YWJj";
    uint8_t output[3];
    int result = btstack_base64_decoder_process_block(input, strlen((const char*) input), output, sizeof(output)); 
    CHECK_EQUAL(sizeof(output), result);
    CHECK_EQUAL_ARRAY((uint8_t *) "abc", output, 3);
}

TEST(Base64Decoder, ab){
    const uint8_t input[] = "YWI=";
    uint8_t output[3];
    int result = btstack_base64_decoder_process_block(input, strlen((const char*) input), output, sizeof(output)); 
    CHECK_EQUAL(2, result);
    CHECK_EQUAL_ARRAY((uint8_t *) "ab", output, 2);
}

TEST(Base64Decoder, a){
    const uint8_t input[] = "YQ==";
    uint8_t output[3];
    int result = btstack_base64_decoder_process_block(input, strlen((const char*) input), output, sizeof(output)); 
    CHECK_EQUAL(1, result);
    CHECK_EQUAL_ARRAY((uint8_t *) "a", output, 1);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
