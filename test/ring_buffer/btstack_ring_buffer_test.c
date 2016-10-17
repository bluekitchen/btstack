#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"
#include "btstack_ring_buffer.h"

static  uint8_t storage[10];

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

    uint16_t number_of_bytes_read = 0;
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
    uint16_t number_of_bytes_read = 0;
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
        uint16_t number_of_bytes_read = 0;
        btstack_ring_buffer_read(&ring_buffer, test_read_data, test_data_size, &number_of_bytes_read); 
        CHECK_EQUAL(0, memcmp(test_write_data, test_read_data, test_data_size));
    }
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}