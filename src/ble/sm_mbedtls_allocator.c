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

#include "btstack_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "sm_mbedtls_allocator.h"
#include "btstack_util.h"
#include "btstack_debug.h" 

#ifdef ENABLE_LE_SECURE_CONNECTIONS
#ifdef HAVE_HCI_CONTROLLER_DHKEY_SUPPORT
#error "Support for DHKEY Support in HCI Controller not implemented yet. Please use software implementation" 
#else
#define USE_MBEDTLS_FOR_ECDH
#endif
#endif

#ifdef USE_MBEDTLS_FOR_ECDH
#include "mbedtls/config.h"

#ifndef HAVE_MALLOC
// #define DEBUG_ALLOCATIONS

size_t mbed_memory_allocated_current;
size_t mbed_memory_allocated_max;
size_t mbed_memory_space_max;
size_t mbed_memory_max;
size_t mbed_memory_smallest_buffer = 0xfffffff;
int    mbed_memory_num_allocations;

#define NUM_SIZES 150
int current_individual_allocation[NUM_SIZES];
int max_individual_allocation[NUM_SIZES];
int max_total_allocation[NUM_SIZES];

// customized allocator for use with BTstack's Security Manager
// assumptions:
// - allocations are multiple of 8
// - smallest allocation is 8

static uint32_t   sm_allocator_size;
static uint8_t  * sm_allocator_buffer;
static uint32_t   sm_allocator_used;

static sm_allocator_node_t * sm_mbedtls_node_for_offset(uint32_t offset){
    if (offset > sm_allocator_size){
        printf("Error! offset %u\n", offset);
    }
    return (sm_allocator_node_t *) (sm_allocator_buffer + offset);
}

static void dump_allocations(void){
    size_t overhead = mbed_memory_num_allocations * sizeof(void*); 
    printf("SM Per Block - Summary: Allocations %u. Current %lu (+ %zu = %zu used), Max %lu.\n", 
        mbed_memory_num_allocations, 
        mbed_memory_allocated_current, overhead, mbed_memory_allocated_current + overhead,
        mbed_memory_allocated_max);
    int i;
    int total = 0;
    printf("- current    : [ ");
    for (i=0;i<sizeof(current_individual_allocation) / sizeof(int);i++){
        printf("%02u ", current_individual_allocation[i]);
        total += current_individual_allocation[i] * i * 8;
        if (i == 16) {
            printf(" - ");
            i = 40;
        }
        if (i == 46) {
            printf(" - ");
            i = 140;
        }
    }
    printf(" = %u\n", total);
    printf("- current max: [ ");
    total = 0;
    for (i=0;i<sizeof(max_individual_allocation) / sizeof(int);i++){
        printf("%02u ", max_individual_allocation[i]);
        total += max_individual_allocation[i] * i * 8;
        if (i == 16) {
            printf(" - ");
            i = 40;
        }
        if (i == 46) {
            printf(" - ");
            i = 140;
        }
    }
    printf(" = %u\n", total);
    printf("- total   max: [ ");
    total = 0;
    for (i=0;i<sizeof(max_total_allocation) / sizeof(int);i++){
        printf("%02u ", max_total_allocation[i]);
        total += max_total_allocation[i] * i * 8;
        if (i == 16) {
            printf(" - ");
            i = 40;
        }
        if (i == 46) {
            printf(" - ");
            i = 140;
        }
    }
    printf(" = %u\n", total);
}

void sm_mbedtls_allocator_status(void){
#ifdef DEBUG_ALLOCATIONS
    uint32_t current_pos = 0;
    sm_allocator_node_t * current = sm_mbedtls_node_for_offset(current_pos);
    int i = 1;
    size_t bytes_free = 0;
    printf("SM Status:\n");
    while (1){
        printf("- SM ALLOC: Free %u: pos %u, size %u, next %u\n", i++, current_pos, current->size, current->next);
        bytes_free += current->size;
        current_pos = current->next;
        current = sm_mbedtls_node_for_offset(current_pos);
        if (current_pos == 0) break;
    }
    size_t overhead = mbed_memory_num_allocations * sizeof(void*); 
    printf("- Summary: Allocations %u. Current %lu (+ %zu = %zu used), Max %lu. Free %zu. Total: %zu\n", 
        mbed_memory_num_allocations, 
        mbed_memory_allocated_current, overhead, mbed_memory_allocated_current + overhead,
        mbed_memory_allocated_max, bytes_free, bytes_free + mbed_memory_allocated_current + overhead );

    dump_allocations();
#endif
}

