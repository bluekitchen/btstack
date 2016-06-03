#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ble/sm_mbedtls_allocator.h"

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

static uint8_t mbedtls_memory_buffer[5000]; // experimental value on 64-bit system

uint32_t offset_for_buffer(void * buffer){
	return (uint8_t*)buffer - mbedtls_memory_buffer;
}

TEST_GROUP(sm_mbedtls_allocator_test){
};

TEST(sm_mbedtls_allocator_test, init){
	sm_mbedtls_allocator_init(mbedtls_memory_buffer, sizeof(mbedtls_memory_buffer));
	sm_allocator_node_t * anchor = (sm_allocator_node_t*) mbedtls_memory_buffer;
	sm_allocator_node_t * first = (sm_allocator_node_t*) &mbedtls_memory_buffer[8];
	CHECK_EQUAL(8, anchor->next);
	CHECK_EQUAL(0, anchor->size);
	CHECK_EQUAL(0, first->next);
	CHECK_EQUAL(sizeof(mbedtls_memory_buffer) - 8, first->size);
}

TEST(sm_mbedtls_allocator_test, alloc1){
	sm_mbedtls_allocator_init(mbedtls_memory_buffer, sizeof(mbedtls_memory_buffer));
    void * buffer1 = sm_mbedtls_allocator_calloc(1,8);
    CHECK_EQUAL(8 + sizeof(void*), offset_for_buffer(buffer1));
}

TEST(sm_mbedtls_allocator_test, alloc_free){
	sm_mbedtls_allocator_init(mbedtls_memory_buffer, sizeof(mbedtls_memory_buffer));
    void * buffer1 = sm_mbedtls_allocator_calloc(1,8);
    sm_mbedtls_allocator_free(buffer1);
	sm_allocator_node_t * anchor = (sm_allocator_node_t*) mbedtls_memory_buffer;
	sm_allocator_node_t * first = (sm_allocator_node_t*) &mbedtls_memory_buffer[8];
	CHECK_EQUAL(8, anchor->next);
	CHECK_EQUAL(0, anchor->size);
	CHECK_EQUAL(0, first->next);
	CHECK_EQUAL(sizeof(mbedtls_memory_buffer) - 8, first->size);
}

TEST(sm_mbedtls_allocator_test, alloc2){
	sm_mbedtls_allocator_init(mbedtls_memory_buffer, sizeof(mbedtls_memory_buffer));
    void * buffer1 = sm_mbedtls_allocator_calloc(1,8);
    void * buffer2 = sm_mbedtls_allocator_calloc(1,8);
    CHECK_EQUAL(8 + sizeof(void*) + 8 + sizeof(void*), offset_for_buffer(buffer2));
}

TEST(sm_mbedtls_allocator_test, alloc_free_2){
	sm_mbedtls_allocator_init(mbedtls_memory_buffer, sizeof(mbedtls_memory_buffer));
    void * buffer1 = sm_mbedtls_allocator_calloc(1,8);
    void * buffer2 = sm_mbedtls_allocator_calloc(1,8);
    sm_mbedtls_allocator_free(buffer1);
    sm_mbedtls_allocator_free(buffer2);
	sm_allocator_node_t * anchor = (sm_allocator_node_t*) mbedtls_memory_buffer;
	sm_allocator_node_t * first = (sm_allocator_node_t*) &mbedtls_memory_buffer[8];
	CHECK_EQUAL(8, anchor->next);
	CHECK_EQUAL(0, anchor->size);
	CHECK_EQUAL(0, first->next);
	CHECK_EQUAL(sizeof(mbedtls_memory_buffer) - 8, first->size);
}

TEST(sm_mbedtls_allocator_test, alloc_free_3){
	sm_mbedtls_allocator_init(mbedtls_memory_buffer, sizeof(mbedtls_memory_buffer));
    void * buffer1 = sm_mbedtls_allocator_calloc(1,8);
    void * buffer2 = sm_mbedtls_allocator_calloc(1,8);
    sm_mbedtls_allocator_free(buffer2);
    sm_mbedtls_allocator_free(buffer1);
	sm_allocator_node_t * anchor = (sm_allocator_node_t*) mbedtls_memory_buffer;
	CHECK_EQUAL(8, anchor->next);
	CHECK_EQUAL(0, anchor->size);
}

