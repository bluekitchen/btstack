#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "btstack_chunk_buffer.h"

static uint8_t input[0x10000];
static uint8_t output[sizeof(input)];

extern "C" uint32_t btstack_min(uint32_t a, uint32_t b){
    return a < b ? a : b;
}

TEST_GROUP(ChunkBuffer){
    btstack_chunk_buffer_t context;

    void setup(void){
        memset(input, 0x5a, sizeof(input));
        memset(output, 0, sizeof(output));
        btstack_chunk_buffer_init(&context, input, sizeof(input));
    }
};

TEST(ChunkBuffer, Read65536Bytes){
    uint32_t bytes_read = btstack_chunk_buffer_read(&context, output, sizeof(output));

    CHECK_EQUAL(sizeof(input), bytes_read);
    CHECK_EQUAL(0, btstack_chunk_buffer_bytes_available(&context));
    MEMCMP_EQUAL(input, output, sizeof(input));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
