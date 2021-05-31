/*
 * Copyright (C) 2016 BlueKitchen GmbH
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

#define BTSTACK_FILE__ "btstack_ring_buffer.c"

/*
 *  btstack_ring_buffer.c
 *
 */

#include <string.h>

#include "btstack_ring_buffer.h"
#include "btstack_util.h"

#define ERROR_CODE_MEMORY_CAPACITY_EXCEEDED 0x07


// init ring buffer
void btstack_ring_buffer_init(btstack_ring_buffer_t * ring_buffer, uint8_t * storage, uint32_t storage_size){
    ring_buffer->storage = storage;
    ring_buffer->size = storage_size;
    btstack_ring_buffer_reset(ring_buffer);
}

void btstack_ring_buffer_reset(btstack_ring_buffer_t * ring_buffer){
    ring_buffer->last_read_index = 0;
    ring_buffer->last_written_index = 0;
    ring_buffer->full = 0;
}

uint32_t btstack_ring_buffer_bytes_available(btstack_ring_buffer_t * ring_buffer){
    if (ring_buffer->full) return ring_buffer->size;
    int diff = ring_buffer->last_written_index - ring_buffer->last_read_index;
    if (diff >= 0) return diff;
    return diff + ring_buffer->size;
}

// test if ring buffer is empty
int btstack_ring_buffer_empty(btstack_ring_buffer_t * ring_buffer){
    return btstack_ring_buffer_bytes_available(ring_buffer) == 0u;
}

// 
uint32_t btstack_ring_buffer_bytes_free(btstack_ring_buffer_t * ring_buffer){
    return ring_buffer->size - btstack_ring_buffer_bytes_available(ring_buffer);
}

// add byte block to ring buffer, 
int btstack_ring_buffer_write(btstack_ring_buffer_t * ring_buffer, uint8_t * data, uint32_t data_length){
    if (btstack_ring_buffer_bytes_free(ring_buffer) < data_length){
        return ERROR_CODE_MEMORY_CAPACITY_EXCEEDED;
    }

    // simplify logic below by asserting data_length > 0
    if (data_length == 0u) return 0u;

    // copy first chunk
    unsigned int bytes_until_end = ring_buffer->size - ring_buffer->last_written_index;
    unsigned int bytes_to_copy = btstack_min(bytes_until_end, data_length);
    (void)memcpy(&ring_buffer->storage[ring_buffer->last_written_index],
                 data, bytes_to_copy);
    data_length -= bytes_to_copy;
    data += bytes_to_copy;

    // update last written index
    ring_buffer->last_written_index += bytes_to_copy;
    if (ring_buffer->last_written_index == ring_buffer->size){
        ring_buffer->last_written_index = 0;
    }

    // copy second chunk
    if (data_length != 0) {
        (void)memcpy(&ring_buffer->storage[0], data, data_length);
        ring_buffer->last_written_index += data_length;
    }

    // mark buffer as full
    if (ring_buffer->last_written_index == ring_buffer->last_read_index){
        ring_buffer->full = 1;
    }
    return ERROR_CODE_SUCCESS;
} 

// fetch data_length bytes from ring buffer
void btstack_ring_buffer_read(btstack_ring_buffer_t * ring_buffer, uint8_t * data, uint32_t data_length, uint32_t * number_of_bytes_read){
    // limit data to get and report
    data_length = btstack_min(data_length, btstack_ring_buffer_bytes_available(ring_buffer));
    *number_of_bytes_read = data_length;

    // simplify logic below by asserting data_length > 0
    if (data_length == 0u) return;

    // copy first chunk
    unsigned int bytes_until_end = ring_buffer->size - ring_buffer->last_read_index;
    unsigned int bytes_to_copy = btstack_min(bytes_until_end, data_length);
    (void)memcpy(data, &ring_buffer->storage[ring_buffer->last_read_index],
                 bytes_to_copy);
    data_length -= bytes_to_copy;
    data += bytes_to_copy;

    // update last read index
    ring_buffer->last_read_index += bytes_to_copy;
    if (ring_buffer->last_read_index == ring_buffer->size){
        ring_buffer->last_read_index = 0;
    }

    // copy second chunk
    if (data_length != 0) {
        (void)memcpy(data, &ring_buffer->storage[0], data_length);
        ring_buffer->last_read_index += data_length;
    }

    // clear full flag
    ring_buffer->full = 0;
} 

