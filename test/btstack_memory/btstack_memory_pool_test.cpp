/*
 * Copyright (C) 2014 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */
 
// *****************************************************************************
//
// test rfcomm query tests
//
// *****************************************************************************


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTest/CommandLineTestRunner.h"

#include "bluetooth_data_types.h"
#include "btstack_util.h"
#include "btstack_memory_pool.h"

#define MAX_NUM_PDUS 20

typedef struct {
    btstack_linked_item_t  item;
    uint8_t value;
} test_pdu_t;

TEST_GROUP(MemoryPool){
    test_pdu_t pdu_storage[MAX_NUM_PDUS];
    btstack_memory_pool_t pdu_pool;

    void setup(void){
        int i;
        for (i = 0; i < MAX_NUM_PDUS; i++){
            pdu_storage[i].value = i;
        }
    }
};

TEST(MemoryPool, CreateAndGetZero){
    btstack_memory_pool_create(&pdu_pool, pdu_storage, 0, sizeof(test_pdu_t));
    test_pdu_t * node = (test_pdu_t *) btstack_memory_pool_get(&pdu_pool);
    CHECK(node == NULL);
}


TEST(MemoryPool, CreateThreeAndGetFour){
    btstack_memory_pool_create(&pdu_pool, pdu_storage, 3, sizeof(test_pdu_t));
    test_pdu_t * node;

    node = (test_pdu_t *) btstack_memory_pool_get(&pdu_pool);
    CHECK(node != NULL);
    CHECK(node->value == 2);

    node = (test_pdu_t *) btstack_memory_pool_get(&pdu_pool);
    CHECK(node != NULL);
    CHECK(node->value == 1);

    node = (test_pdu_t *) btstack_memory_pool_get(&pdu_pool);
    CHECK(node != NULL);
    CHECK(node->value == 0);

    node = (test_pdu_t *) btstack_memory_pool_get(&pdu_pool);
    CHECK(node == NULL);
}

TEST(MemoryPool, CreateAndFree){
    btstack_memory_pool_create(&pdu_pool, pdu_storage, 3, sizeof(test_pdu_t));
    
    test_pdu_t * next_node = (test_pdu_t *) btstack_memory_pool_get(&pdu_pool);
    CHECK_EQUAL(2, next_node->value);
    btstack_memory_pool_free(&pdu_pool, next_node);

    next_node = (test_pdu_t *) btstack_memory_pool_get(&pdu_pool);
    CHECK(next_node != NULL);
    CHECK_EQUAL(2, next_node->value);

    next_node = (test_pdu_t *) btstack_memory_pool_get(&pdu_pool);
    CHECK(next_node != NULL);
    CHECK_EQUAL(1, next_node->value);

    next_node = (test_pdu_t *) btstack_memory_pool_get(&pdu_pool);
    CHECK(next_node != NULL);
    CHECK_EQUAL(0, next_node->value);

    next_node = (test_pdu_t *) btstack_memory_pool_get(&pdu_pool);
    CHECK(next_node == NULL);
}

int main (int argc, const char * argv[]){
    return CommandLineTestRunner::RunAllTests(argc, argv);
}