static inline void sm_mbedtls_allocator_update_max(){
#if 1
    uint32_t current_pos = 0;
    sm_allocator_node_t * current = sm_mbedtls_node_for_offset(current_pos);
    while (1){
        if (current_pos + 8 > mbed_memory_space_max) {
            mbed_memory_space_max = current_pos + 8;
            printf("SM Alloc: space used %zu (%zu data + %u allocations)\n", mbed_memory_space_max, mbed_memory_allocated_current, mbed_memory_num_allocations);
        }
        current_pos = current->next;
        current = sm_mbedtls_node_for_offset(current_pos);
        if (current_pos == 0) break;
    }
#endif
}



void * sm_mbedtls_allocator_calloc(size_t count, size_t size){
    size_t num_bytes = count * size;
    size_t total = num_bytes + sizeof(void *);
    mbed_memory_allocated_current += num_bytes;
    mbed_memory_smallest_buffer = btstack_min(mbed_memory_smallest_buffer, num_bytes);
    mbed_memory_num_allocations++;

    // printf("SM Alloc %zu bytes\n", num_bytes);

    // if (num_bytes > 1000){
    //     printf("big alloc!\n");
    // }

    int index = num_bytes / 8;
    current_individual_allocation[index]++;
 
    if (current_individual_allocation[index] > max_individual_allocation[index]){
        max_individual_allocation[index] = current_individual_allocation[index];
        dump_allocations();
    }
    // mbed_memory_allocated_max   = btstack_max(mbed_memory_allocated_max, mbed_memory_allocated_current);
    if (mbed_memory_allocated_current > mbed_memory_allocated_max){
        memcpy(max_total_allocation, current_individual_allocation, sizeof(max_total_allocation));
        mbed_memory_allocated_max = mbed_memory_allocated_current;
        dump_allocations();
    }

    uint32_t prev_pos, current_pos, node_pos;
    sm_allocator_node_t * prev, * current, * node;

    // find exact block
    prev_pos = 0;
    prev    = sm_mbedtls_node_for_offset(prev_pos);
    current_pos = prev->next;
    while (current_pos){
        current = sm_mbedtls_node_for_offset(current_pos);
        if (current->size == total){
            prev->next = current->next;
            memset(current, 0, total);
            *(uint32_t*)current = total;
            sm_mbedtls_allocator_update_max();
#ifdef DEBUG_ALLOCATIONS
            printf("sm_mbedtls_allocator_calloc(%zu,%zu) = total %zu -> pos %u\n", count, size, total, current_pos);
#endif
            sm_mbedtls_allocator_status();
            return &sm_allocator_buffer[current_pos + sizeof(void *)];
        }
        // next
        prev = current;
        prev_pos = current_pos;
        current_pos = current->next;
    }
    // find first large enough block
    prev_pos = 0;
    prev    = sm_mbedtls_node_for_offset(prev_pos);
    current_pos = prev->next;
    while (current_pos){
        current = sm_mbedtls_node_for_offset(current_pos);
        if (current->size > total){
#ifdef DEBUG_ALLOCATIONS
            printf("sm_mbedtls_allocator_calloc splitting block at %u, size %u (prev %u)\n", current_pos, current->size, prev_pos);
#endif
            node_pos = current_pos + total;
            node = sm_mbedtls_node_for_offset(node_pos);
            node->next = current->next;
            node->size = current->size - total;
#ifdef DEBUG_ALLOCATIONS
            printf("sm_mbedtls_allocator_calloc new block at %u, size %u\n", node_pos, node->size);
#endif
            prev->next = node_pos;
            memset(current, 0, total);
            *(uint32_t*)current = total;
            sm_mbedtls_allocator_update_max();
#ifdef DEBUG_ALLOCATIONS
            printf("sm_mbedtls_allocator_calloc(%zu,%zu) = total %zu -> pos %u\n", count, size, total, current_pos);
#endif
            sm_mbedtls_allocator_status();
            return &sm_allocator_buffer[current_pos + sizeof(void *)];
        }
        // next
        prev = current;
        prev_pos = current_pos;
        current_pos = current->next;
    }

#ifdef DEBUG_ALLOCATIONS
    // failed to allocate
    printf("sm_mbedtls_allocator_calloc error, no free chunk found!\n");
    exit(10);
#else
    log_error("sm_mbedtls_allocator_calloc error, no free chunk found to allocate %zu bytes\n", total);
    return 0;
#endif
}

