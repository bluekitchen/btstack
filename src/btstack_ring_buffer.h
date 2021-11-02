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
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL BLUEKITCHEN
 * GMBH OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
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

/**
 * @title Ring Buffer
 *
 */

#ifndef BTSTACK_RING_BUFFER_H
#define BTSTACK_RING_BUFFER_H

#if defined __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct btstack_ring_buffer {
    uint8_t  * storage;
    uint32_t size;    
    uint32_t last_read_index;
    uint32_t last_written_index;
    uint8_t  full;
} btstack_ring_buffer_t;

/* API_START */

/**
 * Init ring buffer
 * @param ring_buffer object
 * @param storage
 * @param storage_size in bytes
 */
void btstack_ring_buffer_init(btstack_ring_buffer_t * ring_buffer, uint8_t * storage, uint32_t storage_size);

/**
 * Reset ring buffer to initial state (empty)
 * @param ring_buffer object
 */
void btstack_ring_buffer_reset(btstack_ring_buffer_t * ring_buffer);

/**
 * Check if ring buffer is empty
 * @param ring_buffer object
 * @return TRUE if empty
 */
int btstack_ring_buffer_empty(btstack_ring_buffer_t * ring_buffer);

/**
 * Get number of bytes available for read
 * @param ring_buffer object
 * @return number of bytes available for read
 */
uint32_t btstack_ring_buffer_bytes_available(btstack_ring_buffer_t * ring_buffer);

/**
 * Get free space available for write
 * @param ring_buffer object
 * @return number of bytes available for write
 */
uint32_t btstack_ring_buffer_bytes_free(btstack_ring_buffer_t * ring_buffer);

/**
 * Write bytes into ring buffer
 * @param ring_buffer object
 * @param data to store
 * @param data_length
 * @return 0 if ok, ERROR_CODE_MEMORY_CAPACITY_EXCEEDED if not enough space in buffer
 */
int btstack_ring_buffer_write(btstack_ring_buffer_t * ring_buffer, uint8_t * data, uint32_t data_length); 

/**
 * Read from ring buffer
 * @param ring_buffer object
 * @param buffer to store read data
 * @param length to read
 * @param number_of_bytes_read
 */
void btstack_ring_buffer_read(btstack_ring_buffer_t * ring_buffer, uint8_t * buffer, uint32_t length, uint32_t * number_of_bytes_read); 

/* API_END */

#if defined __cplusplus
}
#endif

#endif // BTSTACK_RING_BUFFER_H