TEST(sm_mbedtls_allocator_test, alloc_alloc_free_alloc_1){
	sm_mbedtls_allocator_init(mbedtls_memory_buffer, sizeof(mbedtls_memory_buffer));
    void * buffer1 = sm_mbedtls_allocator_calloc(1,8);
    void * buffer2 = sm_mbedtls_allocator_calloc(1,8);
    sm_mbedtls_allocator_free(buffer1);
    buffer1 = sm_mbedtls_allocator_calloc(1,8);
    CHECK_EQUAL(8 + sizeof(void*), offset_for_buffer(buffer1));
}

TEST(sm_mbedtls_allocator_test, alloc_alloc_free_alloc_2){
	sm_mbedtls_allocator_init(mbedtls_memory_buffer, sizeof(mbedtls_memory_buffer));
    void * buffer1 = sm_mbedtls_allocator_calloc(1,8);
    void * buffer2 = sm_mbedtls_allocator_calloc(1,8);
    sm_mbedtls_allocator_free(buffer2);
    buffer2 = sm_mbedtls_allocator_calloc(1,8);
    CHECK_EQUAL(8 + sizeof(void*) + 8 + sizeof(void*), offset_for_buffer(buffer2));
}

TEST(sm_mbedtls_allocator_test, alloc_alloc_alloc_free_free_free_1){
	sm_mbedtls_allocator_init(mbedtls_memory_buffer, sizeof(mbedtls_memory_buffer));
    void * buffer1 = sm_mbedtls_allocator_calloc(1,8);
    void * buffer2 = sm_mbedtls_allocator_calloc(2,8);
    void * buffer3 = sm_mbedtls_allocator_calloc(3,8);
    sm_mbedtls_allocator_free(buffer1);
    sm_mbedtls_allocator_free(buffer2);
    sm_mbedtls_allocator_free(buffer3);
	sm_allocator_node_t * anchor = (sm_allocator_node_t*) mbedtls_memory_buffer;
	sm_allocator_node_t * first = (sm_allocator_node_t*) &mbedtls_memory_buffer[8];
	CHECK_EQUAL(8, anchor->next);
	CHECK_EQUAL(0, anchor->size);
	CHECK_EQUAL(0, first->next);
	CHECK_EQUAL(sizeof(mbedtls_memory_buffer) - 8, first->size);
}

TEST(sm_mbedtls_allocator_test, alloc_alloc_alloc_free_alloc_1){
	sm_mbedtls_allocator_init(mbedtls_memory_buffer, sizeof(mbedtls_memory_buffer));
    void * buffer1 = sm_mbedtls_allocator_calloc(1,8);
    void * buffer2 = sm_mbedtls_allocator_calloc(2,8);
    void * buffer3 = sm_mbedtls_allocator_calloc(3,8);
    sm_mbedtls_allocator_free(buffer1);
    buffer1 = sm_mbedtls_allocator_calloc(1,8);
    CHECK_EQUAL(8 + sizeof(void*), offset_for_buffer(buffer1));
}

TEST(sm_mbedtls_allocator_test, alloc_alloc_alloc_free_alloc_2){
	sm_mbedtls_allocator_init(mbedtls_memory_buffer, sizeof(mbedtls_memory_buffer));
    void * buffer1 = sm_mbedtls_allocator_calloc(1,8);
    void * buffer2 = sm_mbedtls_allocator_calloc(2,8);
    void * buffer3 = sm_mbedtls_allocator_calloc(3,8);
    sm_mbedtls_allocator_free(buffer2);
    buffer2 = sm_mbedtls_allocator_calloc(1,8);
    CHECK_EQUAL(8 + 2 * sizeof(void*) + 8, offset_for_buffer(buffer2));
}

TEST(sm_mbedtls_allocator_test, alloc_alloc_alloc_free_alloc_3){
	sm_mbedtls_allocator_init(mbedtls_memory_buffer, sizeof(mbedtls_memory_buffer));
    void * buffer1 = sm_mbedtls_allocator_calloc(1,8);
    void * buffer2 = sm_mbedtls_allocator_calloc(2,8);
    void * buffer3 = sm_mbedtls_allocator_calloc(3,8);
    sm_mbedtls_allocator_free(buffer3);
    buffer3 = sm_mbedtls_allocator_calloc(1,8);
    CHECK_EQUAL(8 + 3 * sizeof(void*) + 8 + 16, offset_for_buffer(buffer3));
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}