void sm_mbedtls_allocator_free(void * data){
    uint32_t prev_pos, current_pos, next_pos;
    sm_allocator_node_t * current, * prev, * next;
    current = (sm_allocator_node_t*) (((uint8_t *) data) - sizeof(void *));
    current_pos = ((uint8_t *) current) - ((uint8_t *) sm_allocator_buffer);
    size_t total = *(uint32_t*) current;
    size_t num_bytes = total - sizeof(void *);
    mbed_memory_allocated_current -= num_bytes;
    mbed_memory_num_allocations--;
  
    int index = num_bytes / 8;
    current_individual_allocation[index]--;

    // printf("SM Free %zu bytes\n", num_bytes);
 

#ifdef DEBUG_ALLOCATIONS
    printf("sm_mbedtls_allocator_free: pos %u, total %zu\n", current_pos, total);
#endif
    // find previous node
    prev_pos = 0;
    prev = sm_mbedtls_node_for_offset(prev_pos);
    while (prev->next < current_pos){
        prev_pos = prev->next;
        prev = sm_mbedtls_node_for_offset(prev_pos);
    }

    // setup new node
    current->next = prev->next;
    current->size = total;
    prev->next = current_pos;

    // merge with previous ?
#ifdef DEBUG_ALLOCATIONS
    printf("sm_mbedtls_allocator_free: prev %u, %u\n", prev_pos, prev->size);
#endif
    if (prev_pos + prev->size == current_pos){
#ifdef DEBUG_ALLOCATIONS
        printf("sm_mbedtls_allocator_free: merge with previous\n");
#endif
        prev->size += current->size;
        prev->next  = current->next;
        current = prev;
        current_pos = prev_pos;
    }

    // merge with next node?
    next_pos = current->next;
    if (current_pos + current->size == next_pos){
        next = sm_mbedtls_node_for_offset(next_pos);
#ifdef DEBUG_ALLOCATIONS
        printf("sm_mbedtls_allocator_free: merge with next at pos %u, size %u\n", next_pos, next->size);
#endif
        current->size += next->size;
        current->next  = next->next;
    }
    sm_mbedtls_allocator_status();
}

void sm_mbedtls_allocator_init(uint8_t * buffer, uint32_t size){
    sm_allocator_buffer = buffer;
    sm_allocator_size = size;
    sm_allocator_node_t * anchor = sm_mbedtls_node_for_offset(0);
    anchor->next = sizeof(sm_allocator_node_t);
    anchor->size = 0;
    sm_allocator_node_t * first = sm_mbedtls_node_for_offset(anchor->next);
    first->next = 0;
    first->size = size - sizeof(sm_allocator_node_t);
    sm_allocator_used = 2 * sizeof(sm_allocator_node_t);
    sm_mbedtls_allocator_status();
}

#if 0
void * sm_mbedtls_allocator_calloc(size_t count, size_t size){
    size_t total = count * size;
    mbed_memory_allocated_current += total;
    mbed_memory_allocated_max = btstack_max(mbed_memory_allocated_max, mbed_memory_allocated_current);
    mbed_memory_smallest_buffer = btstack_min(mbed_memory_smallest_buffer, total);
    mbed_memory_num_allocations++;
    void * result = calloc(4 + total, 1);
    *(uint32_t*) result = total;    
    printf("sm_mbedtls_allocator_calloc(%zu, %zu) -> res %p. Total %lu, Max %lu, Smallest %lu, Count %u\n", count, size, result, mbed_memory_allocated_current, mbed_memory_allocated_max, mbed_memory_smallest_buffer, mbed_memory_num_allocations);
    return ((uint8_t *) result) + 4;
}

void sm_mbedtls_allocator_free(void * data){
    void * orig = ((uint8_t *) data) - 4;
    size_t total = *(uint32_t *)orig;
    mbed_memory_allocated_current -= total;
    mbed_memory_num_allocations--;
    printf("sm_mbedtls_allocator_free(%p) - %zu bytes. Total %lu, Count %u\n", data, total, mbed_memory_allocated_current, mbed_memory_num_allocations);
    free(orig);
}
#endif
#endif
#endif
