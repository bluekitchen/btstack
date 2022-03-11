#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "btstack_ring_buffer.h"
#include "btstack_util.h"

static  uint8_t storage[10];

uint32_t btstack_min(uint32_t a, uint32_t b){
    return a < b ? a : b;
}

TEST_GROUP(RingBuffer){
    btstack_ring_buffer_t ring_buffer;
    int storage_size;

    void setup(void){
        storage_size = sizeof(storage);
        memset(storage, 0, storage_size);
        btstack_ring_buffer_init(&ring_buffer, storage, storage_size);
    }
};

TEST(RingBuffer, EmptyBuffer){
    CHECK_TRUE(btstack_ring_buffer_empty(&ring_buffer));
    CHECK_EQUAL(0, btstack_ring_buffer_bytes_available(&ring_buffer));
    CHECK_EQUAL(storage_size, btstack_ring_buffer_bytes_free(&ring_buffer));
}

TEST(RingBuffer, WriteBuffer){
    uint8_t test_write_data[] = {1,2,3,4, 5};
    int test_data_size = sizeof(test_write_data);
    uint8_t test_read_data[test_data_size];

    btstack_ring_buffer_write(&ring_buffer, test_write_data, test_data_size);
    CHECK_EQUAL(test_data_size, btstack_ring_buffer_bytes_available(&ring_buffer));

    uint32_t number_of_bytes_read = 0;
    memset(test_read_data, 0, test_data_size);
    btstack_ring_buffer_read(&ring_buffer, test_read_data, test_data_size, &number_of_bytes_read); 
    
    CHECK_EQUAL(0, memcmp(test_write_data, test_read_data, test_data_size));

}

TEST(RingBuffer, WriteFullBuffer){    
    uint8_t test_write_data[] = {1,2,3,4,5,6,7,8,9,10};
    int test_data_size = sizeof(test_write_data);
    uint8_t test_read_data[test_data_size];

    btstack_ring_buffer_write(&ring_buffer, test_write_data, test_data_size);
    CHECK_EQUAL(test_data_size, btstack_ring_buffer_bytes_available(&ring_buffer));

    memset(test_read_data, 0, test_data_size);
    uint32_t number_of_bytes_read = 0;
    btstack_ring_buffer_read(&ring_buffer, test_read_data, test_data_size, &number_of_bytes_read); 

    CHECK_EQUAL(0, memcmp(test_write_data, test_read_data, test_data_size));
}


TEST(RingBuffer, ReadWrite){    
    uint8_t test_write_data[] = {1,2,3,4};
    int test_data_size = sizeof(test_write_data);
    uint8_t test_read_data[test_data_size];

    int i;
    for (i=0;i<30;i++){
        btstack_ring_buffer_write(&ring_buffer, test_write_data, test_data_size);
        CHECK_EQUAL(test_data_size, btstack_ring_buffer_bytes_available(&ring_buffer));

        memset(test_read_data, 0, test_data_size);
        uint32_t number_of_bytes_read = 0;
        btstack_ring_buffer_read(&ring_buffer, test_read_data, test_data_size, &number_of_bytes_read); 
        CHECK_EQUAL(0, memcmp(test_write_data, test_read_data, test_data_size));
    }
}

TEST(RingBuffer, ReadWriteChunks){    
    uint8_t test_write_data[] = {1,2,3,4,5,6};
    int test_data_size = sizeof(test_write_data);
    int chunk_size = 3;
    uint8_t test_read_data[chunk_size];

    btstack_ring_buffer_write(&ring_buffer, test_write_data, test_data_size);
    CHECK_EQUAL(test_data_size, btstack_ring_buffer_bytes_available(&ring_buffer));

    while (test_data_size){
        memset(test_read_data, 0, chunk_size);
        uint32_t number_of_bytes_read = 0;
        btstack_ring_buffer_read(&ring_buffer, test_read_data, chunk_size, &number_of_bytes_read); 
        test_data_size -= chunk_size;
        CHECK_EQUAL(test_data_size, btstack_ring_buffer_bytes_available(&ring_buffer));
    }
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}