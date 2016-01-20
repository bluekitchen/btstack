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

/*
 *  btstack_memory_pool.c
 *
 *  Fixed-size block allocation
 *
 *  Free blocks are kept in singly linked list
 *
 */

#include "btstack_memory_pool.h"

#include <stddef.h>
#include "btstack_debug.h"

typedef struct node {
    struct node * next;
} node_t;

void btstack_memory_pool_create(btstack_memory_pool_t *pool, void * storage, int count, int block_size){
    node_t *free_blocks = (node_t*) pool;
    char   *mem_ptr = (char *) storage;
    int i;
    
    // create singly linked list of all available blocks
    free_blocks->next = NULL;
    for (i = 0 ; i < count ; i++){
        btstack_memory_pool_free(pool, mem_ptr);
        mem_ptr += block_size;
    }
}

void * btstack_memory_pool_get(btstack_memory_pool_t *pool){
    node_t *free_blocks = (node_t*) pool;
    
    if (!free_blocks->next) return NULL;
    
    // remove first
    node_t *node      = free_blocks->next;
    free_blocks->next = node->next;
    
    return (void*) node;
}

void btstack_memory_pool_free(btstack_memory_pool_t *pool, void * block){
    node_t *free_blocks = (node_t*) pool;
    node_t *node        = (node_t*) block;

    // raise error and abort if node already in list
    node_t * it;
    for (it = free_blocks->next; it ; it = it->next){
        if (it == node) {
            log_error("btstack_memory_pool_free: block %p freed twice for pool %p", block, pool);
            return;
        }
    }

    // add block as node to list
    node->next          = free_blocks->next;
    free_blocks->next   = node;
}
